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

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/plugins/signalling/signalling-protocol.h>

#include "plugin.h"
#include "info.h"

extern mrp_plugin_t *signalling_plugin;


void free_backchannel(backchannel_t *b)
{
    mrp_free(b->client_id);
    mrp_free(b);
}


int _mrp_info_register(const char *client_id, mrp_info_cb cb, void *data)
{
    data_t *ctx = signalling_plugin->data;
    backchannel_t *b;

    b = mrp_htbl_lookup(ctx->backchannels, (void *) client_id);

    if (b) {
        /* someone is already handling this signal */
        return -1;
    }

    b = mrp_allocz(sizeof(backchannel_t));

    if (!b) {
        return -1;
    }

    b->cb = cb;
    b->data = data;

    /* for hash table memory management */
    b->client_id = mrp_strdup(client_id);

    mrp_htbl_insert(ctx->backchannels, b->client_id, b);

    return 0;
}


void _mrp_info_unregister(const char *client_id)
{
    data_t *ctx = signalling_plugin->data;
    backchannel_t *b;

    b = mrp_htbl_lookup(ctx->backchannels, (void *) client_id);

    if (b) {
        mrp_htbl_remove(ctx->backchannels, (void *) client_id, TRUE);
    }
}
