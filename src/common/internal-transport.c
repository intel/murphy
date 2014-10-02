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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <murphy/common.h>

#define INTERNAL "internal"

typedef struct internal_s internal_t;

struct internal_s {
    MRP_TRANSPORT_PUBLIC_FIELDS; /* common transport fields */
    char name[MRP_SOCKADDR_SIZE]; /* bound connection name */
    mrp_sockaddr_t address; /* internal connection name*/
    bool active;
    bool bound;
    bool listening;

    internal_t *endpoint; /* that we are connected to */
};

typedef struct {
    void *data;
    size_t size;
    internal_t *u;
    mrp_sockaddr_t *addr;
    socklen_t addrlen;
    bool free_data;
    int offset;
    bool custom;
    uint16_t tag;

    mrp_list_hook_t hook;
} internal_message_t;


/* storage for the global data, TODO: refactor away */
static mrp_htbl_t *servers = NULL;
static mrp_htbl_t *connections = NULL;
static mrp_list_hook_t msg_queue;
static mrp_deferred_t *d;
static uint32_t cid;


static void process_queue(mrp_deferred_t *d, void *user_data)
{
    internal_message_t *msg;
    internal_t *endpoint;
    mrp_list_hook_t *p, *n;

    MRP_UNUSED(user_data);

    mrp_disable_deferred(d);

    mrp_list_foreach(&msg_queue, p, n) {

        msg = mrp_list_entry(p, typeof(*msg), hook);

        if (!msg) {
            mrp_log_error("no message!");
            goto end;
        }

        if (!msg->u->connected) {
            if (!msg->addr) {
                mrp_log_error("connected transport without address!");
                goto end;
            }

            /* Find the recipient. Look first from the server table.*/
            endpoint = mrp_htbl_lookup(servers, msg->addr->data);

            if (!endpoint) {

                /* Look next from the general connections table. */
                endpoint = mrp_htbl_lookup(connections, msg->addr->data);
            }
        }
        else {
            endpoint = msg->u->endpoint;
        }

        if (!endpoint || !endpoint->recv_data) {
            mrp_log_error("no endpoint matching the address");
            goto end;
        }

        /* skip the length word when sending */
        endpoint->recv_data(
                (mrp_transport_t *) endpoint, msg->data + msg->offset,
                msg->size, &msg->u->address, MRP_SOCKADDR_SIZE);

end:
        if (msg) {

            if (msg->free_data) {
                if (msg->custom) {
                    /* FIXME: should be mrp_data_free(msg->data, msg->tag); */
                    mrp_free(msg->data);
                }
                else
                    mrp_msg_unref(msg->data);
            }

            mrp_list_delete(&msg->hook);
            mrp_free(msg);
        }
    }
}


static int internal_initialize_table(internal_t *u)
{
    mrp_htbl_config_t servers_conf;
    mrp_htbl_config_t connections_conf;

    MRP_UNUSED(u);

    if (servers && connections && d)
        return 0; /* already initialized */

    servers_conf.comp = mrp_string_comp;
    servers_conf.hash = mrp_string_hash;
    servers_conf.free = NULL;
    servers_conf.nbucket = 0;
    servers_conf.nentry = 10;

    servers = mrp_htbl_create(&servers_conf);

    if (!servers)
        goto error;

    connections_conf.comp = mrp_string_comp;
    connections_conf.hash = mrp_string_hash;
    connections_conf.free = NULL;
    connections_conf.nbucket = 0;
    connections_conf.nentry = 10;

    connections = mrp_htbl_create(&connections_conf);

    if (!connections)
        goto error;

    mrp_list_init(&msg_queue);

    cid = 0;

    d = mrp_add_deferred(u->ml, process_queue, NULL);

    if (!d)
        goto error;

    mrp_disable_deferred(d);

    return 0;

error:

    if (servers)
        mrp_htbl_destroy(servers, FALSE);

    servers = NULL;

    if (connections)
        mrp_htbl_destroy(connections, FALSE);

    connections = NULL;

    return -1;
}


static socklen_t internal_resolve(const char *str, mrp_sockaddr_t *addr,
                              socklen_t size, const char **typep)
{
    int len;

    MRP_UNUSED(size);

    if (!str)
        return 0;

    len = strlen(str);

    if (len <= 9 || len >= MRP_SOCKADDR_SIZE)
        return 0;

    if (strncmp("internal:", str, 9))
        return 0;

    if (typep)
        *typep = INTERNAL;

    memcpy(addr->data, str+9, len-9+1);

    return len-9;
}


