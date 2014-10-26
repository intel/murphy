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
#include <alloca.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/transport.h>

#include "domain-control-types.h"
#include "message.h"
#include "table.h"
#include "client.h"


/*
 * mark an enforcement point busy (typically while executing a callback)
 */

#define DOMCTL_MARK_BUSY(dc, ...) do {             \
        (dc)->busy++;                              \
        __VA_ARGS__                                \
        (dc)->busy--;                              \
        check_destroyed(dc);                       \
    } while (0)


/*
 * a pending request
 */

typedef struct {
    mrp_list_hook_t         hook;        /* hook to pending request queue */
    uint32_t                seqno;       /* sequence number/request id */
    int                     invoke : 1;  /* whether a pending invocation */
    union {
        mrp_domctl_status_cb_t  status;  /* request completion callback */
        mrp_domctl_return_cb_t  ret;     /* invocation return cb */
    } cb;
    void                   *user_data;   /* opaque callback data */
} pending_request_t;


/*
 * a registered proxied method
 */

typedef struct {
    mrp_list_hook_t         hook;        /* to list of methods */
    char                   *name;        /* method name */
    size_t                  max_out;     /* max return arguments */
    mrp_domctl_invoke_cb_t  cb;          /* handler callback */
    void                   *user_data;   /* opaque handler data */
} method_t;

static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);
static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t addrlen,
                        void *user_data);
static void closed_cb(mrp_transport_t *t, int error, void *user_data);


static int queue_pending(mrp_domctl_t *dc, uint32_t seq,
                         mrp_domctl_status_cb_t cb, void *user_data);
static int notify_pending(mrp_domctl_t *dc, msg_t *msg);
static int queue_invoke(mrp_domctl_t *dc, uint32_t seq,
                        mrp_domctl_return_cb_t cb, void *user_data);
static void purge_pending(mrp_domctl_t *dc);




mrp_domctl_t *mrp_domctl_create(const char *name, mrp_mainloop_t *ml,
                                mrp_domctl_table_t *tables, int ntable,
                                mrp_domctl_watch_t *watches, int nwatch,
                                mrp_domctl_connect_cb_t connect_cb,
                                mrp_domctl_watch_cb_t watch_cb, void *user_data)
{
    mrp_domctl_t       *dc;
    mrp_domctl_table_t *st, *dt;
    mrp_domctl_watch_t *sw, *dw;
    int              i;

    dc = mrp_allocz(sizeof(*dc));

    if (dc != NULL) {
        mrp_list_init(&dc->pending);
        dc->ml = ml;

        dc->name    = mrp_strdup(name);
        dc->tables  = mrp_allocz_array(typeof(*dc->tables) , ntable);
        dc->watches = mrp_allocz_array(typeof(*dc->watches), nwatch);

        if (dc->name != NULL &&
            (dc->tables  != NULL || ntable == 0) &&
            (dc->watches != NULL || nwatch == 0)) {
            for (i = 0; i < ntable; i++) {
                st = tables + i;
                dt = dc->tables + i;

                dt->table       = mrp_strdup(st->table);
                dt->mql_columns = mrp_strdup(st->mql_columns);
                dt->mql_index   = mrp_strdup(st->mql_index ? st->mql_index:"");

                if (!dt->table || !dt->mql_columns || !dt->mql_index)
                    break;

                dc->ntable++;
            }

            for (i = 0; i < nwatch; i++) {
                sw = watches + i;
                dw = dc->watches + i;

                dw->table       = mrp_strdup(sw->table);
                dw->mql_columns = mrp_strdup(sw->mql_columns);
                dw->mql_where   = mrp_strdup(sw->mql_where ? sw->mql_where:"");
                dw->max_rows    = sw->max_rows;

                if (!dw->table || !dw->mql_columns || !dw->mql_where)
                    break;

                dc->nwatch++;
            }

            dc->connect_cb = connect_cb;
            dc->watch_cb   = watch_cb;
            dc->user_data  = user_data;
            dc->seqno      = 1;

            mrp_list_init(&dc->methods);

            return dc;
        }

        mrp_domctl_destroy(dc);
    }

    return NULL;
}


