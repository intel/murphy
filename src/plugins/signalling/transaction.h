#ifndef __MURPHY_SIGNALLING_TRANSACTION_H__
#define __MURPHY_SIGNALLING_TRANSACTION_H__

#include <stdint.h>

#include <murphy/common.h>
#include <murphy/plugins/signalling/signalling.h>

#include "plugin.h"

#define MRP_SiGNALLING_DEFAULT_TIMEOUT 5000 /* msecs */

typedef struct {
    char **domains;
    uint32_t n_domains;
    uint32_t domain_array_size;
    char **rows;
    uint32_t n_rows;
    uint32_t row_array_size;

    mrp_tx_success_cb success_cb;
    void *success_data;
    mrp_tx_error_cb error_cb;
    void *error_data;
} transaction_data_t;

/* an ongoing transaction */
typedef struct {
    uint32_t id;         /* The real ID. */
    uint32_t caller_id;  /* TODO: id assigned by caller. */
    uint32_t timeout;
    mrp_timer_t *timer;

    char **acked;
    char **nacked;
    char **not_answered;

    uint n_acked;
    uint n_nacked;
    uint n_not_answered;

    uint n_total;
    transaction_data_t data;
} transaction_t;


int fire_transaction(data_t *ctx, transaction_t *tx);

void complete_transaction(data_t *ctx, transaction_t *tx);

void free_transaction(transaction_t *tx);
transaction_t *get_transaction(data_t *ctx, uint32_t id);
void put_transaction(data_t *ctx, transaction_t *tx);
void remove_transaction(data_t *ctx, transaction_t *tx);



#endif /* __MURPHY_SIGNALLING_TRANSACTION_H__ */
