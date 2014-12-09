/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/wsck-transport.h>

#include <murphy/resolver/resolver.h>
#include <murphy/core/domain.h>

#include "proxy.h"
#include "message.h"
#include "table.h"
#include "notify.h"
#include "domain-control.h"

static mrp_transport_t *create_transport(pdp_t *pdp, const char *address);
static void destroy_transport(mrp_transport_t *t);

static int invoke_handler(void *handler_data, const char *id,
                          const char *method, int narg,
                          mrp_domctl_arg_t *args,
                          mrp_domain_return_cb_t return_cb,
                          void *user_data);

static uint32_t RESEVT_START, RESEVT_DONE, RESEVT_FAIL;


static void resolver_event_cb(mrp_event_watch_t *w, uint32_t id, int format,
                              void  *data, void *user_data)
{
    pdp_t *pdp = (pdp_t *)user_data;

    MRP_UNUSED(w);
    MRP_UNUSED(format);
    MRP_UNUSED(data);

    if (id == RESEVT_START)
        pdp->ractive++;
    else if (id == RESEVT_DONE || id == RESEVT_FAIL)
        pdp->ractive--;

    mrp_debug("resolver is %s active", pdp->ractive == 1 ? "now" :
              pdp->ractive > 1 ? "still" : "no longer");

    if (pdp->ractive == 0) {
        schedule_notification(pdp);
        pdp->rblocked = false;
    }
}


static int add_resolver_trigger(pdp_t *pdp)
{
    mrp_context_t   *ctx = pdp->ctx;
    mrp_mainloop_t  *ml  = ctx->ml;
    mrp_event_bus_t *bus = mrp_event_bus_get(ml, MRP_RESOLVER_BUS);
    mrp_event_mask_t mask;

    if (bus == NULL)
        return FALSE;

    RESEVT_START = mrp_event_id(MRP_RESOLVER_EVENT_STARTED);
    RESEVT_DONE  = mrp_event_id(MRP_RESOLVER_EVENT_DONE);
    RESEVT_FAIL  = mrp_event_id(MRP_RESOLVER_EVENT_FAILED);

    mrp_mask_init(&mask);
    if (!mrp_mask_set(&mask, RESEVT_START) ||
        !mrp_mask_set(&mask, RESEVT_DONE ) ||
        !mrp_mask_set(&mask, RESEVT_FAIL ))
        return FALSE;

    pdp->reh = mrp_event_add_watch_mask(bus, &mask, resolver_event_cb, pdp);

    return pdp->reh != NULL;
}


static void del_resolver_trigger(pdp_t *pdp)
{
    mrp_event_del_watch(pdp->reh);
    pdp->reh = NULL;
}


pdp_t *create_domain_control(mrp_context_t *ctx,
                             const char *extaddr, const char *intaddr,
                             const char *wrtaddr, const char *httpdir)
{
    pdp_t *pdp;

    pdp = mrp_allocz(sizeof(*pdp));

    if (pdp != NULL) {
        pdp->ctx     = ctx;
        pdp->address = extaddr;

        if (init_proxies(pdp) && init_tables(pdp)) {

            if (!add_resolver_trigger(pdp))
                goto fail;

            if (extaddr && *extaddr)
                pdp->extt = create_transport(pdp, extaddr);

            if (intaddr && *intaddr)
                pdp->intt = create_transport(pdp, intaddr);

            if (wrtaddr && *wrtaddr) {
                pdp->wrtt = create_transport(pdp, wrtaddr);

                if (pdp->wrtt != NULL) {
                    const char *sm_opt = MRP_WSCK_OPT_SENDMODE;
                    const char *sm_val = MRP_WSCK_SENDMODE_TEXT;
                    const char *hd_opt = MRP_WSCK_OPT_HTTPDIR;
                    const char *hd_val = httpdir;

                    mrp_transport_setopt(pdp->wrtt, sm_opt, sm_val);
                    mrp_transport_setopt(pdp->wrtt, hd_opt, hd_val);
                }
            }


            if ((!extaddr || !*extaddr || pdp->extt != NULL) &&
                (!intaddr || !*intaddr || pdp->intt != NULL) &&
                (!wrtaddr || !*wrtaddr || pdp->wrtt != NULL)) {
                mrp_set_domain_invoke_handler(ctx, invoke_handler, pdp);
                return pdp;
            }
        }

    fail:
        destroy_domain_control(pdp);
    }

    return NULL;
}


