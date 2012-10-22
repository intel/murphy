#include <errno.h>
#include <alloca.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/transport.h>

#include "domain-control-types.h"
#include "table.h"
#include "message.h"
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
    mrp_domctl_status_cb_t  cb;          /* callback to call upon completion */
    void                   *user_data;   /* opaque callback data */
} pending_request_t;


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);
static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t addrlen,
                        void *user_data);
static void closed_cb(mrp_transport_t *t, int error, void *user_data);


static int queue_pending(mrp_domctl_t *dc, uint32_t seq,
                         mrp_domctl_status_cb_t cb, void *user_data);
static int notify_pending(mrp_domctl_t *dc, uint32_t seq, int error,
                          const char *msg);
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

        if (dc->name != NULL && dc->tables != NULL && dc->watches != NULL) {
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

            return dc;
        }

        mrp_domctl_destroy(dc);
    }

    return NULL;
}


static void destroy_domctl(mrp_domctl_t *dc)
{
    int i;

    mrp_free(dc->name);

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
    mrp_msg_t *msg;
    int        success;

    msg = create_register_message(dc);

    if (msg != NULL) {
        success = mrp_transport_send(dc->t, msg);
        mrp_msg_unref(msg);
    }
    else
        success = FALSE;

    return success;
}


int mrp_domctl_connect(mrp_domctl_t *dc, const char *address)
{
    static mrp_transport_evt_t evt = {
        .closed      = closed_cb,
        .recvmsg     = recv_cb,
        .recvmsgfrom = recvfrom_cb,
    };

    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;

    if (dc == NULL)
        return FALSE;

    addrlen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        dc->t = mrp_transport_create(dc->ml, type, &evt, dc, 0);

        if (dc->t != NULL) {
            if (mrp_transport_connect(dc->t, &addr, addrlen))
                if (domctl_register(dc))
                    return TRUE;

            mrp_transport_destroy(dc->t);
            dc->t = NULL;
        }
    }

    return FALSE;
}


void mrp_domctl_disconnect(mrp_domctl_t *dc)
{
    if (dc->t != NULL) {
        mrp_transport_destroy(dc->t);
        dc->t         = NULL;
        dc->connected = FALSE;
    }
}


int mrp_domctl_set_data(mrp_domctl_t *dc, mrp_domctl_data_t *tables, int ntable,
                     mrp_domctl_status_cb_t cb, void *user_data)
{
    mrp_msg_t *msg;
    uint32_t   seq = dc->seqno++;
    int        success, i;

    if (!dc->connected)
        return FALSE;

    for (i = 0; i < ntable; i++) {
        if (tables[i].id < 0 || tables[i].id >= dc->ntable)
            return FALSE;
    }

    msg = create_set_message(seq, tables, ntable);

    if (msg != NULL) {
        /*
          mrp_log_info("set data message message:");
          mrp_msg_dump(msg, stdout);
        */

        success = mrp_transport_send(dc->t, msg);
        mrp_msg_unref(msg);

        if (success)
            queue_pending(dc, seq, cb, user_data);

        return success;
    }
    else
        return FALSE;
}


static void process_ack(mrp_domctl_t *dc, uint32_t seq)
{
    if (seq != 0)
        notify_pending(dc, seq, 0, NULL);
    else
        notify_connect(dc);
}


static void process_nak(mrp_domctl_t *dc, uint32_t seq, int32_t err,
                        const char *msg)
{
    if (seq != 0)
        notify_pending(dc, seq, err, msg);
    else
        notify_disconnect(dc, err, msg);
}


static void process_notify(mrp_domctl_t *dc, mrp_msg_t *msg, uint32_t seq)
{
    mrp_domctl_data_t  *data, *d;
    mrp_domctl_value_t *values, *v;
    void            *it;
    uint16_t         ntable, ntotal, nrow, ncol;
    uint16_t         tblid;
    int              t, r, c;
    uint16_t         type;
    mrp_msg_value_t  value;

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
            return;

        if (tblid >= dc->nwatch)
            return;

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
                    return;

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
                    return;
                }

                v++;
            }
        }

        d++;
    }

    dc->watch_cb(dc, data, ntable, dc->user_data);
}


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    mrp_domctl_t  *dc = (mrp_domctl_t *)user_data;
    uint16_t       type, nchange, ntotal;
    uint32_t       seq;
    int            ntable, ncolumn;
    int32_t        error;
    const char    *errmsg;

    MRP_UNUSED(t);

    /*
      mrp_log_info("Received message:");
      mrp_msg_dump(msg, stdout);
    */

    if (!mrp_msg_get(msg,
                     MRP_PEPMSG_UINT16(MSGTYPE, &type),
                     MRP_PEPMSG_UINT32(MSGSEQ , &seq ),
                     MRP_MSG_END)) {
        mrp_domctl_disconnect(dc);
        notify_disconnect(dc, EINVAL, "malformed message from client");
        return;
    }

    switch (type) {
    case MRP_PEPMSG_ACK:
        process_ack(dc, seq);
        break;

    case MRP_PEPMSG_NAK:
        error  = EINVAL;
        errmsg = "request failed, unknown error";

        mrp_msg_get(msg,
                    MRP_PEPMSG_SINT32(ERRCODE, &error),
                    MRP_PEPMSG_STRING(ERRMSG , &errmsg),
                    MRP_MSG_END);

        process_nak(dc, seq, error, errmsg);
        break;

    case MRP_PEPMSG_NOTIFY:
        if (mrp_msg_get(msg,
                        MRP_PEPMSG_UINT16(NCHANGE, &nchange),
                        MRP_PEPMSG_UINT16(NTOTAL , &ntotal),
                        MRP_MSG_END)) {
            ntable  = nchange;
            ncolumn = ntotal;

            process_notify(dc, msg, seq);
        }
        break;

    default:
        break;
    }

}


static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t addrlen,
                        void *user_data)
{
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    /* XXX TODO:
     *    This should neither be called nor be necessary to specify.
     *    However, currently the transport layer mandates having to
     *    give both recv and recvfrom event callbacks if no connection
     *    event callback is given. However this is not correct because
     *    on a client side one wants to be able to create a connection-
     *    oriented transport without both connection and recvfrom event
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
    else
        notify_disconnect(dc, ECONNRESET, "server has closed the connection");
}


static int queue_pending(mrp_domctl_t *dc, uint32_t seq,
                         mrp_domctl_status_cb_t cb, void *user_data)
{
    pending_request_t *pending;

    pending = mrp_allocz(sizeof(*pending));

    if (pending != NULL) {
        mrp_list_init(&pending->hook);

        pending->seqno     = seq;
        pending->cb        = cb;
        pending->user_data = user_data;

        mrp_list_append(&dc->pending, &pending->hook);

        return TRUE;
    }
    else
        return FALSE;
}


static int notify_pending(mrp_domctl_t *dc, uint32_t seq, int error,
                          const char *msg)
{
    mrp_list_hook_t   *p, *n;
    pending_request_t *pending;

    mrp_list_foreach(&dc->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        if (pending->seqno == seq) {
            DOMCTL_MARK_BUSY(dc, {
                    pending->cb(dc, error, msg, pending->user_data);
                    mrp_list_delete(&pending->hook);
                    mrp_free(pending);
                });

            return TRUE;
        }
    }

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
