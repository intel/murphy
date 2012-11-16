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

#include "proxy.h"
#include "table.h"
#include "message.h"
#include "notify.h"
#include "domain-control.h"

static int create_transports(pdp_t *pdp);
static void destroy_transports(pdp_t *pdp);

pdp_t *create_domain_control(mrp_context_t *ctx, const char *address)
{
    pdp_t *pdp;

    pdp = mrp_allocz(sizeof(*pdp));

    if (pdp != NULL) {
        pdp->ctx     = ctx;
        pdp->address = address;

        if (init_proxies(pdp) && init_tables(pdp) && create_transports(pdp))
            return pdp;
        else
            destroy_domain_control(pdp);
    }

    return NULL;
}


void destroy_domain_control(pdp_t *pdp)
{
    if (pdp != NULL) {
        destroy_proxies(pdp);
        destroy_tables(pdp);
        destroy_transports(pdp);

        mrp_free(pdp);
    }
}


static void notify_cb(mrp_mainloop_t *ml, mrp_deferred_t *d, void *user_data)
{
    pdp_t *pdp = (pdp_t *)user_data;

    MRP_UNUSED(ml);

    mrp_disable_deferred(d);
    pdp->notify_scheduled = FALSE;
    notify_table_changes(pdp);
}


void schedule_notification(pdp_t *pdp)
{

    if (pdp->notify == NULL)
        pdp->notify = mrp_add_deferred(pdp->ctx->ml, notify_cb, pdp);

    if (!pdp->notify_scheduled) {
        mrp_debug("scheduling client notification");
        mrp_enable_deferred(pdp->notify);
    }
}


static void send_ack_reply(mrp_transport_t *t, uint32_t seq)
{
    mrp_msg_t *msg;

    msg = create_ack_message(seq);

    if (msg != NULL) {
        mrp_transport_send(t, msg);
        mrp_msg_unref(msg);
    }
}


static void send_nak_reply(mrp_transport_t *t, uint32_t seq, int error,
                           const char *errmsg)
{
    mrp_msg_t *msg;

    msg = create_nak_message(seq, error, errmsg);

    if (msg != NULL) {
        mrp_transport_send(t, msg);
        mrp_msg_unref(msg);
    }
}


static int process_register_request(pep_proxy_t *proxy, mrp_msg_t *req,
                                    uint32_t seq)
{
    mrp_transport_t    *t = proxy->t;
    char               *name;
    mrp_domctl_table_t *tables;
    mrp_domctl_watch_t *watches;
    uint16_t            utable, uwatch;
    int                 ntable, nwatch;
    int                 error;
    const char         *errmsg;

    if (mrp_msg_get(req,
                    MRP_PEPMSG_STRING(NAME   , &name  ),
                    MRP_PEPMSG_UINT16(NTABLE , &utable),
                    MRP_PEPMSG_UINT16(NWATCH , &uwatch),
                    MRP_MSG_END)) {
        ntable  = utable;
        nwatch  = uwatch;
        tables  = alloca(ntable * sizeof(*tables));
        watches = alloca(nwatch * sizeof(*watches));

        if (decode_register_message(req, tables, ntable, watches, nwatch)) {
            if (register_proxy(proxy, name, tables, ntable, watches, nwatch,
                               &error, &errmsg)) {
                send_ack_reply(t, seq);
                proxy->notify_all = TRUE;
                schedule_notification(proxy->pdp);

                return TRUE;
            }
        }
        else
            goto malformed;
    }
    else {
    malformed:
        error  = EINVAL;
        errmsg = "malformed register message";
    }

    send_nak_reply(t, seq, error, errmsg);

    return FALSE;
}


static void process_unregister_request(pep_proxy_t *proxy, uint32_t seq)
{
    send_ack_reply(proxy->t, seq);
    unregister_proxy(proxy);
}


