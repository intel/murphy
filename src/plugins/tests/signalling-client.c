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
#include <murphy/plugins/signalling/signalling-protocol.h>

#define _GNU_SOURCE
#include <getopt.h>

#define MAX_DOMAINS 32

typedef struct {
    mrp_transport_t *t;
    mrp_mainloop_t *ml;

    char *name;
    char *info;
    char *domains[MAX_DOMAINS];
    uint n_domains;
    bool ack;
} client_t;


static void dump_decision(client_t *c, ep_decision_t *msg)
{
    uint i;

    MRP_UNUSED(c);

    printf("Message contents:\n");
    for (i = 0; i < msg->n_rows; i++) {
        printf("row %d: '%s'\n", i+1, msg->rows[i]);
    }
    printf("%s required.\n\n", msg->reply_required ? "Reply" : "No reply");
}


static int send_info(client_t *c, char *data)
{
    ep_info_t msg;
    int ret;

    printf("sending info message '%s'\n", data);

    msg.msg = data;

    ret = mrp_transport_senddata(c->t, &msg, TAG_INFO);

    if (!ret) {
        printf("failed to send info message\n");
    }

    return ret;
}


static int send_registration(client_t *c)
{
    ep_register_t msg;
    int ret;

    msg.ep_name = c->name;
    msg.domains = c->domains;
    msg.n_domains = c->n_domains;

    ret = mrp_transport_senddata(c->t, &msg, TAG_REGISTER);

    if (!ret) {
        printf("failed to send register message\n");
    }

    return ret;
}


static int send_reply(client_t *c, ep_decision_t *msg, uint32_t success)
{
    ep_ack_t reply;
    int ret;

    reply.id = msg->id;
    reply.success = success;

    ret = mrp_transport_senddata(c->t, &reply, TAG_ACK);

    if (!ret) {
        printf("failed to send reply\n");
    }

    return ret;
}


static void handle_decision(client_t *c, ep_decision_t *msg)
{
    printf("Handle decision\n");

    dump_decision(c, msg);

    if (msg->reply_required)
        send_reply(c, msg, c->ack ? EP_ACK: EP_NACK);

    /* try sending an info signal here */
    if (c->info) {
        send_info(c, c->info);
    }
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    client_t *c = user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(error);

    printf("Received closed event\n");

    mrp_mainloop_quit(c->ml, 0);
}


static void recvfrom_evt(mrp_transport_t *t, void *data, uint16_t tag,
             mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    client_t *c = user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);
    MRP_UNUSED(user_data);
    MRP_UNUSED(t);

    printf("Received message (0x%02x)\n", tag);

    switch (tag) {
        case TAG_POLICY_DECISION:
            handle_decision(c, data);
            break;
        case TAG_ERROR:
            printf("Server sends an error message!\n");
            break;
        default:
            /* no other messages supported ATM */
            break;
    }
}


static void recv_evt(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvfrom_evt(t, data, tag, NULL, 0, user_data);
}


static void print_usage(const char *argv0)
{
    printf("usage: %s -i <id> [options]\n\n"
           "The possible options are:\n"
           "  -n, --nack                     send NACKs instead of ACKs\n"
           "  -d, --domain                   specify a policy domain\n"
           "  -h, --help                     show help on usage\n",
           argv0);
}


static int add_domain(client_t *c, char *domain)
{
     if (c->n_domains >= MAX_DOMAINS)
        return -1;

    c->domains[c->n_domains++] = mrp_strdup(domain);

    return 0;
}


static int parse_cmdline(client_t *c, int argc, char **argv)
{
#   define OPTIONS "nd:i:I:h"
    struct option options[] = {
        { "nack"  , no_argument      , NULL, 'n' },
        { "domain", required_argument, NULL, 'd' },
        { "id"    , required_argument, NULL, 'i' },
        { "info"  , required_argument, NULL, 'I' },
        { "help"  , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'n':
            c->ack = FALSE;
            break;

        case 'd':
            if (add_domain(c, optarg) < 0) {
                return FALSE;
            }
            break;

        case 'i':
            c->name = mrp_strdup(optarg);
            break;

        case 'I':
            c->info = mrp_strdup(optarg);
            break;

        case 'h':
            print_usage(argv[0]);
            exit(0);
            break;

        default:
            print_usage(argv[0]);
            return FALSE;
        }
    }

    return TRUE;
}


static void free_client(client_t *c)
{
    /* TODO: delete the transport */

    for (; c->n_domains > 0; c->n_domains--)
        mrp_free(c->domains[c->n_domains-1]);

    mrp_free(c->info);
    mrp_free(c->name);
}


int main(int argc, char **argv)
{
    socklen_t alen;
    mrp_sockaddr_t addr;
    int ret, flags;

    client_t client;
    static mrp_transport_evt_t evt; /* static members are initialized to zero */

    client.name = NULL;
    client.info = NULL;
    client.n_domains = 0;
    client.ack = TRUE;

    if (!parse_cmdline(&client, argc, argv)) {
        goto error;
    }

    if (!client.name) {
        printf("Error: 'id' is a mandatory argument!\n");
        print_usage(argv[0]);
        goto error;
    }

    evt.closed = closed_evt;
    evt.recvdatafrom = recvfrom_evt;
    evt.recvdata = recv_evt;

    if (!mrp_msg_register_type(&ep_register_descr) ||
        !mrp_msg_register_type(&ep_decision_descr) ||
        !mrp_msg_register_type(&ep_ack_descr) ||
        !mrp_msg_register_type(&ep_info_descr)) {
        printf("Error: registering data types failed!\n");
        goto error;
    }

    client.ml = mrp_mainloop_create();

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;

    client.t = mrp_transport_create(client.ml, "unxs", &evt, &client, flags);
    if (client.t == NULL) {
        printf("Error: creating a new transport failed!\n");
        goto error;
    }

    alen = mrp_transport_resolve(NULL, "unxs:/tmp/murphy/signalling", &addr, sizeof(addr), NULL);
    if (alen <= 0) {
        printf("Error: resolving address failed!\n");
        goto error;
    }


    ret = mrp_transport_connect(client.t, &addr, alen);
    if (ret == 0) {
        printf("Error: connect failed!\n");
        goto error;
    }

    send_registration(&client);

    mrp_mainloop_run(client.ml);

    free_client(&client);

    return 0;

error:
    free_client(&client);
    return 1;
}