static int internal_open(mrp_transport_t *mu)
{
    internal_t *u = (internal_t *)mu;

    if (internal_initialize_table(u) < 0)
        return FALSE;

    memset(u->name, 0, MRP_SOCKADDR_SIZE);
    memset(u->address.data, 0, MRP_SOCKADDR_SIZE);

    u->active = FALSE;

    snprintf(u->address.data, MRP_SOCKADDR_SIZE, INTERNAL"_%d", cid++);

    mrp_htbl_insert(connections, u->address.data, mu);

    return TRUE;
}


static int internal_bind(mrp_transport_t *mu, mrp_sockaddr_t *addr,
                     socklen_t addrlen)
{
    internal_t *u = (internal_t *)mu;

    if (internal_initialize_table(u) < 0)
        return FALSE;

    memcpy(u->name, addr->data, addrlen+1);

    mrp_htbl_insert(servers, u->name, u);

    u->active = TRUE;
    u->bound = TRUE;

    return TRUE;
}


static int internal_listen(mrp_transport_t *mu, int backlog)
{
    internal_t *u = (internal_t *)mu;

    MRP_UNUSED(backlog);

    if (!u->bound)
        return FALSE;

    u->listening = TRUE;

    return TRUE;
}


static int internal_accept(mrp_transport_t *mt, mrp_transport_t *mlt)
{
    internal_t *t = (internal_t *) mt;
    internal_t *lt = (internal_t *) mlt;
    internal_t *client = lt->endpoint;

    t->endpoint = client;
    client->endpoint = t;

    lt->endpoint = NULL; /* connection process is now over */

    return TRUE;
}


static void remove_messages(internal_t *u)
{
    internal_message_t *msg;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&msg_queue, p, n) {
        msg = mrp_list_entry(p, typeof(*msg), hook);

        if (strcmp(msg->addr->data, u->name) == 0
            || strcmp(msg->addr->data, u->address.data) == 0) {

            if (msg->free_data) {
                if (msg->custom) {
                    /* FIXME: should be mrp_data_free(msg->data, msg->tag); */
                    mrp_free(msg->data);
                }
                else
                    mrp_msg_unref(msg->data);
            }

            mrp_list_delete(&msg->hook);
            mrp_free(msg);
        }
    }
}


static void internal_close(mrp_transport_t *mu)
{
    internal_t *u = (internal_t *)mu;

    /* Is this client or server? If server, go remove the connection from
     * servers table. */

    if (u->bound) {
        /* server listening socket */
        mrp_htbl_remove(servers, u->name, FALSE);
        u->bound = FALSE;
    }

    mrp_htbl_remove(connections, u->address.data, FALSE);

    u->active = FALSE;

    remove_messages(u);
}


static int internal_connect(mrp_transport_t *mu, mrp_sockaddr_t *addr,
                        socklen_t addrlen)
{
    internal_t *u = (internal_t *)mu;
    internal_t *host;
    mrp_transport_t *mt;

    MRP_UNUSED(addrlen);

    /* client connecting */

    if (!servers) {
        mrp_log_error("no servers available for connecting");
        return FALSE;
    }

    host = mrp_htbl_lookup(servers, addr->data);

    if (!host) {
        mrp_log_error("server '%s' wasn't found", addr->data);
        return FALSE;
    }

    mt = (mrp_transport_t *) host;

    host->endpoint = u; /* temporary connection data */

    host->evt.connection(mt, mt->user_data);

    return TRUE;
}


static int internal_disconnect(mrp_transport_t *mu)
{
    internal_t *u = (internal_t *)mu;

    if (u->connected) {
        internal_t *endpoint = u->endpoint;

        if (endpoint) {
            endpoint->endpoint = NULL;
            mrp_transport_disconnect((mrp_transport_t *) endpoint);
        }
        u->endpoint = NULL;
    }

    return TRUE;
}


