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

#ifndef __MURPHY_PROCESS_H__
#define __MURPHY_PROCESS_H__

#include <murphy/common/macros.h>
#include <murphy/common/mainloop.h>

MRP_CDECL_BEGIN

/* functions for the murphy family of processes */

typedef enum {
    MRP_PROCESS_STATE_UNKNOWN,
    MRP_PROCESS_STATE_READY,
    MRP_PROCESS_STATE_NOT_READY
} mrp_process_state_t;

typedef void (*mrp_process_watch_handler_t)(const char *,
        mrp_process_state_t, void *);

int mrp_process_set_state(const char *id, mrp_process_state_t state);

mrp_process_state_t mrp_process_query_state(const char *id);

int mrp_process_set_watch(const char *id, mrp_mainloop_t *ml,
        mrp_process_watch_handler_t cb, void *userdata);

int mrp_process_remove_watch(const char *id);

/* functions to track external processes by pid */

typedef struct mrp_pid_watch_s mrp_pid_watch_t;

typedef void (*mrp_pid_watch_handler_t)(pid_t,
        mrp_process_state_t, void *);

mrp_process_state_t mrp_pid_query_state(pid_t pid);

mrp_pid_watch_t *mrp_pid_set_watch(pid_t pid, mrp_mainloop_t *ml,
        mrp_pid_watch_handler_t cb, void *userdata);

int mrp_pid_remove_watch(mrp_pid_watch_t *w);


MRP_CDECL_END

#endif /* __MURPHY_PROCESS_H__ */