static void destroy_domctl(mrp_domctl_t *dc)
{
    int i;

    purge_pending(dc);

    for (i = 0; i < dc->ntable; i++) {
        mrp_free((char *)dc->tables[i].table);
        mrp_free((char *)dc->tables[i].mql_columns);
        mrp_free((char *)dc->tables[i].mql_index);
    }
    mrp_free(dc->tables);

    for (i = 0; i < dc->nwatch; i++) {
        mrp_free((char *)dc->watches[i].table);
        mrp_free((char *)dc->watches[i].mql_columns);
        mrp_free((char *)dc->watches[i].mql_where);
    }
    mrp_free(dc->watches);

    mrp_free(dc->name);
    mrp_free(dc);
}


static inline void check_destroyed(mrp_domctl_t *dc)
{
    if (dc->destroyed && dc->busy <= 0) {
        destroy_domctl(dc);
    }
}


void mrp_domctl_destroy(mrp_domctl_t *dc)
{
    if (dc != NULL) {
        mrp_domctl_disconnect(dc);

        if (dc->busy <= 0)
            destroy_domctl(dc);
        else
            dc->destroyed = TRUE;
    }
}


static void notify_disconnect(mrp_domctl_t *dc, uint32_t errcode,
                              const char *errmsg)
{
    DOMCTL_MARK_BUSY(dc, {
            dc->connected = FALSE;
            dc->connect_cb(dc, FALSE, errcode, errmsg, dc->user_data);
        });
}


static void notify_connect(mrp_domctl_t *dc)
{
    DOMCTL_MARK_BUSY(dc, {
            dc->connected = TRUE;
            dc->connect_cb(dc, TRUE, 0, NULL, dc->user_data);
        });
}


static int domctl_register(mrp_domctl_t *dc)
{
    register_msg_t  reg;
    mrp_msg_t      *msg;
    int             success;

    mrp_clear(&reg);
    reg.type    = MSG_TYPE_REGISTER;
    reg.seq     = 0;
    reg.name    = dc->name;
    reg.tables  = dc->tables;
    reg.ntable  = dc->ntable;
    reg.watches = dc->watches;
    reg.nwatch  = dc->nwatch;

    msg = msg_encode_message((msg_t *)&reg);

    if (msg != NULL) {
        success = mrp_transport_send(dc->t, msg);
        mrp_msg_unref(msg);
    }
    else
        success = FALSE;

    return success;
}


static int try_connect(mrp_domctl_t *dc)
{
    static mrp_transport_evt_t evt;

    evt.closed      = closed_cb;
    evt.recvmsg     = recv_cb;
    evt.recvmsgfrom = recvfrom_cb;

    dc->t = mrp_transport_create(dc->ml, dc->ttype, &evt, dc, 0);

    if (dc->t != NULL) {
        if (mrp_transport_connect(dc->t, &dc->addr, dc->addrlen))
            if (domctl_register(dc))
                return TRUE;

        mrp_transport_destroy(dc->t);
        dc->t = NULL;
    }

    return FALSE;
}


static void stop_reconnect(mrp_domctl_t *dc)
{
    mrp_del_timer(dc->ctmr);
    dc->ctmr = NULL;
}


static void reconnect_cb(mrp_timer_t *t, void *user_data)
{
    mrp_domctl_t *dc = (mrp_domctl_t *)user_data;

    MRP_UNUSED(t);

    if (try_connect(dc))
        stop_reconnect(dc);
}


