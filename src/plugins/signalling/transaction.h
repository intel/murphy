/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/* exported functions */
uint32_t _mrp_tx_open_signal();
int _mrp_tx_add_domain(uint32_t id, const char *domain);
int _mrp_tx_add_data(uint32_t id, const char *row);
void _mrp_tx_add_success_cb(uint32_t id, mrp_tx_success_cb cb, void *data);
void _mrp_tx_add_error_cb(uint32_t id, mrp_tx_error_cb cb, void *data);
int _mrp_tx_close_signal(uint32_t id);
void _mrp_tx_cancel_signal(uint32_t id);


#endif /* __MURPHY_SIGNALLING_TRANSACTION_H__ */
