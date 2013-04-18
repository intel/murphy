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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


static void process_watch(const char *id, mrp_process_state_t s,
        void *userdata)
{
    mrp_mainloop_t *ml = (mrp_mainloop_t *) userdata;

    printf("process watch received event for %s: %s (%p)\n",
        id, s == MRP_PROCESS_STATE_READY ? "ready" : "not ready", userdata);

    mrp_mainloop_quit(ml, 0);
}


static void test_process_watch(mrp_mainloop_t *ml)
{
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
}

static void pid_watch(pid_t pid, mrp_process_state_t s, void *userdata)
{
    mrp_mainloop_t *ml = (mrp_mainloop_t *) userdata;

    printf("pid watch received event for %d: %s (%p)\n",
        pid, s == MRP_PROCESS_STATE_READY ? "ready" : "not ready", userdata);

    mrp_mainloop_quit(ml, 0);
}

static void test_pid_watch(mrp_mainloop_t *ml)
{
    pid_t pid = fork();

    if (pid < 0) {
        printf("error forking\n");
    }
    else if (pid > 0) {
        mrp_pid_watch_t *w;

        if (mrp_pid_query_state(pid) != MRP_PROCESS_STATE_READY) {
            printf("failed to query the process READY state\n");
        }

        printf("setting pid watch\n");
        w = mrp_pid_set_watch(pid, ml, pid_watch, ml);

        printf("killing the process '%d'\n", pid);
        kill(pid, 15);
        waitpid(pid, NULL, 0);

        printf("running main loop\n");
        mrp_mainloop_run(ml);

        if (mrp_pid_query_state(pid) != MRP_PROCESS_STATE_NOT_READY) {
            printf("failed to query the process NOT READY state\n");
        }
        printf("removing the watch\n");
        mrp_pid_remove_watch(w);
    }
}

int main(int argc, char **argv) {
    mrp_mainloop_t *ml = mrp_mainloop_create();

    if (argc == 2 && strcmp(argv[1], "pid") == 0) {
        test_pid_watch(ml);
    }
    else if (argc == 2 && strcmp(argv[1], "process") == 0) {
        test_process_watch(ml);
    }
    else {
        printf("Usage: process-watch-test <process|pid>\n");
    }

    mrp_mainloop_destroy(ml);
}

