#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/plugins/signalling/signalling.h>

#include "plugin.h"
#include "util.h"
#include "client.h"
#include "transaction.h"

extern mrp_plugin_t *signalling_plugin;


void free_transaction(transaction_t *tx)
{
    uint i;

    if (tx->timer)
        mrp_del_timer(tx->timer);

    for (i = 0; i < tx->data.n_rows; i++) {
        mrp_free(tx->data.rows[i]);
    }
    mrp_free(tx->data.rows);

    for (i = 0; i < tx->data.n_domains; i++) {
        mrp_free(tx->data.domains[i]);
    }
    mrp_free(tx->data.domains);

    mrp_free(tx->acked);
    mrp_free(tx->nacked);
    mrp_free(tx->not_answered);

    mrp_free(tx);
}


transaction_t *get_transaction(data_t *ctx, uint32_t id)
{
    return mrp_htbl_lookup(ctx->txs, u_to_p(id));
}


void put_transaction(data_t *ctx, transaction_t *tx)
{
    mrp_htbl_insert(ctx->txs, u_to_p(tx->id), tx);
}


void remove_transaction(data_t *ctx, transaction_t *tx)
{
    mrp_htbl_remove(ctx->txs, u_to_p(tx->id), TRUE);
}


static uint32_t assign_id(data_t *ctx)
{
    return ctx->next_id++;
}


static bool domain_match(client_t *c, transaction_t *tx)
{
    uint i, j;

    for (i = 0; i < tx->data.n_domains; i++) {
        for (j = 0; j < c->ndomains; j++) {
            if (strcmp(tx->data.domains[i], c->domains[j]) == 0) {
                return TRUE;
            }
        }
    }
    return FALSE;
}


static int is_interested(void *key, void *entry, void *user_data)
{
    transaction_t *tx = user_data;
    client_t *c = entry;

    MRP_UNUSED(key);

    if (!tx || !c)
        return MRP_HTBL_ITER_STOP;

    if (domain_match(c, tx)) {
        tx->not_answered[tx->n_not_answered++] = c->name;
    }

    return MRP_HTBL_ITER_MORE;
}


static void signalling_timeout(mrp_mainloop_t *ml, mrp_timer_t *timer,
                             void *user_data)
{
    uint32_t id = p_to_u(user_data);
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    MRP_UNUSED(ml);
    MRP_UNUSED(timer);

    tx = get_transaction(ctx, id);

    if (tx) {
        complete_transaction(ctx, tx);
    }
}


int fire_transaction(data_t *ctx, transaction_t *tx)
{
    /* TODO: make proper queuing */

    uint i;

    for (i = 0; i < tx->n_not_answered; i++) {
        client_t *c = mrp_htbl_lookup(ctx->clients, tx->not_answered[i]);
        if (send_policy_decision(ctx, c, tx) < 0) {
            signalling_error("Failed to send policy decision to %s", c->name);
        }
    }

    mrp_add_timer(ctx->ctx->ml, tx->timeout, signalling_timeout,
            u_to_p(tx->id));

    return 0;
}


void complete_transaction(data_t *ctx, transaction_t *tx)
{
    /* call the transaction callbacks */

    if (tx->n_not_answered == 0 && tx->n_acked == tx->n_total) {
        if (tx->data.success_cb)
            tx->data.success_cb(tx->id, tx->data.success_data);
    }
    else if (tx->n_nacked > 0 ){
        if (tx->data.error_cb)
            tx->data.error_cb(tx->id, MRP_TX_ERROR_NACKED, tx->data.error_data);
    }
    else {
        if (tx->data.error_cb)
            tx->data.error_cb(tx->id, MRP_TX_ERROR_NOT_ANSWERED, tx->data.error_data);
    }

    /* remove the transaction from the list */
    remove_transaction(ctx, tx);
}


static uint32_t mrp_tx_open_signal_with_id(uint32_t id)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = mrp_allocz(sizeof(transaction_t));
    tx->id = id;
    tx->data.row_array_size = 32;
    tx->data.rows = mrp_allocz(tx->data.row_array_size);
    tx->timeout = MRP_SiGNALLING_DEFAULT_TIMEOUT;

    tx->data.domain_array_size = 8;
    tx->data.domains = mrp_allocz(tx->data.domain_array_size);

    put_transaction(ctx, tx);

    return id;
}


uint32_t mrp_tx_open_signal()
{
    data_t *ctx = signalling_plugin->data;

    return mrp_tx_open_signal_with_id(assign_id(ctx));
}


int mrp_tx_add_domain(uint32_t id, const char *domain)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (!tx)
        return -1;

    tx->data.domains[tx->data.n_domains++] = mrp_strdup(domain);

    if (tx->data.n_domains == tx->data.domain_array_size) {
        tx->data.n_domains *= 2;
        tx->data.domains = mrp_realloc(tx->data.domains, tx->data.n_domains);
    }

    return 0;
}


int mrp_tx_add_data(uint32_t id, const char *row)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (!tx)
        return -1;

    tx->data.rows[tx->data.n_rows++] = mrp_strdup(row);

    if (tx->data.n_rows == tx->data.row_array_size) {
        tx->data.n_rows *= 2;
        tx->data.rows = mrp_realloc(tx->data.rows, tx->data.n_rows);
    }

    return 0;
}


void mrp_tx_add_success_cb(uint32_t id, mrp_tx_success_cb cb, void *data)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (tx) {
        tx->data.success_cb = cb;
        tx->data.success_data = data;
    }
}


void mrp_tx_add_error_cb(uint32_t id, mrp_tx_error_cb cb, void *data)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (tx) {
        tx->data.error_cb = cb;
        tx->data.error_data = data;
    }
}


void mrp_tx_close_signal(uint32_t id)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (tx) {
        /* allocate the client arrays */

        tx->acked = mrp_allocz_array(char *, ctx->n_clients);
        tx->nacked = mrp_allocz_array(char *, ctx->n_clients);
        tx->not_answered = mrp_allocz_array(char *, ctx->n_clients);

        mrp_htbl_foreach(ctx->clients, is_interested, tx);

        tx->n_total = tx->n_not_answered;
        fire_transaction(ctx, tx);
    }
}


void mrp_tx_cancel_signal(uint32_t id)
{
    data_t *ctx = signalling_plugin->data;
    transaction_t *tx;

    tx = get_transaction(ctx, id);

    if (tx) {
        remove_transaction(ctx, tx);
    }
}
