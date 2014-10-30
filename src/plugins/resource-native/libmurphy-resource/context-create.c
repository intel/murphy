/*
 * Copyright (c) 2014, Intel Corporation
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <murphy/plugins/resource-native/libmurphy-resource/resource-api.h>

mrp_mainloop_t *ml;

void state_cb(mrp_res_context_t *cx, mrp_res_error_t err, void *userdata)
{
    MRP_UNUSED(cx);
    MRP_UNUSED(err);
    MRP_UNUSED(userdata);
}

void deferred_cb(mrp_deferred_t *d, void *user_data)
{
    uint *iterations = user_data;

    (*iterations)--;

    if (*iterations == 0) {
        mrp_del_deferred(d);
        mrp_mainloop_quit(ml, 0);
        return;
    }

    mrp_res_context_t *ctx = mrp_res_create(ml, state_cb, NULL);
    mrp_res_destroy(ctx);
}

void usage()
{
    printf("context-create <iterations>\n");
}

int main(int argc, char **argv)
{
    uint iterations = 0;

    if (argc != 2) {
        usage();
        exit(1);
    }

    if ((ml = mrp_mainloop_create()) == NULL)
        exit(1);

    iterations = strtoul(argv[1], NULL, 10);

    mrp_add_deferred(ml, deferred_cb, &iterations);

    /* start looping */
    mrp_mainloop_run(ml);

    return 0;
}
