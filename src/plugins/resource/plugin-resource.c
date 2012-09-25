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
#include <string.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>
#include <murphy/core/plugin.h>
#include <murphy/core/console.h>
#include <murphy/core/event.h>

#include <murphy-db/mql.h>
#include <murphy-db/mqi.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>
#include <murphy/resource/manager-api.h>

enum {
    ARG_CONFIG_FILE,
    ARG_ADDRESS,
};

typedef struct {
    mrp_plugin_t      *plugin;
    mrp_event_watch_t *w;
    mrp_sockaddr_t     saddr;
    socklen_t          alen;
    const char        *atyp;
    mrp_transport_t   *listen;
    mrp_list_hook_t    clients;
} resource_data_t;

typedef struct {
    mrp_list_hook_t        list;
    resource_data_t       *data;
    uint32_t               id;
    mrp_resource_client_t *rscli;
    mrp_transport_t       *transp;
} client_t;


void print_zones_cb(mrp_console_t *c,void *user_data,int argc, char **argv);
void print_classes_cb(mrp_console_t *c,void *user_data,int argc,char **argv);
void print_sets_cb(mrp_console_t *c,void *user_data,int argc,char **argv);
void print_owners_cb(mrp_console_t *c,void *user_data,int argc,char **argv);


MRP_CONSOLE_GROUP(resource_group, "resource", NULL, NULL, {
        MRP_TOKENIZED_CMD("zones"  , print_zones_cb, FALSE,
                          "zones", "prints zones",
                          "prints the available zones. The data sources "
                          "for the printout are the internal data structures "
                          "of the resource library."),
        MRP_TOKENIZED_CMD("classes"  , print_classes_cb, FALSE,
                          "classes", "prints application classes",
                          "prints the available application classes. The "
                          "data sources for the printout are the internal "
                          "data structures of the resource library."),
        MRP_TOKENIZED_CMD("sets", print_sets_cb, FALSE,
                          "sets", "prints resource sets",
                          "prints the current resource sets for each "
                          "application class. The data sources for the "
                          "printout are the internal data structures of the "
                          "resource library"),
        MRP_TOKENIZED_CMD("owners" , print_owners_cb , TRUE,
                          "owners", "prints resource owners",
                          "prints for each zone the owner application class "
                          "of each resource. The data sources for the "
                          "printout are the internal data structures of the "
                          "resource library")

});


void print_zones_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    const char **zone_names;
    int i;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_console_printf(c, "Zones:\n");

    if ((zone_names = mrp_zone_get_all_names(0, NULL))) {

        for (i = 0;  zone_names[i];  i++)
            mrp_console_printf (c, "   %s\n", zone_names[i]);


        mrp_free(zone_names);
    }
}


void print_classes_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    const char **class_names;
    int i;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_console_printf(c, "Application classes:\n");

    if ((class_names = mrp_application_class_get_all_names(0, NULL))) {

        for (i = 0;  class_names[i];  i++)
            mrp_console_printf(c, "   %s\n", class_names[i]);


        mrp_free(class_names);
    }
}


void print_sets_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    char buf[8192];

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_application_class_print(buf, sizeof(buf));

    mrp_console_printf(c, "%s", buf);
}


void print_owners_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    char buf[2048];

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_resource_owner_print(buf, sizeof(buf));

    mrp_console_printf(c, "%s", buf);
}

int set_default_configuration(void)
{
    typedef struct {
        const char     *name;
        bool            share;
        mrp_attr_def_t *attrs;
    } resdef_t;

    static const char *zones[] = {
        "driver",
        "front-passenger",
        "rear-left-passenger",
        "rear-right-passenger",
        NULL
    };

    static const char *classes[] = {
        "implicit",
        "player",
        "game",
        "phone",
        "navigator",
        NULL
    };

    static mrp_attr_def_t audio_attrs[] = {
        { "role", MRP_RESOURCE_RW, mqi_string , .value.string="music" },
        {  NULL ,        0       , mqi_unknown, .value.string=NULL    }
    };

    static resdef_t  resources[] = {
        { "audio_playback" , true , audio_attrs  },
        { "audio_recording", true , NULL         },
        { "video_playback" , false, NULL         },
        { "video_recording", false, NULL         },
        {      NULL        , false, NULL         }
    };

    const char *name;
    resdef_t *rdef;
    uint32_t i;

    mrp_zone_definition_create(NULL);

    for (i = 0;  (name = zones[i]);  i++)
        mrp_zone_create(name, NULL);

    for (i = 0;  (name = classes[i]); i++)
        mrp_application_class_create(name, i);

    for (i = 0;  (rdef = resources + i)->name;  i++) {
        mrp_resource_definition_create(rdef->name, rdef->share, rdef->attrs,
                                       NULL, NULL);
    }

    return 0;
}


