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

#ifndef __MURPHY_SIGNALLING_ENDPOINT_H__
#define __MURPHY_SIGNALLING_ENDPOINT_H__

#include <stdint.h>

#include <murphy/common.h>

/* The different transport types require different hacks to make them work.
   This is a "sin bin" to handle them. */

/* We don't have any security, so we don't allow any network connections etc.
   coming from outside of the device. */

 typedef enum {
    TPORT_UNKNOWN,
    TPORT_UNXS,
    TPORT_DBUS,
    TPORT_INTERNAL,
    TPORT_MAX
 } signalling_transport_t;

typedef struct {
    char *address;          /* endpoint address */
    signalling_transport_t type;  /* endpoint type */
    bool connection_oriented; /* if the endpoint is connection-oriented */

    const char *stype;
    mrp_sockaddr_t addr;
    socklen_t addrlen;
    mrp_mainloop_t *ml;
    mrp_transport_t *t;

    mrp_list_hook_t hook; /* es */
    mrp_list_hook_t clients;

    void *user_data;
    mrp_transport_evt_t proxy_evt; /* the proxy callbacks */
    mrp_transport_evt_t evt; /* the real callbacks to call */
} endpoint_t;

typedef struct {
    mrp_list_hook_t hook; /* for clients */
    endpoint_t *e;
    void *client;
} endpoint_tport_t;

endpoint_t *create_endpoint(const char *address, mrp_mainloop_t *ml);

int clean_endpoint(endpoint_t *e);

int start_endpoint(endpoint_t *e, mrp_transport_evt_t *evt, void *userdata);

void delete_endpoint(endpoint_t *e);

mrp_transport_t *accept_connection(endpoint_t *e, mrp_transport_t *lt,
        void *client);

#endif /* __MURPHY_SIGNALLING_ENDPOINT_H__ */