static void process_set_request(pep_proxy_t *proxy, mrp_msg_t *msg,
                                uint32_t seq)
{
    mrp_domctl_data_t  *data, *d;
    mrp_domctl_value_t *values, *v;
    void               *it;
    uint16_t            ntable, ntotal, nrow, ncol;
    uint16_t            tblid;
    int                 t, r, c;
    uint16_t            type;
    mrp_msg_value_t     value;
    int                 error;
    const char         *errmsg;

    if (!mrp_msg_get(msg,
                     MRP_PEPMSG_UINT16(NCHANGE, &ntable),
                     MRP_PEPMSG_UINT16(NTOTAL , &ntotal),
                     MRP_MSG_END))
        return;

    data   = alloca(sizeof(*data)   * ntable);
    values = alloca(sizeof(*values) * ntotal);

    it     = NULL;
    d      = data;
    v      = values;

    for (t = 0; t < ntable; t++) {
        if (!mrp_msg_iterate_get(msg, &it,
                                 MRP_PEPMSG_UINT16(TBLID, &tblid),
                                 MRP_PEPMSG_UINT16(NROW , &nrow ),
                                 MRP_PEPMSG_UINT16(NCOL , &ncol ),
                                 MRP_MSG_END))
            goto reply_nak;

        if (tblid >= proxy->ntable)
            goto reply_nak;

        d->id      = tblid;
        d->ncolumn = ncol;
        d->nrow    = nrow;
        d->rows    = alloca(sizeof(*d->rows) * nrow);

        for (r = 0; r < nrow; r++) {
            d->rows[r] = v;

            for (c = 0; c < ncol; c++) {
                if (!mrp_msg_iterate_get(msg, &it,
                                         MRP_PEPMSG_ANY(DATA, &type, &value),
                                         MRP_MSG_END))
                    goto reply_nak;

                switch (type) {
                case MRP_MSG_FIELD_STRING:
                    v->type = MRP_DOMCTL_STRING;
                    v->str  = value.str;
                    break;
                case MRP_MSG_FIELD_SINT32:
                    v->type = MRP_DOMCTL_INTEGER;
                    v->s32  = value.s32;
                    break;
                case MRP_MSG_FIELD_UINT32:
                    v->type = MRP_DOMCTL_UNSIGNED;
                    v->u32  = value.u32;
                    break;
                case MRP_MSG_FIELD_DOUBLE:
                    v->type = MRP_DOMCTL_DOUBLE;
                    v->dbl  = value.dbl;
                    break;
                default:
                    goto reply_nak;
                }

                v++;
            }
        }

        d++;
    }

    if (set_proxy_tables(proxy, data, ntable, &error, &errmsg)) {
        send_ack_reply(proxy->t, seq);
    }
    else {
    reply_nak:
        send_nak_reply(proxy->t, seq, error, errmsg);
    }
}


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    pep_proxy_t *proxy = (pep_proxy_t *)user_data;
    char        *name  = proxy && proxy->name ? proxy->name : "<unknown>";
    uint16_t     type;
    uint32_t     seq;

    /*
      mrp_log_info("Message from client %p:", proxy);
      mrp_msg_dump(msg, stdout);
    */

    if (!mrp_msg_get(msg,
                     MRP_PEPMSG_UINT16(MSGTYPE, &type),
                     MRP_PEPMSG_UINT32(MSGSEQ , &seq ),
                     MRP_MSG_END)) {
        mrp_log_error("Malformed message from client %s.", name);
        send_nak_reply(t, 0, EINVAL, "malformed message");
    }
    else {
        switch (type) {
        case MRP_PEPMSG_REGISTER:
            if (!process_register_request(proxy, msg, seq))
                destroy_proxy(proxy);
            break;

        case MRP_PEPMSG_UNREGISTER:
            process_unregister_request(proxy, seq);
            break;

        case MRP_PEPMSG_SET:
            process_set_request(proxy, msg, seq);
            break;

        default:
            break;
        }
    }
}


static void connect_cb(mrp_transport_t *ext, void *user_data)
{
    pdp_t       *pdp = (pdp_t *)user_data;
    pep_proxy_t *proxy;
    int          flags;

    proxy = create_proxy(pdp);

    if (proxy != NULL) {
        flags    = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        proxy->t = mrp_transport_accept(ext, proxy, flags);

        if (proxy->t != NULL)
            mrp_log_info("Accepted new client connection.");
        else {
            mrp_log_error("Failed to accept new client connection.");
            destroy_proxy(proxy);
        }
    }
}


static void closed_cb(mrp_transport_t *t, int error, void *user_data)
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


static int create_ext_transport(pdp_t *pdp)
{
    static mrp_transport_evt_t evt = {
        .closed      = closed_cb,
        .recvmsg     = recv_cb,
        .recvmsgfrom = NULL,
        .connection  = connect_cb,
    };

    mrp_transport_t *t;
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    int              flags;
    const char      *type;

    t       = NULL;
    addrlen = mrp_transport_resolve(NULL, pdp->address,
                                    &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        flags = MRP_TRANSPORT_REUSEADDR;
        t     = mrp_transport_create(pdp->ctx->ml, type, &evt, pdp, flags);

        if (t != NULL) {
            if (mrp_transport_bind(t, &addr, addrlen) &&
                mrp_transport_listen(t, 4)) {
                mrp_log_info("Listening on transport %s...", pdp->address);
                pdp->ext = t;

                return TRUE;
            }
            else
                mrp_log_error("Failed to bind transport to %s.", pdp->address);
        }
        else
            mrp_log_error("Failed to create transport for %s.", pdp->address);
    }
    else
        mrp_log_error("Invalid transport address %s.", pdp->address);

    return FALSE;
}


static void destroy_ext_transport(pdp_t *pdp)
{
    if (pdp != NULL) {
        mrp_transport_destroy(pdp->ext);
        pdp->ext = NULL;
    }
}


static int create_transports(pdp_t *pdp)
{
    return create_ext_transport(pdp);
}


static void destroy_transports(pdp_t *pdp)
{
    destroy_ext_transport(pdp);
}