static void connection_evt(mrp_transport_t *listen, void *user_data)
{
    static uint32_t  id;

    resource_data_t *data   = (resource_data_t *)user_data;
    mrp_plugin_t    *plugin = data->plugin;
    int              flags  = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
    client_t        *client = mrp_allocz(sizeof(client_t));
    char             name[256];

    if (!client) {
        mrp_log_error("%s: Memory alloc error. Can't accept new connection",
                      plugin->instance);
        return;
    }

    client->data = data;

    snprintf(name, sizeof(name), "client%u", (client->id = ++id));
    client->rscli = mrp_resource_client_create(name, client);

    if (!(client->transp = mrp_transport_accept(listen, client, flags))) {
        mrp_log_error("%s: failed to accept new connection", plugin->instance);
        mrp_resource_client_destroy(client->rscli);
        mrp_free(client);
        return;
    }

    mrp_list_append(&data->clients, &client->list);

    mrp_log_warning("%s: %s connected", plugin->instance, name);
}

void closed_evt(mrp_transport_t *transp, int error, void *user_data)
{
    client_t        *client = (client_t *)user_data;
    resource_data_t *data   = client->data;
    mrp_plugin_t    *plugin = data->plugin;

    MRP_UNUSED(transp);

    if (error)
        mrp_log_error("%s: connection error %d (%s)",
                      plugin->instance, error, strerror(error));
    else
        mrp_log_warning("%s: peer closed connection", plugin->instance);

    mrp_resource_client_destroy(client->rscli);

    mrp_list_delete(&client->list);
    mrp_free(client);
}



static void recvfrom_msg(mrp_transport_t *transp, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    resource_data_t *data = (resource_data_t *)user_data;

    mrp_log_warning("%s: received a message", data->plugin->instance);
    //mrp_dump_msg(msg, stdout);
}

static void recv_msg(mrp_transport_t *transp, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(transp, msg, NULL, 0, user_data);
}



static int initiate_transport(mrp_plugin_t *plugin)
{
    static mrp_transport_evt_t evt = {
        { .recvmsg = recv_msg },
        { .recvmsgfrom = recvfrom_msg },
        .closed = NULL,
        .connection = NULL
    };

    mrp_context_t    *ctx   = plugin->ctx;
    mrp_plugin_arg_t *args  = plugin->args;
    resource_data_t  *data  = (resource_data_t *)plugin->data;
    const char       *addr  = args[ARG_ADDRESS].str;
    int               flags = MRP_TRANSPORT_REUSEADDR;
    bool              stream;

    //register_messages(data);

    data->alen = mrp_transport_resolve(NULL, addr, &data->saddr,
                                       sizeof(data->saddr), &data->atyp);

    if (data->alen <= 0) {
        mrp_log_error("%s: failed to resolve transport arddress '%s'",
                      plugin->instance, addr);
        return -1;
    }


    if (strncmp(addr, "tcp", 3) && strncmp(addr, "unxs", 4))
        stream = false;
    else {
        stream = true;
        evt.connection = connection_evt;
        evt.closed = closed_evt;
    }

    data->listen = mrp_transport_create(ctx->ml, data->atyp, &evt, data,flags);

    if (!data->listen) {
        mrp_log_error("%s: can't create listening transport",plugin->instance);
        return -1;
    }

    if (!mrp_transport_bind(data->listen, &data->saddr, data->alen)) {
        mrp_log_error("%s: can't bind to address %s", plugin->instance, addr);
        return -1;
    }

    if (stream && !mrp_transport_listen(data->listen, 0)) {
        mrp_log_error("%s: can't listen for connections", plugin->instance);
        return -1;
    }

    return 0;
}


