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

mrp_plugin_t *signalling_plugin;

enum {
    ARG_ADDRESS /* signalling socket address, 'address:port' */
};


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
    mrp_htbl_config_t client_conf, tx_conf;

    signalling_info("> init()");

    if ((data = mrp_allocz(sizeof(*data))) == NULL)
        return FALSE;

    data->ctx     = plugin->ctx;
    data->address = plugin->args[ARG_ADDRESS].str;

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

    /* we only support unix domain sockets for the time being */

    if (!strncmp(data->address, "unxs:", 5)) {
        data->path = data->address + 5;
        if (socket_setup(data)) {
            plugin->data = data;
            signalling_info("set up at address '%s'.", data->address);

            signalling_plugin = plugin;

            return TRUE;
        }
    }

    mrp_free(data);

    signalling_error("failed to set up signalling at address '%s'.",
            plugin->args[ARG_ADDRESS].str);

    return FALSE;
}


static void signalling_exit(mrp_plugin_t *plugin)
{
    data_t *ctx = plugin->data;

    signalling_info("cleaning up instance '%s'...", plugin->instance);

    /* FIXME: call error callbacks of all active transactions? */

    mrp_htbl_destroy(ctx->clients, TRUE);
    mrp_htbl_destroy(ctx->txs, TRUE);
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
        signalling_init, signalling_exit, signalling_args, NULL);
