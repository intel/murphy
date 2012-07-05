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
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/core.h>

#include "endpoint.h"
#include "util.h"


endpoint_t *create_endpoint(const char *address, mrp_mainloop_t *ml)
{
    endpoint_t *e;

    e = mrp_alloc(sizeof(endpoint_t));

    e->ml = ml;
    e->address = mrp_strdup(address);
    e->addrlen = mrp_transport_resolve(NULL, e->address,
            &e->addr, sizeof(e->addr), &e->stype);

    if (e->addrlen <= 0) {
        signalling_error("failed to bind address %s", e->address);
        mrp_free(e->address);
        mrp_free(e);
        return NULL;
    }

    if (strncmp(e->stype, "unxs", 4) == 0) {
        e->type = TPORT_UNXS;
        e->connection_oriented = TRUE;
    }
    else if (strncmp(e->stype, "dbus", 4) == 0) {
        e->type = TPORT_DBUS;
        e->connection_oriented = FALSE;
    }
    else if (strncmp(e->stype, "internal", 8) == 0) {
        e->type = TPORT_INTERNAL;
        e->connection_oriented = TRUE;
    }
    else {
        signalling_error("not supported transport type: %s", e->stype);
        mrp_free(e->address);
        mrp_free(e);
        return NULL;
    }

    mrp_list_init(&e->hook);
    mrp_list_init(&e->clients);

    return e;
}


int clean_endpoint(endpoint_t *e)
{
    switch (e->type) {
        case TPORT_UNXS:
        {
            char *path = e->address + 5;
            int len;

            /* make sure that this is a real file system path */
            len = strlen(path);
            if (len >= 1) {
                if (path[0] != '@') {
                    /* TODO: stat and check the socket status */
                    unlink(path);
                }
            }
            break;
        }
        default:
        break;
    }
    return 0;
}


static void recvfrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
            mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    endpoint_tport_t *et = user_data;

    signalling_info("proxying recvfrom_evt (ep: %s) (%p, %p, %d, %p)",
            et->e->address, t, data, tag, user_data);

    if (et->e->evt.recvdatafrom) {
        et->e->evt.recvdatafrom(t, data, tag, addr, addrlen, et->client);
    }
}


static void recv_evt(mrp_transport_t *t, void *data, uint16_t tag,
            void *user_data)
{
    endpoint_tport_t *et = user_data;

    signalling_info("proxying recv_evt (ep: %s) (%p, %p, %d, %p)",
            et->e->address, t, data, tag, user_data);

    if (et->e->evt.recvdata) {
        et->e->evt.recvdata(t, data, tag, et->client);
    }
}

static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    endpoint_t *e = user_data;

    signalling_info("proxying connection_evt (ep: %s) (%p, %p)",
            e->address, lt, user_data);

    switch (e->type) {
        case TPORT_DBUS:
        {
            /* start listening to name changed event to discover when the
               client goes away? */

            break;
        }
        default:
        break;
    }

    if (e->evt.connection) {

        e->evt.connection(lt, e);
    }
}

static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    endpoint_tport_t *et;

    /* check if t is valid */

    if (!t->connected) {

        /* we are trying to fix the case where the connection has never been
           accepted, meaning there is no endpoint_tport_t allocated at all. */
        return;
    }

    et = user_data;

    signalling_info("proxying closed_evt (ep: %s) (%p, %d, %p)",
            et->e->address, t, error, user_data);

    if (et->e->evt.closed) {
        et->e->evt.closed(t, error, et->client);
    }

    mrp_list_delete(&et->hook);
    mrp_free(et);
}


int start_endpoint(endpoint_t *e, mrp_transport_evt_t *evt, void *userdata)
{
    int flags;

    memcpy(&e->evt, evt, sizeof(mrp_transport_evt_t));

    e->user_data = userdata;

    e->proxy_evt.connection = connection_evt;
    e->proxy_evt.closed = closed_evt;
    e->proxy_evt.recvdatafrom = recvfrom_evt;
    e->proxy_evt.recvdata = recv_evt;

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;
    e->t = mrp_transport_create(e->ml, e->stype, &e->proxy_evt, e, flags);

    if (e->t == NULL) {
        signalling_error("Error: Failed to create listening socket transport.");
        goto error;
    }

    if (!mrp_transport_bind(e->t, &e->addr, e->addrlen)) {
        signalling_error("Error: Failed to bind to address %s.", e->address);
        goto error;
    }

    if (!mrp_transport_listen(e->t, 4)) {
        signalling_error("Error: Failed to listen on server transport (%s).",
                e->address);
        goto error;
    }

    return 0;

error:
    return -1;
}


mrp_transport_t *accept_connection(endpoint_t *e, mrp_transport_t *lt,
            void *client)
{
    int flags = 0;
    endpoint_tport_t *et;
    mrp_transport_t *t;

    et = mrp_allocz(sizeof(endpoint_tport_t));
    et->e = e;
    et->client = client;
    mrp_list_init(&et->hook);

    switch (e->type) {
        default:
            flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        break;
    }

    t = mrp_transport_accept(lt, et, flags);

    if (!t)
        goto error;

    mrp_list_append(&e->clients, &et->hook);

    return t;

error:
    mrp_free(et);
    return NULL;
}


void delete_endpoint(endpoint_t *e)
{
    mrp_list_hook_t *p, *n;
    endpoint_tport_t *et;

    mrp_transport_disconnect(e->t);
    mrp_transport_destroy(e->t);

    /* go through the client data list and free */

    mrp_list_foreach(&e->clients, p, n) {
        et = mrp_list_entry(p, typeof(*et), hook);
        mrp_list_delete(&et->hook);
        mrp_free(et);
    }

    mrp_free(e->address);
    mrp_free(e);
}