void destroy_domain_control(pdp_t *pdp)
{
    if (pdp != NULL) {
        del_resolver_trigger(pdp);
        destroy_proxies(pdp);
        destroy_tables(pdp);
        destroy_transport(pdp->extt);
        destroy_transport(pdp->intt);
        destroy_transport(pdp->wrtt);

        mrp_free(pdp);
    }
}


static void notify_cb(mrp_deferred_t *d, void *user_data)
{
    pdp_t *pdp = (pdp_t *)user_data;

    mrp_disable_deferred(d);
    pdp->notify_scheduled = false;
    notify_table_changes(pdp);
}


void schedule_notification(pdp_t *pdp)
{

    if (pdp->notify == NULL)
        pdp->notify = mrp_add_deferred(pdp->ctx->ml, notify_cb, pdp);

    if (!pdp->notify_scheduled) {
        mrp_debug("scheduling client notification");
        mrp_enable_deferred(pdp->notify);
        pdp->notify_scheduled = true;
    }
}


static int msg_send_message(pep_proxy_t *proxy, msg_t *msg)
{
    mrp_msg_t *tmsg;

    tmsg = msg_encode_message(msg);

    if (tmsg != NULL) {
        mrp_transport_send(proxy->t, tmsg);
        mrp_msg_unref(tmsg);

        return TRUE;
    }
    else
        return FALSE;
}


static int msg_send_ack(pep_proxy_t *proxy, uint32_t seq)
{
    ack_msg_t ack;

    mrp_clear(&ack);
    ack.type = MSG_TYPE_ACK;
    ack.seq  = seq;

    return proxy->ops->send_msg(proxy, (msg_t *)&ack);
}


static int msg_send_nak(pep_proxy_t *proxy, uint32_t seq,
                        int32_t error, const char *msg)
{
    nak_msg_t nak;

    mrp_clear(&nak);
    nak.type   =  MSG_TYPE_NAK;
    nak.seq    = seq;
    nak.error  = error;
    nak.msg    = msg;

    return proxy->ops->send_msg(proxy, (msg_t *)&nak);
}



static void process_register(pep_proxy_t *proxy, register_msg_t *reg)
{
    int         error;
    const char *errmsg;

    if (register_proxy(proxy, reg->name, reg->tables, reg->ntable,
                       reg->watches, reg->nwatch, &error, &errmsg)) {
        msg_send_ack(proxy, reg->seq);
        schedule_notification(proxy->pdp);
    }
    else
        msg_send_nak(proxy, reg->seq, error, errmsg);
}


static void process_unregister(pep_proxy_t *proxy, unregister_msg_t *unreg)
{
    msg_send_ack(proxy, unreg->seq);
}


static void process_set(pep_proxy_t *proxy, set_msg_t *set)
{
    int         error;
    const char *errmsg;

    if (set_proxy_tables(proxy, set->tables, set->ntable, &error, &errmsg)) {
        msg_send_ack(proxy, set->seq);
    }
    else
        msg_send_nak(proxy, set->seq, error, errmsg);
}


static void process_invoke(pep_proxy_t *proxy, invoke_msg_t *invoke)
{
    mrp_context_t          *ctx = proxy->pdp->ctx;
    mrp_domain_invoke_cb_t  cb;
    void                   *user_data;
    int                     max_out;
    mrp_domctl_arg_t       *args;
    int                     narg;
    return_msg_t            ret;
    mrp_msg_t              *msg;
    int                     i;

    mrp_clear(&ret);

    ret.type = MSG_TYPE_RETURN;
    ret.seq  = invoke->seq;
    args     = NULL;
    narg     = 0;

    if (!mrp_lookup_domain_method(ctx, invoke->name, &cb, &max_out,&user_data)) {
        ret.error = MRP_DOMCTL_NOTFOUND;
    }
    else {
        ret.error = MRP_DOMCTL_OK;

        narg = ret.narg = max_out;

        if (narg > 0) {
            args = ret.args = alloca(narg * sizeof(args[0]));
            memset(args, 0, narg * sizeof(args[0]));
        }

        ret.retval = cb(invoke->narg, invoke->args,
                        &ret.narg, ret.args, user_data);
    }

    msg = msg_encode_message((msg_t *)&ret);

    if (msg != NULL) {
        mrp_transport_send(proxy->t, msg);
        mrp_msg_unref(msg);
    }

    narg = ret.narg;
    for (i = 0; i < narg; i++) {
        if (args[i].type == MRP_DOMCTL_STRING)
            mrp_free((char *)args[i].str);
        else if (MRP_DOMCTL_IS_ARRAY(args[i].type)) {
            uint32_t j;

            for (j = 0; j < args[i].size; j++)
                if (MRP_DOMCTL_ARRAY_TYPE(args[i].type) == MRP_DOMCTL_STRING)
                    mrp_free(((char **)args[i].arr)[j]);

            mrp_free(args[i].arr);
        }
    }
}