static int internal_sendto(mrp_transport_t *mu, mrp_msg_t *data,
                       mrp_sockaddr_t *addr, socklen_t addrlen)
{
    internal_t *u = (internal_t *)mu;
    void *buf;
    size_t size;
    internal_message_t *msg;

    size = mrp_msg_default_encode(data, &buf);

    if (size == 0 || buf == NULL) {
        return FALSE;
    }

    msg = mrp_allocz(sizeof(internal_message_t));

    if (!msg)
        return FALSE;

    msg->addr = addr;
    msg->addrlen = addrlen;
    msg->data = buf;
    msg->free_data = FALSE;
    msg->offset = 0;
    msg->size = size;
    msg->u = u;
    msg->custom = FALSE;

    mrp_list_init(&msg->hook);
    mrp_list_append(&msg_queue, &msg->hook);

    mrp_enable_deferred(d);

    return TRUE;
}


static int internal_send(mrp_transport_t *mu, mrp_msg_t *msg)
{
    if (!mu->connected) {
        return FALSE;
    }

    return internal_sendto(mu, msg, NULL, 0);
}


static int internal_sendrawto(mrp_transport_t *mu, void *data, size_t size,
                          mrp_sockaddr_t *addr, socklen_t addrlen)
{
    internal_t *u = (internal_t *)mu;
    internal_message_t *msg;

    msg = mrp_allocz(sizeof(internal_message_t));

    if (!msg)
        return FALSE;

    msg->addr = addr;
    msg->addrlen = addrlen;
    msg->data = data;
    msg->free_data = FALSE;
    msg->offset = 0;
    msg->size = size;
    msg->u = u;
    msg->custom = FALSE;

    mrp_list_init(&msg->hook);
    mrp_list_append(&msg_queue, &msg->hook);

    mrp_enable_deferred(d);

    return TRUE;
}


static int internal_sendraw(mrp_transport_t *mu, void *data, size_t size)
{
    if (!mu->connected) {
        return FALSE;
    }

    return internal_sendrawto(mu, data, size, NULL, 0);
}


static size_t encode_custom_data(void *data, void **newdata, uint16_t tag)
{
    mrp_data_descr_t *type = mrp_msg_find_type(tag);
    uint32_t *lenp;
    uint16_t *tagp;
    size_t reserve, size;
    int len;
    void *buf;

    if (type == NULL) {
        mrp_log_error("type not found!");
        return 0;
    }

    reserve = sizeof(*lenp) + sizeof(*tagp);
    size = mrp_data_encode(&buf, data, type, reserve);

    if (size == 0) {
        mrp_log_error("data encoding failed");
        return 0;
    }

    /* some format conversion */

    lenp = buf;
    len = size - sizeof(*lenp);
    tagp = buf + sizeof(*lenp);

    *tagp = htobe16(tag);

    *newdata = buf;

    return len;
}


static int internal_senddatato(mrp_transport_t *mu, void *data, uint16_t tag,
                           mrp_sockaddr_t *addr, socklen_t addrlen)
{
    internal_t *u = (internal_t *)mu;
    mrp_data_descr_t *type = mrp_msg_find_type(tag);
    void *newdata = NULL;
    size_t size;
    internal_message_t *msg;

    if (type == NULL)
        return FALSE;

    msg = mrp_allocz(sizeof(internal_message_t));

    if (!msg)
        return FALSE;

    size = encode_custom_data(data, &newdata, tag);

    if (!newdata) {
        mrp_log_error("custom data encoding failed");
        mrp_free(msg);
        return FALSE;
    }

    msg->addr = addr;
    msg->addrlen = addrlen;
    msg->data = newdata;
    msg->free_data = TRUE;
    msg->offset = 4;
    msg->size = size;
    msg->u = u;
    msg->custom = TRUE;
    msg->tag = tag;

    mrp_list_init(&msg->hook);
    mrp_list_append(&msg_queue, &msg->hook);

    mrp_enable_deferred(d);

    return TRUE;
}


static int internal_senddata(mrp_transport_t *mu, void *data, uint16_t tag)
{
    if (!mu->connected) {
        return FALSE;
    }

    return internal_senddatato(mu, data, tag, NULL, 0);
}




MRP_REGISTER_TRANSPORT(internal, INTERNAL, internal_t, internal_resolve,
                       internal_open, NULL, internal_close, NULL,
                       internal_bind, internal_listen, internal_accept,
                       internal_connect, internal_disconnect,
                       internal_send, internal_sendto,
                       internal_sendraw, internal_sendrawto,
                       internal_senddata, internal_senddatato,
                       NULL, NULL,
                       NULL, NULL,
                       NULL, NULL);
