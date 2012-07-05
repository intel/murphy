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

#ifndef __MURPHY_SIGNALLING_H__
#define __MURPHY_SIGNALLING_H__

#include <stdint.h>
#include <murphy/common.h>

typedef enum {
    MRP_TX_ERROR_UNDEFINED,
    MRP_TX_ERROR_NOT_ANSWERED,
    MRP_TX_ERROR_NACKED,
    MRP_TX_ERROR_MAX
} mrp_tx_error_t;

typedef void (*mrp_tx_success_cb) (uint32_t tx, void *data);
typedef void (*mrp_tx_error_cb) (uint32_t tx, mrp_tx_error_t err, void *data);
typedef void (*mrp_info_cb) (char *msg, void *data);


#if 0
/** Opens a new signal with given 'tx'. */

int mrp_tx_open_signal_with_id(uint32_t tx);
#endif

/** Opens a new signal. Returns the assigned signal 'tx'. */

uint32_t mrp_tx_open_signal();

/** Adds a policy domain to the signal identified by 'tx'. */

int mrp_tx_add_domain(uint32_t tx, const char *domain);

/** Adds a data row to the signal identified by 'tx'. */

int mrp_tx_add_data(uint32_t tx, const char *row);

/** Adds a success callback to the signal identified by 'tx'. The callback
    will be called if the signal is successfully ACKed by all enforcement
    points registered to listen for the domains. If the success or error
    callback isn't set, the enforcement points are not required to reply
    to the signal.*/

void mrp_tx_add_success_cb(uint32_t tx, mrp_tx_success_cb cb, void *data);

/** Adds an error callback to the signal identified by 'tx'. The callback
    will be called if the signal is NACKed or not answered to by one or more
    enforcement points registered to listen for the domains. If the success
    or error callback isn't set, the enforcement points are not required to
    reply to the signal.*/

void mrp_tx_add_error_cb(uint32_t tx, mrp_tx_error_cb cb, void *data);

/** Closes the signal identified by 'tx' and sends it onward to the enforcement
    points. Returns a negative value if the transaction cannot be found. */

int mrp_tx_close_signal(uint32_t tx);

/** Cancels a signal identified by 'tx'.*/

void mrp_tx_cancel_signal(uint32_t tx);

/** Register a backchannel handler for extra messages coming from the
    enforcement point identified by 'client_id'. Returns negative value
    if the handler was already registered. */

int mrp_info_register(char *client_id, mrp_info_cb cb, void *data);

/** Unregister a backchannel handler for enforcement point identified by
    'client_id'. */

void mrp_info_unregister(char *client_id);


#endif /* __MURPHY_SIGNALLING_H__ */