static void process_return(pep_proxy_t *proxy, return_msg_t *ret)
{
    uint32_t                id = ret->seq;
    mrp_domain_return_cb_t  cb;
    void                   *user_data;

    if (!proxy_dequeue_pending(proxy, id, &cb, &user_data))
        return;

    cb(ret->error, ret->retval, ret->narg, ret->args, user_data);
}


static void process_message(pep_proxy_t *proxy, msg_t *msg)
{
    char *name  = proxy->name ? proxy->name : "<unknown>";

    switch (msg->any.type) {
    case MSG_TYPE_REGISTER:
        process_register(proxy, &msg->reg);
        break;
    case MSG_TYPE_UNREGISTER:
        process_unregister(proxy, &msg->unreg);
        break;
    case MSG_TYPE_SET:
        process_set(proxy, &msg->set);
        break;
    case MSG_TYPE_INVOKE:
        process_invoke(proxy, &msg->invoke);
        break;
    case MSG_TYPE_RETURN:
        process_return(proxy, &msg->ret);
        break;
    default:
        mrp_log_error("Unexpected message 0x%x from client %s.",
                      msg->any.type, name);
        break;
    }
}


static int invoke_handler(void *handler_data, const char *domain,
                          const char *method, int narg,
                          mrp_domctl_arg_t *args,
                          mrp_domain_return_cb_t return_cb,
                          void *user_data)
{
    pdp_t        *pdp   = (pdp_t *)handler_data;
    pep_proxy_t  *proxy = find_proxy(pdp, domain);
    uint32_t      id;
    invoke_msg_t  invoke;

    if (proxy == NULL)
        return FALSE;

    id = proxy_queue_pending(proxy, return_cb, user_data);

    if (!id)
        return FALSE;

    mrp_clear(&invoke);

    invoke.type  = MSG_TYPE_INVOKE;
    invoke.seq   = id;
    invoke.name  = method;
    invoke.noret = (return_cb == NULL);
    invoke.narg  = narg;
    invoke.args  = args;

    return msg_send_message(proxy, (msg_t *)&invoke);
}


static int msg_op_send_msg(pep_proxy_t *proxy, msg_t *msg)
{
    return msg_send_message(proxy, msg);
}


static void msg_op_unref_msg(void *msg)
{
    mrp_msg_unref((mrp_msg_t *)msg);
}


static int msg_op_create_notify(pep_proxy_t *proxy)
{
    if (proxy->notify_msg == NULL)
        proxy->notify_msg = msg_create_notify();

    if (proxy->notify_msg != NULL)
        return TRUE;
    else
        return FALSE;
}


static int msg_op_update_notify(pep_proxy_t *proxy, int tblid, mql_result_t *r)
{
    int n;

    n = msg_update_notify((mrp_msg_t *)proxy->notify_msg, tblid, r);

    if (n >= 0) {
        proxy->notify_ncolumn += n;
        proxy->notify_ntable++;
    }

    return n;
}


static int msg_op_send_notify(pep_proxy_t *proxy)
{
    mrp_msg_t *msg     = proxy->notify_msg;
    uint16_t   nchange = proxy->notify_ntable;
    uint16_t   ntotal  = proxy->notify_ncolumn;

    mrp_msg_set(msg, MSG_UINT16(NCHANGE, nchange));
    mrp_msg_set(msg, MSG_UINT16(NTOTAL , ntotal));

    return mrp_transport_send(proxy->t, msg);
}