static int start_reconnect(mrp_domctl_t *dc)
{
    int interval;

    if (dc->ctmr == NULL && dc->cival >= 0) {
        interval = dc->cival ? 1000 * dc->cival : 5000;
        dc->ctmr = mrp_add_timer(dc->ml, interval, reconnect_cb, dc);

        if (dc->ctmr == NULL)
            return FALSE;
    }

    return TRUE;
}


int mrp_domctl_connect(mrp_domctl_t *dc, const char *address, int interval)
{
    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;

    if (dc == NULL)
        return FALSE;

    addrlen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        dc->addr    = addr;
        dc->addrlen = addrlen;
        dc->cival   = interval;
        dc->ttype   = type;

        if (try_connect(dc))
            return TRUE;

        if (interval >= 0)
            return start_reconnect(dc);
    }

    return FALSE;
}


void mrp_domctl_disconnect(mrp_domctl_t *dc)
{
    if (dc->t != NULL) {
        stop_reconnect(dc);
        mrp_transport_destroy(dc->t);
        dc->t         = NULL;
        dc->connected = FALSE;
    }
}


int mrp_domctl_set_data(mrp_domctl_t *dc, mrp_domctl_data_t *tables, int ntable,
                     mrp_domctl_status_cb_t cb, void *user_data)
{
    set_msg_t  set;
    mrp_msg_t *msg;
    uint32_t   seq = dc->seqno++;
    int        success, i;

    if (!dc->connected)
        return FALSE;

    for (i = 0; i < ntable; i++) {
        if (tables[i].id < 0 || tables[i].id >= dc->ntable)
            return FALSE;
    }

    mrp_clear(&set);
    set.type   = MSG_TYPE_SET;
    set.seq    = seq;
    set.tables = tables;
    set.ntable = ntable;

    msg = msg_encode_message((msg_t *)&set);

    if (msg != NULL) {
        success = mrp_transport_send(dc->t, msg);
        mrp_msg_unref(msg);

        if (success)
            queue_pending(dc, seq, cb, user_data);

        return success;
    }
    else
        return FALSE;
}


int mrp_domctl_invoke(mrp_domctl_t *dc, const char *name, int narg,
                      mrp_domctl_arg_t *args, mrp_domctl_return_cb_t reply_cb,
                      void *user_data)
{
    invoke_msg_t  invoke;
    mrp_msg_t    *msg;
    uint32_t      seq = dc->seqno++;
    int           success;

    if (!dc->connected)
        return FALSE;

    if (reply_cb == NULL && user_data != NULL)
        return FALSE;

    mrp_clear(&invoke);
    invoke.type  = MSG_TYPE_INVOKE;
    invoke.seq   = seq;
    invoke.name  = name;
    invoke.noret = reply_cb ? TRUE : FALSE;
    invoke.narg  = narg;
    invoke.args  = args;

    msg = msg_encode_message((msg_t *)&invoke);

    if (msg != NULL) {
        success = mrp_transport_send(dc->t, msg);
        mrp_msg_unref(msg);

        if (success)
            queue_invoke(dc, seq, reply_cb, user_data);

        return success;
    }
    else
        return FALSE;
}


int mrp_domctl_register_methods(mrp_domctl_t *dc, mrp_domctl_method_def_t *defs,
                                size_t ndef)
{
    mrp_domctl_method_def_t *def;
    method_t                *m;
    size_t                   i;

    for (i = 0, def = defs; i < ndef; i++, def++) {
        m = mrp_allocz(sizeof(*m));

        if (m == NULL)
            return FALSE;

        mrp_list_init(&m->hook);

        m->name      = mrp_strdup(def->name);
        m->max_out   = def->max_out;
        m->cb        = def->cb;
        m->user_data = def->user_data;

        if (m->name == NULL) {
            mrp_free(m);
            return FALSE;
        }

        mrp_list_append(&dc->methods, &m->hook);
    }

    return TRUE;
}