static void event_cb(mrp_event_watch_t *w, int id, mrp_msg_t *event_data,
                     void *user_data)
{
    mrp_plugin_t     *plugin = (mrp_plugin_t *)user_data;
    mrp_plugin_arg_t *args   = plugin->args;
    resource_data_t  *data   = (resource_data_t *)plugin->data;
    const char       *event  = mrp_get_event_name(id);
    const char       *cfgfile;

    MRP_UNUSED(w);
    MRP_UNUSED(event_data);


    mrp_log_warning("%s: got event 0x%x (%s):", plugin->instance, id, event);

    if (data && event) {
        if (!strcmp(event, MRP_PLUGIN_EVENT_STARTED)) {
            cfgfile = args[ARG_CONFIG_FILE].str;

            set_default_configuration();
            mrp_log_warning("%s: built-in default configuration is in use",
                            plugin->instance);

            initiate_transport(plugin);

            return;
        }
    }
}


static int subscribe_events(mrp_plugin_t *plugin)
{
    resource_data_t  *data = (resource_data_t *)plugin->data;
    mrp_event_mask_t  events;

    mrp_set_named_events(&events,
                         MRP_PLUGIN_EVENT_LOADED,
                         MRP_PLUGIN_EVENT_STARTED,
                         MRP_PLUGIN_EVENT_FAILED,
                         MRP_PLUGIN_EVENT_STOPPING,
                         MRP_PLUGIN_EVENT_STOPPED,
                         MRP_PLUGIN_EVENT_UNLOADED,
                         NULL);

    data->w = mrp_add_event_watch(&events, event_cb, plugin);

    return (data->w != NULL);
}


static void unsubscribe_events(mrp_plugin_t *plugin)
{
    resource_data_t *data = (resource_data_t *)plugin->data;

    if (data->w) {
        mrp_del_event_watch(data->w);
        data->w = NULL;
    }
}



static int resource_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args;
    resource_data_t  *data;

    mrp_log_info("%s() called for resource instance '%s'...", __FUNCTION__,
                 plugin->instance);

    args = plugin->args;
    mrp_log_info(" config-file:  '%s'", args[ARG_CONFIG_FILE].str);


    if (!(data = mrp_allocz(sizeof(*data)))) {
        mrp_log_error("Failed to allocate private data for resource plugin "
                      "instance %s.", plugin->instance);
        return FALSE;
    }

    data->plugin = plugin;
    mrp_list_init(&data->clients);

    plugin->data = data;

    subscribe_events(plugin);

    return TRUE;
}


static void resource_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called for test instance '%s'...", __FUNCTION__,
                 plugin->instance);

    unsubscribe_events(plugin);
}


#define RESOURCE_DESCRIPTION "Plugin to implement resource message protocol"
#define RESOURCE_HELP        "Maybe later ..."
#define RESOURCE_VERSION     MRP_VERSION_INT(0, 0, 1)
#define RESOURCE_AUTHORS     "Janos Kovacs <jankovac503@gmail.com>"

#define DEF_CONFIG_FILE      "/etc/murphy/resource.conf"
#define DEF_ADDRESS          "tcp4:localhost:2012"

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_CONFIG_FILE, STRING, "config-file", DEF_CONFIG_FILE),
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS    , STRING, "address"    , DEF_ADDRESS    ),
};


MURPHY_REGISTER_PLUGIN("resource",
                       RESOURCE_VERSION,
                       RESOURCE_DESCRIPTION,
                       RESOURCE_AUTHORS,
                       RESOURCE_HELP,
                       MRP_SINGLETON,
                       resource_init,
                       resource_exit,
                       args, MRP_ARRAY_SIZE(args),
#if 0
                       exports, MRP_ARRAY_SIZE(exports),
                       imports, MRP_ARRAY_SIZE(imports),
#else
                       NULL, 0,
                       NULL, 0,
#endif
                       &resource_group);
