#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/transport.h>

#include "decision-types.h"
#include "table.h"
#include "message.h"
#include "client.h"


/*
 * mark an enforcement point busy (typically while executing a callback)
 */

#define PEP_MARK_BUSY(pep, ...) do {                \
        (pep)->busy++;                              \
        __VA_ARGS__                                 \
        (pep)->busy--;                              \
        check_destroyed(pep);                       \
    } while (0)


/*
 * a pending request
 */

typedef struct {
    mrp_list_hook_t      hook;           /* hook to pending request queue */
    uint32_t             seqno;          /* sequence number/request id */
    mrp_pep_status_cb_t  cb;             /* callback to call upon completion */
    void                *user_data;      /* opaque callback data */
} pending_request_t;


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);
static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t addrlen,
                        void *user_data);
static void closed_cb(mrp_transport_t *t, int error, void *user_data);


static int queue_pending(mrp_pep_t *pep, uint32_t seq,
                         mrp_pep_status_cb_t cb, void *user_data);
static int notify_pending(mrp_pep_t *pep, uint32_t seq, int error,
                          const char *msg);
static void purge_pending(mrp_pep_t *pep);




mrp_pep_t *mrp_pep_create(const char *name, mrp_mainloop_t *ml,
                          mrp_pep_table_t *owned_tables, int nowned,
                          mrp_pep_table_t *watched_tables, int nwatched,
                          mrp_pep_connect_cb_t connect_cb,
                          mrp_pep_data_cb_t data_cb, void *user_data)
{
    mrp_pep_t *pep;

    pep = mrp_allocz(sizeof(*pep));

    if (pep != NULL) {
        mrp_list_init(&pep->pending);
        pep->ml = ml;

        pep->name    = mrp_strdup(name);
        pep->owned   = mrp_allocz_array(typeof(*pep->owned), nowned);
        pep->watched = mrp_allocz_array(typeof(*pep->watched), nwatched);

        if (pep->name != NULL && pep->owned != NULL && pep->watched != NULL) {
            if (copy_pep_tables(owned_tables, pep->owned, nowned)) {
                pep->nowned = nowned;
                if (copy_pep_tables(watched_tables, pep->watched, nwatched)) {
                    pep->nwatched   = nwatched;
                    pep->connect_cb = connect_cb;
                    pep->data_cb    = data_cb;
                    pep->user_data  = user_data;
                    pep->seqno      = 1;

                    return pep;
                }
            }
        }

        mrp_pep_destroy(pep);
    }

    return NULL;
}


static void destroy_pep(mrp_pep_t *pep)
{
    mrp_free(pep->name);

    free_pep_tables(pep->owned, pep->nowned);
    free_pep_tables(pep->watched, pep->nwatched);

    purge_pending(pep);

    mrp_free(pep);
}


static inline void check_destroyed(mrp_pep_t *pep)
{
    if (pep->destroyed && pep->busy <= 0) {
        destroy_pep(pep);
    }
}


void mrp_pep_destroy(mrp_pep_t *pep)
{
    if (pep != NULL) {
        mrp_pep_disconnect(pep);

        if (pep->busy <= 0)
            destroy_pep(pep);
        else
            pep->destroyed = TRUE;
    }
}


static void notify_disconnect(mrp_pep_t *pep, uint32_t errcode,
                              const char *errmsg)
{
    PEP_MARK_BUSY(pep, {
            pep->connected = FALSE;
            pep->connect_cb(pep, FALSE, errcode, errmsg, pep->user_data);
        });
}


static void notify_connect(mrp_pep_t *pep)
{
    PEP_MARK_BUSY(pep, {
            pep->connected = TRUE;
            pep->connect_cb(pep, TRUE, 0, NULL, pep->user_data);
        });
}


static int pep_register(mrp_pep_t *pep)
{
    mrp_msg_t *msg;
    int        success;

    msg = create_register_message(pep);

    if (msg != NULL) {
        success = mrp_transport_send(pep->t, msg);
        mrp_msg_unref(msg);
    }
    else
        success = FALSE;

    return success;
}


int mrp_pep_connect(mrp_pep_t *pep, const char *address)
{
    static mrp_transport_evt_t evt = {
        .closed      = closed_cb,
        .recvmsg     = recv_cb,
        .recvmsgfrom = recvfrom_cb,
    };

    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;

    if (pep == NULL)
        return FALSE;

    addrlen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        pep->t = mrp_transport_create(pep->ml, type, &evt, pep, 0);

        if (pep->t != NULL) {
            if (mrp_transport_connect(pep->t, &addr, addrlen))
                if (pep_register(pep))
                    return TRUE;

            mrp_transport_destroy(pep->t);
            pep->t = NULL;
        }
    }

    return FALSE;
}


void mrp_pep_disconnect(mrp_pep_t *pep)
{
    if (pep->t != NULL) {
        mrp_transport_destroy(pep->t);
        pep->t         = NULL;
        pep->connected = FALSE;
    }
}