static void msg_op_free_notify(pep_proxy_t *proxy)
{
    mrp_msg_unref((mrp_msg_t *)proxy->notify_msg);
    proxy->notify_msg = NULL;
}


static void msg_connect_cb(mrp_transport_t *t, void *user_data)
{
    static proxy_ops_t ops = {
        .send_msg      = msg_op_send_msg,
        .unref         = msg_op_unref_msg,
        .create_notify = msg_op_create_notify,
        .update_notify = msg_op_update_notify,
        .send_notify   = msg_op_send_notify,
        .free_notify   = msg_op_free_notify,
    };

    pdp_t       *pdp = (pdp_t *)user_data;
    pep_proxy_t *proxy;
    int          flags;

    proxy = create_proxy(pdp);

    if (proxy != NULL) {
        flags    = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        proxy->t = mrp_transport_accept(t, proxy, flags);

        if (proxy->t != NULL) {
            proxy->ops = &ops;
            mrp_log_info("Accepted new client connection.");
        }
        else {
            mrp_log_error("Failed to accept new client connection.");
            destroy_proxy(proxy);
        }
    }
}


static void msg_closed_cb(mrp_transport_t *t, int error, void *user_data)
{
    pep_proxy_t *proxy = (pep_proxy_t *)user_data;
    char        *name  = proxy && proxy->name ? proxy->name : "<unknown>";

    MRP_UNUSED(t);

    if (error)
        mrp_log_error("Transport to client %s closed (%d: %s).",
                      name, error, strerror(error));
    else
        mrp_log_info("Transport to client %s closed.", name);

    mrp_log_info("Destroying client %s.", name);
    destroy_proxy(proxy);
}


static void msg_recv_cb(mrp_transport_t *t, mrp_msg_t *tmsg, void *user_data)
{
    pep_proxy_t *proxy = (pep_proxy_t *)user_data;
    char        *name;
    msg_t       *msg;
    uint32_t     seqno;

    MRP_UNUSED(t);

    /*
      mrp_log_info("Message from client %p:", proxy);
      mrp_msg_dump(msg, stdout);
    */

    if (proxy != NULL) {
        name = proxy->name ? proxy->name : "<unknown>";
        msg  = msg_decode_message(tmsg);

        if (msg != NULL) {
            process_message(proxy, msg);
            msg_free_message(msg);
        }
        else {
            if (!mrp_msg_get(tmsg, MSG_UINT32(MSGSEQ, &seqno), MSG_END))
                seqno = 0;
            mrp_log_error("Failed to decode message from %s.", name);
            msg_send_nak(proxy, seqno, 1, "failed to decode message");
        }
    }
}


static int wrt_send_message(pep_proxy_t *proxy, msg_t *msg)
{
    mrp_json_t *tmsg;

    tmsg = json_encode_message(msg);

    if (tmsg != NULL) {
        mrp_transport_sendcustom(proxy->t, tmsg);
        mrp_json_unref(tmsg);

        return TRUE;
    }
    else
        return FALSE;
}


static int wrt_op_send_msg(pep_proxy_t *proxy, msg_t *msg)
{
    return wrt_send_message(proxy, msg);
}


static void wrt_op_unref_msg(void *msg)
{
    mrp_json_unref((mrp_json_t *)msg);
}


static int wrt_op_create_notify(pep_proxy_t *proxy)
{
    if (proxy->notify_msg == NULL)
        proxy->notify_msg = json_create_notify();

    if (proxy->notify_msg != NULL)
        return TRUE;
    else
        return FALSE;
}


static int wrt_op_update_notify(pep_proxy_t *proxy, int tblid, mql_result_t *r)
{
    int n;

    n = json_update_notify((mrp_json_t *)proxy->notify_msg, tblid, r);

    if (n >= 0) {
        proxy->notify_ncolumn += n;
        proxy->notify_ntable++;
    }

    return n;
}


static int wrt_op_send_notify(pep_proxy_t *proxy)
{
    mrp_json_t *msg     = proxy->notify_msg;
    int         nchange = proxy->notify_ntable;
    int         ntotal  = proxy->notify_ncolumn;

    if (mrp_json_add_integer(msg, "nchange", nchange) &&
        mrp_json_add_integer(msg, "ntotal" , ntotal))
        return mrp_transport_sendcustom(proxy->t, msg);
    else
        return FALSE;
}


