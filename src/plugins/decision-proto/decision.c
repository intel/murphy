#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include "message.h"
#include "proxy.h"
#include "table.h"
#include "notify.h"
#include "decision.h"

static int create_transports(pdp_t *pdp);
static void destroy_transports(pdp_t *pdp);

pdp_t *create_decision(mrp_context_t *ctx, const char *address)
{
    pdp_t *pdp;

    pdp = mrp_allocz(sizeof(*pdp));

    if (pdp != NULL) {
        pdp->ctx     = ctx;
        pdp->address = address;

        if (init_proxies(pdp) && init_tables(pdp) && create_transports(pdp))
            return pdp;
        else
            destroy_decision(pdp);
    }

    return NULL;
}


void destroy_decision(pdp_t *pdp)
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
    mrp_transport_t *t = proxy->t;
    char            *name;
    uint16_t         utable, uwatch, ucolumn;
    int              ntable, nwatch, ncolumn;
    int              error;
    const char      *errmsg;

    if (mrp_msg_get(req,
                    MRP_PEPMSG_STRING(NAME   , &name   ),
                    MRP_PEPMSG_UINT16(NTABLE , &utable ),
                    MRP_PEPMSG_UINT16(NWATCH , &uwatch ),
                    MRP_PEPMSG_UINT16(NCOLDEF, &ucolumn),
                    MRP_MSG_END)) {
        mrp_pep_table_t  tables[utable], watches[uwatch];
        mqi_column_def_t columns[ucolumn];

        ntable  = utable;
        nwatch  = uwatch;
        ncolumn = ucolumn;

        if (decode_register_message(req, tables, ntable, watches, nwatch,
                                    columns, ncolumn)) {
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


static void process_set_request(pep_proxy_t *proxy, mrp_msg_t *req,
                                uint32_t seq)
{
#if 1
    uint16_t    utable, uvalue, tblid, nrow;
    int         ntable, nvalue, i;
    int         error;
    const char *errmsg;
    void       *it;

    it = NULL;

    if (mrp_msg_iterate_get(req, &it,
                    MRP_PEPMSG_UINT16(NCHANGE, &utable),
                    MRP_PEPMSG_UINT16(NTOTAL , &uvalue),
                    MRP_MSG_END)) {
        mrp_pep_data_t  data[utable], *d;
        mrp_pep_value_t values[uvalue], *v;

        ntable = utable;
        nvalue = uvalue;
        d      = data;
        v      = values;

        for (i = 0; i < ntable; i++) {
            if (!mrp_msg_iterate_get(req, &it,
                                     MRP_PEPMSG_UINT16(TBLID, &tblid),
                                     MRP_PEPMSG_UINT16(NROW , &nrow),
                                     MRP_MSG_END)) {
                error  = EINVAL;
                errmsg = "malformed set message";
                goto reply_nak;
            }

            if (tblid >= proxy->ntable) {
                error  = ENOENT;
                errmsg = "invalid table id";
                goto reply_nak;
            }

            d->id      = tblid;
            d->columns = v;
            d->coldefs = proxy->tables[d->id].columns;
            d->ncolumn = proxy->tables[d->id].ncolumn;
            d->nrow    = nrow;

            if (nvalue < d->ncolumn * d->nrow) {
                error  = EINVAL;
                errmsg = "invalid set message";
                goto reply_nak;
            }

            if (!decode_set_message(req, &it, d)) {
                error  = EINVAL;
                errmsg = "invalid set message";
                goto reply_nak;
            }

            v += d->ncolumn * d->nrow;
            d++;
        }

        if (set_proxy_tables(proxy, data, ntable, &error, &errmsg)) {
            send_ack_reply(proxy->t, seq);

            return;
        }
    }

 reply_nak:
    send_nak_reply(proxy->t, seq, error, errmsg);
#else
    uint16_t    utable, uvalue;
    int         ntable, nvalue;
    int         error;
    const char *errmsg;

    if (mrp_msg_get(req,
                    MRP_PEPMSG_UINT16(NTABLE, &utable),
                    MRP_PEPMSG_UINT16(NTOTAL, &uvalue),
                    MRP_MSG_END)) {
        mrp_pep_data_t  tables[utable];
        mrp_pep_value_t values[uvalue];

        ntable = utable;
        nvalue = uvalue;

        if (decode_set_message(req, tables, ntable, values, nvalue)) {
            if (set_proxy_tables(proxy, tables, ntable, &error, &errmsg)) {
                send_ack_reply(proxy->t, seq);

                return;
            }
        }
        else
            goto malformed;
    }
    else {
    malformed:
        error  = EINVAL;
        errmsg = "malformed set message";
    }

    send_nak_reply(proxy->t, seq, error, errmsg);
#endif
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