static method_t *find_method(mrp_domctl_t *dc, const char *name)
{
    mrp_list_hook_t *p, *n;
    method_t        *m;

    mrp_list_foreach(&dc->methods, p, n) {
        m = mrp_list_entry(p, typeof(*m), hook);

        if (!strcmp(m->name, name))
            return m;
    }

    return NULL;
}


static void process_ack(mrp_domctl_t *dc, ack_msg_t *ack)
{
    if (ack->seq != 0)
        notify_pending(dc, (msg_t *)ack);
    else
        notify_connect(dc);
}


static void process_nak(mrp_domctl_t *dc, nak_msg_t *nak)
{
    if (nak->seq != 0)
        notify_pending(dc, (msg_t *)nak);
    else
        notify_disconnect(dc, nak->error, nak->msg);
}


static void process_notify(mrp_domctl_t *dc, notify_msg_t *notify)
{
    dc->watch_cb(dc, notify->tables, notify->ntable, dc->user_data);
}


static void process_invoke(mrp_domctl_t *dc, invoke_msg_t *invoke)
{
    method_t         *m;
    mrp_domctl_arg_t *args, error;
    int               narg;
    return_msg_t      ret;
    mrp_msg_t        *msg;
    int               i;

    mrp_clear(&ret);

    m = find_method(dc, invoke->name);

    ret.type = MSG_TYPE_RETURN;
    ret.seq  = invoke->seq;

    if (m == NULL) {
        ret.error = MRP_DOMCTL_NOTFOUND;
        args = NULL;
        narg = 0;
    }
    else {
        ret.error = MRP_DOMCTL_OK;

        narg = ret.narg = m->max_out;

        if (narg > 0) {
            args = ret.args = alloca(narg * sizeof(args[0]));
            memset(args, 0, narg * sizeof(args[0]));
        }
        else
            args = NULL;

        ret.retval = m->cb(dc, invoke->narg, invoke->args,
                           &ret.narg, ret.args, m->user_data);
    }

    msg = msg_encode_message((msg_t *)&ret);

    if (msg == NULL) {
        error.type = MRP_DOMCTL_STRING;
        error.str  = "failed to encode return message (arguments)";
        ret.error  = MRP_DOMAIN_FAILED;
        ret.narg   = 1;
        ret.args   = &error;

        msg = msg_encode_message((msg_t *)&ret);

        ret.narg = 0;
        ret.args = args = NULL;
    }

    if (msg != NULL) {
        mrp_transport_send(dc->t, msg);
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


static void process_return(mrp_domctl_t *dc, return_msg_t *ret)
{
    notify_pending(dc, (msg_t *)ret);
}


static void recv_cb(mrp_transport_t *t, mrp_msg_t *tmsg, void *user_data)
{
    mrp_domctl_t  *dc = (mrp_domctl_t *)user_data;
    msg_t         *msg;

    MRP_UNUSED(t);

    /*
      mrp_log_info("Received message:");
      mrp_msg_dump(msg, stdout);
    */

    msg = msg_decode_message(tmsg);

    if (msg != NULL) {
        switch (msg->any.type) {
        case MSG_TYPE_NOTIFY:
            process_notify(dc, &msg->notify);
            break;
        case MSG_TYPE_ACK:
            process_ack(dc, &msg->ack);
            break;
        case MSG_TYPE_NAK:
            process_nak(dc, &msg->nak);
            break;
        case MSG_TYPE_INVOKE:
            process_invoke(dc, &msg->invoke);
            break;
        case MSG_TYPE_RETURN:
            process_return(dc, &msg->ret);
            break;
        default:
            mrp_domctl_disconnect(dc);
            notify_disconnect(dc, EINVAL, "unexpected message from server");
            break;
        }

        msg_free_message(msg);
    }
    else {
        mrp_domctl_disconnect(dc);
        notify_disconnect(dc, EINVAL, "invalid message from server");
    }
}


static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t addrlen,
                        void *user_data)
{
    MRP_UNUSED(t);
    MRP_UNUSED(msg);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);
    MRP_UNUSED(user_data);

    /* XXX TODO:
     *    This should neither be called nor be necessary to specify.
     *    However, currently the transport layer mandates having to
     *    give both recv and recvfrom event callbacks if no connection
     *    event callback is given. However this is not correct because
     *    on a client side one wants to be able to create a connection-
     *    oriented transport without either connection or recvfrom event
     *    callbacks. This needs to be fixed in transport by moving the
     *    appropriate callback checks lower in the stack to the actual
     *    transport backends.
     */

    mrp_log_error("Whoa... recvfrom called for a connected transport.");
    exit(1);
}