static void wrt_op_free_notify(pep_proxy_t *proxy)
{
    mrp_json_unref((mrp_json_t *)proxy->notify_msg);
    proxy->notify_msg = NULL;
}


static void wrt_connect_cb(mrp_transport_t *t, void *user_data)
{
    static proxy_ops_t ops = {
        .send_msg      = wrt_op_send_msg,
        .unref         = wrt_op_unref_msg,
        .create_notify = wrt_op_create_notify,
        .update_notify = wrt_op_update_notify,
        .send_notify   = wrt_op_send_notify,
        .free_notify   = wrt_op_free_notify,
    };

    pdp_t       *pdp = (pdp_t *)user_data;
    pep_proxy_t *proxy;
    int          flags;

    proxy = create_proxy(pdp);

    if (proxy != NULL) {
        flags    = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        proxy->t = mrp_transport_accept(t, proxy, flags);

        if (proxy->t != NULL) {
            proxy->ops = &ops;
            mrp_log_info("Accepted new client connection.");
        }
        else {
            mrp_log_error("Failed to accept new client connection.");
            destroy_proxy(proxy);
        }
    }
}


static void wrt_closed_cb(mrp_transport_t *t, int error, void *user_data)
{
    pep_proxy_t *proxy = (pep_proxy_t *)user_data;
    char        *name  = proxy && proxy->name ? proxy->name : "<unknown>";

    MRP_UNUSED(t);

    if (error)
        mrp_log_error("Transport to client %s closed (%d: %s).",
                      name, error, strerror(error));
    else
        mrp_log_info("Transport to client %s closed.", name);

    mrp_log_info("Destroying client %s.", name);
    destroy_proxy(proxy);
}


static void wrt_recv_cb(mrp_transport_t *t, void *data, void *user_data)
{
    pep_proxy_t *proxy = (pep_proxy_t *)user_data;
    char        *name;
    msg_t       *msg;
    int          seqno;

    MRP_UNUSED(t);

    /*
      mrp_log_info("Message from WRT client %p:", proxy);
    */

    if (proxy != NULL) {
        name = proxy->name ? proxy->name : "<unknown>";
        msg  = json_decode_message(data);

        if (msg != NULL) {
            process_message(proxy, msg);
            msg_free_message(msg);
        }
        else {
            if (!mrp_json_get_integer(data, "seq", &seqno))
                seqno = 0;
            mrp_log_error("Failed to decode message from %s.", name);
            msg_send_nak(proxy, seqno, 1, "failed to decode message");
        }
    }
}




static mrp_transport_t *create_transport(pdp_t *pdp, const char *address)
{
    static mrp_transport_evt_t msg_evt, wrt_evt;

    mrp_transport_evt_t *e;
    mrp_transport_t     *t;
    mrp_sockaddr_t       addr;
    socklen_t            alen;
    int                  flags;
    const char          *type;

    t    = NULL;
    alen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr), &type);

    if (alen <= 0) {
        mrp_log_error("Failed to resolve transport address '%s'.", address);
        return NULL;
    }

    flags = MRP_TRANSPORT_REUSEADDR;

    if (strncmp(address, "wsck", 4) != 0) {
        e = &msg_evt;

        e->connection  = msg_connect_cb;
        e->closed      = msg_closed_cb;
        e->recvmsg     = msg_recv_cb;
        e->recvmsgfrom = NULL;
    }
    else {
        e = &wrt_evt;

        e->connection     = wrt_connect_cb;
        e->closed         = wrt_closed_cb;
        e->recvcustom     = wrt_recv_cb;
        e->recvcustomfrom = NULL;

        flags |= MRP_TRANSPORT_MODE_CUSTOM;
    }

    t = mrp_transport_create(pdp->ctx->ml, type, e, pdp, flags);

    if (t != NULL) {
        if (mrp_transport_bind(t, &addr, alen) && mrp_transport_listen(t, 4))
            return t;
        else {
            mrp_log_error("Failed to bind to transport address '%s'.", address);
            mrp_transport_destroy(t);
        }
    }
    else
        mrp_log_error("Failed to create transport '%s'.", address);

    return NULL;
}


static void destroy_transport(mrp_transport_t *t)
{
    mrp_transport_destroy(t);
}