int mrp_pep_set_data(mrp_pep_t *pep, mrp_pep_data_t *data, int ntable,
                     mrp_pep_status_cb_t cb, void *user_data)
{
    mrp_msg_t *msg;
    uint32_t   seq = pep->seqno++;
    int        success, i;

    if (!pep->connected)
        return FALSE;

    for (i = 0; i < ntable; i++) {
        if (data[i].id < 0 || data[i].id >= pep->nowned)
            return FALSE;

        data[i].coldefs = pep->owned[data[i].id].columns;
        data[i].ncolumn = pep->owned[data[i].id].ncolumn;
    }

    msg = create_set_message(seq, data, ntable);

    if (msg != NULL) {
        success = mrp_transport_send(pep->t, msg);
        mrp_msg_unref(msg);

        if (success)
            queue_pending(pep, seq, cb, user_data);

        return success;
    }
    else
        return FALSE;
}


static void process_ack(mrp_pep_t *pep, uint32_t seq)
{
    if (seq != 0)
        notify_pending(pep, seq, 0, NULL);
    else
        notify_connect(pep);
}


static void process_nak(mrp_pep_t *pep, uint32_t seq, int32_t err,
                        const char *msg)
{
    if (seq != 0)
        notify_pending(pep, seq, err, msg);
    else
        notify_disconnect(pep, err, msg);
}


static void process_notify(mrp_pep_t *pep, mrp_msg_t *msg, uint32_t seq,
                           int ntable, int ncolumn)
{
    mrp_pep_table_t  *tbl;
    mrp_pep_data_t    data[ntable], *d;
    mrp_pep_value_t   values[ncolumn], *v;
    mqi_column_def_t *cols;
    void             *it;
    int               ncol, i, j;
    uint16_t          tblid;
    uint16_t          nrow;

    it = NULL;
    d  = data;
    v  = values;

    for (i = 0; i < ntable; i++) {
        if (!mrp_msg_iterate_get(msg, &it,
                                 MRP_PEPMSG_UINT16(TBLID, &tblid),
                                 MRP_PEPMSG_UINT16(NROW , &nrow ),
                                 MRP_MSG_END))
            return;

        if (tblid >= pep->nwatched)
            return;

        tbl  = pep->watched + tblid;
        cols = tbl->columns;
        ncol = tbl->ncolumn;

        d->id      = tblid;
        d->columns = v;
        d->coldefs = tbl->columns;
        d->ncolumn = ncol;
        d->nrow    = nrow;

        if (!decode_notify_message(msg, &it, d))
            return;

        d++;
        v += ncol * nrow;
    }

    pep->data_cb(pep, data, ntable, pep->user_data);
}


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    mrp_pep_t  *pep = (mrp_pep_t *)user_data;
    uint16_t    type, nchange, ntotal;
    uint32_t    seq;
    int         ntable, ncolumn;
    int32_t     error;
    const char *errmsg;

    MRP_UNUSED(t);

    /*
      mrp_log_info("Received message:");
      mrp_msg_dump(msg, stdout);
    */

    if (!mrp_msg_get(msg,
                     MRP_PEPMSG_UINT16(MSGTYPE, &type),
                     MRP_PEPMSG_UINT32(MSGSEQ , &seq ),
                     MRP_MSG_END)) {
        mrp_pep_disconnect(pep);
        notify_disconnect(pep, EINVAL, "malformed message from client");
        return;
    }

    switch (type) {
    case MRP_PEPMSG_ACK:
        process_ack(pep, seq);
        break;

    case MRP_PEPMSG_NAK:
        error  = EINVAL;
        errmsg = "request failed, unknown error";

        mrp_msg_get(msg,
                    MRP_PEPMSG_SINT32(ERRCODE, &error),
                    MRP_PEPMSG_STRING(ERRMSG , &errmsg),
                    MRP_MSG_END);

        process_nak(pep, seq, error, errmsg);
        break;

    case MRP_PEPMSG_NOTIFY:
        if (mrp_msg_get(msg,
                        MRP_PEPMSG_UINT16(NCHANGE, &nchange),
                        MRP_PEPMSG_UINT16(NTOTAL , &ntotal),
                        MRP_MSG_END)) {
            ntable  = nchange;
            ncolumn = ntotal;

            process_notify(pep, msg, seq, ntable, ncolumn);
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
    mrp_pep_t *pep = (mrp_pep_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(pep);

    if (error)
        notify_disconnect(pep, error, strerror(error));
    else
        notify_disconnect(pep, ECONNRESET, "server has closed the connection");
}


static int queue_pending(mrp_pep_t *pep, uint32_t seq,
                         mrp_pep_status_cb_t cb, void *user_data)
{
    pending_request_t *pending;

    pending = mrp_allocz(sizeof(*pending));

    if (pending != NULL) {
        mrp_list_init(&pending->hook);

        pending->seqno     = seq;
        pending->cb        = cb;
        pending->user_data = user_data;

        mrp_list_append(&pep->pending, &pending->hook);

        return TRUE;
    }
    else
        return FALSE;
}


static int notify_pending(mrp_pep_t *pep, uint32_t seq, int error,
                          const char *msg)
{
    mrp_list_hook_t   *p, *n;
    pending_request_t *pending;

    mrp_list_foreach(&pep->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        if (pending->seqno == seq) {
            PEP_MARK_BUSY(pep, {
                    pending->cb(pep, error, msg, pending->user_data);
                    mrp_list_delete(&pending->hook);
                    mrp_free(pending);
                });

            return TRUE;
        }
    }

    return FALSE;
}


static void purge_pending(mrp_pep_t *pep)
{
    mrp_list_hook_t   *p, *n;
    pending_request_t *pending;

    mrp_list_foreach(&pep->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        mrp_list_delete(&pending->hook);
        mrp_free(pending);
    }
}
