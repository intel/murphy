/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/transport.h>
#include <murphy/common/wsck-transport.h>
#include <murphy/common/json.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>

#define DEFAULT_ADDRESS "wsck:127.0.0.1:4000/murphy"


/*
 * plugin argument indices
 */

enum {
    ARG_ADDRESS,                         /* transport address to use */
};


/*
 * WRT resource context
 */

typedef struct {
    mrp_context_t   *ctx;                /* murphy context */
    mrp_transport_t *lt;                 /* transport we listen on */
    const char      *addr;               /* address we listen on */
    mrp_list_hook_t  clients;            /* connected clients */
} wrt_data_t;


typedef struct {
    mrp_context_t   *ctx;                /* murphy context */
    mrp_transport_t *t;                  /* client transport */
    mrp_list_hook_t  hook;               /* to list of clients */
} wrt_client_t;


static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    wrt_data_t   *data = (wrt_data_t *)user_data;
    wrt_client_t *c;
    int           flags;

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        mrp_list_init(&c->hook);

        flags = MRP_TRANSPORT_REUSEADDR;
        c->t  = mrp_transport_accept(lt, c, flags);

        if (c->t != NULL) {
            mrp_list_append(&data->clients, &c->hook);
            mrp_log_info("Accepted WRT resource client connection.");
            return;
        }

        mrp_free(c);
    }

    mrp_log_error("Failed to accept WRT resource client connection.");
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    wrt_client_t *c = (wrt_client_t *)user_data;

    if (error != 0)
        mrp_log_error("WRT resource connection closed with error %d (%s).",
                      error, strerror(error));
    else
        mrp_log_info("WRT resource connection closed.");

    mrp_list_delete(&c->hook);

    mrp_transport_disconnect(t);
    mrp_transport_destroy(t);
    c->t = NULL;

    mrp_free(c);
}


static void recv_evt(mrp_transport_t *t, void *data, size_t size,
                     void *user_data)
{
    wrt_client_t *c = (wrt_client_t *)user_data;

    MRP_UNUSED(c);

    mrp_log_info("recived WRT resource message [%*.*s]",
                 (int)size, (int)size, (char *)data);

    mrp_transport_sendraw(t, "Aye-aye, captain!", 17);
}


static int transport_create(wrt_data_t *data)
{
    static mrp_transport_evt_t evt = {
        { .recvraw     = recv_evt },
        { .recvrawfrom = NULL     },
        .connection    = connection_evt,
        .closed        = closed_evt,
    };

    mrp_mainloop_t *ml = data->ctx->ml;
    mrp_sockaddr_t  addr;
    socklen_t       len;
    const char     *type, *opt, *val;
    int             flags;

    len = mrp_transport_resolve(NULL, data->addr, &addr, sizeof(addr), &type);

    if (len > 0) {
        /*                                   MRP_TRANSPORT_MODE_JSON */
        flags    = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_RAW;
        data->lt = mrp_transport_create(ml, type, &evt, data, flags);

        if (data->lt != NULL) {
            if (mrp_transport_bind(data->lt, &addr, len) &&
                mrp_transport_listen(data->lt, 0)) {
                mrp_log_info("Listening on transport '%s'...", data->addr);

                opt = MRP_WSCK_OPT_SENDMODE;
                val = MRP_WSCK_SENDMODE_TEXT;
                mrp_transport_setopt(data->lt, opt, val);

                return TRUE;
            }

            mrp_transport_destroy(data->lt);
            data->lt = NULL;
        }
    }
    else
        mrp_log_error("Failed to resolve transport address '%s'.", data->addr);

    return FALSE;
}


static void transport_destroy(wrt_data_t *data)
{
    mrp_transport_destroy(data->lt);
}


static int plugin_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    const char       *addr = args[ARG_ADDRESS].str;
    wrt_data_t       *data;

    data = mrp_allocz(sizeof(*data));

    if (data != NULL) {
        mrp_list_init(&data->clients);

        data->ctx  = plugin->ctx;
        data->addr = addr;

        if (!transport_create(data))
            goto fail;

        return TRUE;
    }


 fail:
    if (data != NULL) {
        transport_destroy(data);

        mrp_free(data);
    }

    return FALSE;
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    wrt_data_t *data = (wrt_data_t *)plugin->data;

    transport_destroy(data);

    mrp_free(data);
}


#define PLUGIN_DESCRIPTION "Murphy resource Web runtime bridge plugin."
#define PLUGIN_HELP        "Expose the Murphy resource protocol to WRTs."
#define PLUGIN_AUTHORS     "Krisztian Litkey <kli@iki.fi>"
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)

static mrp_plugin_arg_t plugin_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS, STRING, "address", DEFAULT_ADDRESS)
};


MURPHY_REGISTER_PLUGIN("resource-wrt",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION, PLUGIN_AUTHORS,
                       PLUGIN_HELP, MRP_SINGLETON, plugin_init, plugin_exit,
                       plugin_args, MRP_ARRAY_SIZE(plugin_args),
                       NULL, 0,
                       NULL, 0,
                       NULL);
