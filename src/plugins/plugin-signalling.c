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
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/core.h>

#include "signalling/plugin.h"
#include "signalling/transaction.h"
#include "signalling/client.h"
#include "signalling/util.h"
#include "signalling/endpoint.h"
#include "signalling/info.h"

mrp_plugin_t *signalling_plugin;

enum {
    ARG_ADDRESS /* signalling socket address, 'addr1:port;addr2:port' */
};


static void htbl_free_backchannel(void *key, void *object)
{
    MRP_UNUSED(key);
    free_backchannel(object);
}


static void htbl_free_client(void *key, void *object)
{
    MRP_UNUSED(key);
    free_client(object);
}


static void htbl_free_transaction(void *key, void *object)
{
    MRP_UNUSED(key);
    free_transaction(object);
}


static int signalling_init(mrp_plugin_t *plugin)
{
    data_t *data;
    mrp_htbl_config_t client_conf, tx_conf, backchannel_conf;
    char *address;
    int len = strlen(plugin->args[ARG_ADDRESS].str);
    char buf[len+1];
    char *tmp = buf;

    signalling_info("> init()");

    if ((data = mrp_allocz(sizeof(*data))) == NULL)
        return FALSE;

    data->ctx = plugin->ctx;

    type_init();

    client_conf.comp = mrp_string_comp;
    client_conf.hash = mrp_string_hash;
    client_conf.free = htbl_free_client;
    client_conf.nbucket = 0;
    client_conf.nentry = 10;

    data->clients = mrp_htbl_create(&client_conf);

    tx_conf.comp = int_comp;
    tx_conf.hash = int_hash;
    tx_conf.free = htbl_free_transaction;
    tx_conf.nbucket = 0;
    tx_conf.nentry = 5;

    data->txs = mrp_htbl_create(&tx_conf);

    backchannel_conf.comp = mrp_string_comp;
    backchannel_conf.hash = mrp_string_hash;
    backchannel_conf.free = htbl_free_backchannel;
    backchannel_conf.nbucket = 0;
    backchannel_conf.nentry = 5;

    data->backchannels = mrp_htbl_create(&backchannel_conf);

    /* parse here the address line */

    mrp_list_init(&data->es);

    memcpy(buf, plugin->args[ARG_ADDRESS].str, len);
    buf[len] = '\0';

    signalling_info("address config: '%s'", tmp);

    do {
        address = strtok(tmp, ";");
        if (address) {
            endpoint_t *e;
            signalling_info("address: '%s'", address);
            e = create_endpoint(address, plugin->ctx->ml);
            if (!e) {
                goto error;
            }
            clean_endpoint(e);
            if (server_setup(e, data) < 0) {
                goto error;
            }
            mrp_list_append(&data->es, &e->hook);
        }
        tmp = NULL;
    } while (address);

    plugin->data = data;
    signalling_plugin = plugin;

    return TRUE;

error:
    mrp_free(data);
    signalling_error("failed to set up signalling at address '%s'.",
            plugin->args[ARG_ADDRESS].str);

    signalling_plugin = NULL;

    return FALSE;
}


static void signalling_exit(mrp_plugin_t *plugin)
{
    data_t *ctx = plugin->data;
    mrp_list_hook_t *p, *n;
    endpoint_t *e;

    signalling_info("cleaning up instance '%s'...", plugin->instance);

    /* go through the client data list */

    mrp_list_foreach(&ctx->es, p, n) {
        e = mrp_list_entry(p, typeof(*e), hook);
        mrp_list_delete(&e->hook);
        delete_endpoint(e);
    }

    mrp_htbl_destroy(ctx->clients, TRUE);
    mrp_htbl_destroy(ctx->txs, TRUE);
    mrp_htbl_destroy(ctx->backchannels, TRUE);
}


#define SIGNALLING_DESCRIPTION "A decision signalling plugin for Murphy."
#define SIGNALLING_HELP \
    "The signalling plugin provides one-to-many communication from Murphy\n"  \
    "to enforcement points. The enforcement points are supposed to use\n"     \
    "libsignalling to initialize connection to Murphy and receive events\n"   \
    "from it."

#define SIGNALLING_VERSION MRP_VERSION_INT(0, 0, 1)
#define SIGNALLING_AUTHORS "Ismo Puustinen <ismo.puustinen@intel.com>"


static mrp_plugin_arg_t signalling_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS, STRING, "address", "unxs:/tmp/murphy/signalling"),
};

MURPHY_REGISTER_CORE_PLUGIN("signalling",
        SIGNALLING_VERSION, SIGNALLING_DESCRIPTION,
        SIGNALLING_AUTHORS, SIGNALLING_HELP, MRP_SINGLETON,
        signalling_init, signalling_exit, signalling_args,
        MRP_ARRAY_SIZE(signalling_args), NULL, 0, NULL, 0, NULL);
