#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/plugins/signalling/signalling-protocol.h>

#include "client.h"
#include "transaction.h"
#include "util.h"


void free_client(client_t *c)
{
    uint i;

    mrp_free(c->name);

    for (i = 0; i < c->ndomains; i++) {
        mrp_free(c->domains[i]);
    }
    mrp_free(c->domains);

    mrp_free(c);
}


static void remove_client_from_transactions(client_t *c, data_t *ctx) {
    /* TODO: Go through all transactions. Remove the client from the
       not-answered lists. Re-allocate the client name to the other lists? */

    MRP_UNUSED(c);
    MRP_UNUSED(ctx);

    return;
}


void deregister_and_free_client(client_t *c, data_t *ctx)
{
    remove_client_from_transactions(c, ctx);
    mrp_htbl_remove(ctx->clients, c->name, 0);
    free_client(c);
}


int send_policy_decision(data_t *ctx, client_t *c, transaction_t *tx)
{
    ep_decision_t msg;

    MRP_UNUSED(ctx);

    msg.id = tx->id;
    msg.n_rows = tx->data.n_rows;
    msg.rows = tx->data.rows;

    if (tx->data.success_cb || tx->data.error_cb) {
        signalling_info("Reply required for transaction %u", tx->id);
        msg.reply_required = TRUE;
    }
    else
        msg.reply_required = FALSE;

    return mrp_transport_senddata(c->t, &msg, TAG_POLICY_DECISION);
}


static int handle_ack(client_t *c, data_t *ctx, ep_ack_t *data)
{
    signalling_info("register message");

    transaction_t *tx = get_transaction(ctx, data->id);

    if (!tx) {
        signalling_warn("no transaction with %d found, maybe already done",
            data->id);
        return 0;
    }

    switch(data->success) {
        case EP_ACK:
        {
            uint i, found = 0;
            /* go through the not_answered array */
            for (i = 0; i < tx->n_total; i++) {
                if (strcmp(c->name, tx->not_answered[i]) == 0) {
                    found = 1;
                    tx->acked[tx->n_acked++] = tx->not_answered[i];
                    tx->n_not_answered--;
                }
            }
            if (!found) {
                signalling_warn("spurious ACK from %s, ignoring", c->name);
                return 0;
            }

            break;
        }
        case EP_NACK:
        case EP_NOT_READY:
        {
            uint i, found = 0;
            /* go through the not_answered array */
            for (i = 0; i < tx->n_total; i++) {
                if (strcmp(c->name, tx->not_answered[i]) == 0) {
                    found = 1;
                    tx->nacked[tx->n_nacked++] = tx->not_answered[i];
                    tx->n_not_answered--;

                    /* FIXME: handle error here or wait for all EPs to answer? */
                }
            }
            if (!found) {
                signalling_error("spurious NACK from %s", c->name);
                return 0;
            }

            break;
        }
        default:
            signalling_error("unhandled ACK status!");
            return -1;
    }

    if (tx->n_not_answered == 0) {
        complete_transaction(ctx, tx);
    }

    return 0;
}


static int handle_register(client_t *c, data_t *ctx, ep_register_t *data)
{
    uint i;

    signalling_info("register message");

    signalling_info("ep name: %s", data->ep_name);
    signalling_info("number of domains: %d", data->n_domains);

    if (strcmp(data->ep_name, "") == 0) {
        signalling_error("EP with an empty name");

        /* TODO: send an error message back */
        return -1;
    }

    if (mrp_htbl_lookup(ctx->clients, data->ep_name)) {
        /* there already was a client of similar name */
        signalling_error("EP '%s' already exists in db", data->ep_name);

        /* TODO: send an error message back */
        return -1;
    }

    c->name = mrp_strdup(data->ep_name);
    c->ndomains = data->n_domains;
    c->domains = mrp_alloc_array(char *, data->n_domains);

    for (i = 0; i < data->n_domains; i++) {
        c->domains[i] = mrp_strdup(data->domains[i]);
        signalling_info("domain: %s", data->domains[i]);
    }

    c->registered = TRUE;
    mrp_htbl_insert(ctx->clients, c->name, c);
    ctx->n_clients++;

    return 0;
}


static void recvfrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
             mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    client_t *c = (client_t *)user_data;
    int ret = -1;

    MRP_UNUSED(t);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    signalling_info("Received message (%d)", tag);

    switch(tag) {

        case TAG_REGISTER:
            ret = handle_register(c, c->u, data);
            break;
        case TAG_ACK:
            ret = handle_ack(c, c->u, data);
            break;
        case TAG_UNREGISTER:
            break;
        default:
            signalling_warn("Unhandled message type");
            ret = 0;
            break;
    }

    if (ret < 0) {
        signalling_error("Malformed message");
    }
}


static void recv_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvfrom_evt(t, data, tag, NULL, 0, user_data);
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    client_t *c = (client_t *)user_data;
    data_t *ctx = c->u;

    if (error)
        mrp_log_error("Connection closed with error %d (%s).", error,
                strerror(error));
    else {
        mrp_log_info("Peer has closed the connection.");

        mrp_transport_disconnect(t);
        mrp_transport_destroy(t);
        c->t = NULL;

        if (c->registered)
            deregister_and_free_client(c, ctx);
    }
}


static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    data_t *ctx = (data_t *) user_data;
    int flags;
    client_t *c;

    signalling_info("Connection from peer.");

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        c->t  = mrp_transport_accept(lt, c, flags);
        c->u = ctx;
        c->registered = FALSE;

        if (c->t != NULL) {
            signalling_info("Connection accepted.");
            /* TODO: maybe remove the client if no registration in some time */
            return;
        }

        mrp_transport_destroy(c->t);
        mrp_free(c);
    }
}


int socket_setup(data_t *data)
{
    static mrp_transport_evt_t evt; /* static members are initialized to zero */

    evt.connection = connection_evt;
    evt.closed = closed_evt;
    evt.recvdatafrom = recvfrom_evt;
    evt.recvdata = recv_evt;

    mrp_transport_t *t = NULL;
    mrp_sockaddr_t addr;
    socklen_t addrlen;
    int flags;
    int ret;
    const char *type;

    ret = unlink(data->path);
    if (!(ret == 0 || ret == ENOENT)) {
        signalling_error("Could not unlink the socket at %s: %s",
            data->address, strerror(errno));
        return FALSE;
    }

    addrlen = mrp_transport_resolve(NULL, data->address,
            &addr, sizeof(addr), &type);

    if (addrlen > 0) {

        signalling_info("Address: %s, type: %s", data->address, type);

        flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;
        t = mrp_transport_create(data->ctx->ml, type, &evt, data, flags);

        if (t != NULL) {

            if (mrp_transport_bind(t, &addr, addrlen)) {
                if (mrp_transport_listen(t, 4)) {
                    data->t = t;
                    return TRUE;
                }
                else
                    signalling_error("Failed to listen on server transport.");
            }
            else
                signalling_error("Failed to bind to address %s.", data->address);
        }
        else
            signalling_error("Failed to create listening socket transport.");
    }
    else
        signalling_error("Invalid address '%s'.", data->address);

    mrp_transport_destroy(t);

    return FALSE;
}

int type_init(void)
{
    if (!mrp_msg_register_type(&ep_register_descr) ||
        !mrp_msg_register_type(&ep_decision_descr) ||
        !mrp_msg_register_type(&ep_ack_descr)) {
        mrp_log_error("Failed to register custom data type.");
        return -1;
    }
    return 0;
}