static void closed_cb(mrp_transport_t *t, int error, void *user_data)
{
    mrp_domctl_t *dc = (mrp_domctl_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(dc);

    if (error)
        notify_disconnect(dc, error, strerror(error));
    else {
        notify_disconnect(dc, ECONNRESET, "server has closed the connection");
        start_reconnect(dc);
    }
}


static int queue_pending(mrp_domctl_t *dc, uint32_t seq,
                         mrp_domctl_status_cb_t cb, void *user_data)
{
    pending_request_t *pending;

    pending = mrp_allocz(sizeof(*pending));

    if (pending != NULL) {
        mrp_list_init(&pending->hook);

        pending->invoke    = false;
        pending->seqno     = seq;
        pending->cb.status = cb;
        pending->user_data = user_data;

        mrp_list_append(&dc->pending, &pending->hook);

        return TRUE;
    }
    else
        return FALSE;
}


static int notify_pending(mrp_domctl_t *dc, msg_t *msg)
{
    mrp_list_hook_t   *p, *n;
    pending_request_t *pending;
    uint32_t           seq;
    int                error, status;
    const char        *message;
    int                narg;
    mrp_domctl_arg_t  *args;
    int                success;

    seq = msg->any.seq;

    mrp_list_foreach(&dc->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        if (pending->seqno != seq)
            continue;

        if (!pending->invoke) {
            switch (msg->any.type) {
            case MSG_TYPE_ACK:
                error   = 0;
                message = NULL;
                goto notify;
            case MSG_TYPE_NAK:
                error   = msg->nak.error;
                message = msg->nak.msg;
            notify:
                DOMCTL_MARK_BUSY(dc, {
                        pending->cb.status(dc, error, message,
                                           pending->user_data);
                    });
                success = TRUE;
                break;
            default:
                success = FALSE;
                break;
            }
        }
        else {
            if (msg->any.type == MSG_TYPE_RETURN) {
                error  = msg->ret.error;
                status = msg->ret.retval;
                narg   = msg->ret.narg;
                args   = msg->ret.args;

                DOMCTL_MARK_BUSY(dc, {
                        pending->cb.ret(dc, error, status, narg, args,
                                        pending->user_data);
                    });
                success = TRUE;
            }
            else
                success = FALSE;
        }

        mrp_list_delete(&pending->hook);
        mrp_free(pending);

        return success;
    }

    return FALSE;
}


static int queue_invoke(mrp_domctl_t *dc, uint32_t seq,
                        mrp_domctl_return_cb_t cb, void *user_data)
{
    pending_request_t *pending;

    if (cb == NULL)
        return TRUE;

    pending = mrp_allocz(sizeof(*pending));

    if (pending != NULL) {
        mrp_list_init(&pending->hook);

        pending->invoke    = true;
        pending->seqno     = seq;
        pending->cb.ret    = cb;
        pending->user_data = user_data;

        mrp_list_append(&dc->pending, &pending->hook);

        return TRUE;
    }
    else
        return FALSE;
}


static void purge_pending(mrp_domctl_t *dc)
{
    mrp_list_hook_t   *p, *n;
    pending_request_t *pending;

    mrp_list_foreach(&dc->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        mrp_list_delete(&pending->hook);
        mrp_free(pending);
    }
}
