/*
 * Copyright (c) 2013, Intel Corporation
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

#include <murphy/common.h>
#include <murphy/common/process.h>

static void process_watch(const char *id, mrp_process_state_t s,
        void *userdata)
{
    mrp_mainloop_t *ml = userdata;

    printf("received event for %s: %s (%p)\n",
        id, s == MRP_PROCESS_STATE_READY ? "ready" : "not ready", userdata);

    mrp_mainloop_quit(ml, 0);
}


int main() {
    mrp_mainloop_t *ml = mrp_mainloop_create();

    mrp_process_state_t s = mrp_process_query_state("foobar");

    printf("initial state %s\n",
            s == MRP_PROCESS_STATE_READY ? "ready" : "not ready");

    if (mrp_process_set_state("foobar", MRP_PROCESS_STATE_READY) < 0) {
        printf("error setting the state 1\n");
    }

    s = mrp_process_query_state("foobar");

    printf("second state %s\n",
            s == MRP_PROCESS_STATE_READY ? "ready" : "not ready");

    if (mrp_process_set_state("foobar", MRP_PROCESS_STATE_NOT_READY) < 0) {
        printf("error setting the state 2\n");
    }

    s = mrp_process_query_state("foobar");

    printf("third state %s\n",
            s == MRP_PROCESS_STATE_READY ? "ready" : "not ready");

    if (mrp_process_set_watch("foobar", ml, process_watch, ml) < 0) {
        printf("failed to register watch\n");
    }

    printf("setting state to ready\n");

    if (mrp_process_set_state("foobar", MRP_PROCESS_STATE_READY) < 0) {
        printf("error setting the state 3\n");
    }

    mrp_mainloop_run(ml);

    printf("removing the watch\n");

    if(mrp_process_remove_watch("foobar") < 0) {
        printf("failed to remove watch\n");
    }

    mrp_mainloop_destroy(ml);
}