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

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>
#include <murphy/common/debug.h>
#include <murphy/core/plugin.h>
#include <murphy/core/console.h>
#include <murphy/core/lua-bindings/murphy.h>

#include <murphy-db/mql.h>
#include <murphy-db/mqi.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>
#include <murphy/resource/manager-api.h>
#include <murphy/resource/protocol.h>

#include <murphy/resource/resource-set.h>

#define ATTRIBUTE_MAX MRP_ATTRIBUTE_MAX



enum {
    RESOURCE_ERROR  = -1,
    ATTRIBUTE_ERROR = -1,
    RESOURCE_OK     = 0,
    ATTRIBUTE_OK    = 0,
    ATTRIBUTE_LAST,
    RESOURCE_LAST,
};


enum {
    ARG_ADDRESS,
};


typedef struct {
    mrp_plugin_t      *plugin;
    mrp_event_bus_t   *plugin_bus;
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


static void print_zones_cb(mrp_console_t *, void *, int, char **argv);
static void print_classes_cb(mrp_console_t *, void *, int, char **argv);
static void print_sets_cb(mrp_console_t *, void *, int, char **argv);
static void print_owners_cb(mrp_console_t *, void *, int, char **argv);
static void print_resources_cb(mrp_console_t *, void *, int, char **argv);

static void resource_event_handler(uint32_t, mrp_resource_set_t *, void *);


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
        MRP_TOKENIZED_CMD("owners" , print_owners_cb , FALSE,
                          "owners", "prints resource owners",
                          "prints for each zone the owner application class "
                          "of each resource. The data sources for the "
                          "printout are the internal data structures of the "
                          "resource library"),
        MRP_TOKENIZED_CMD("resources" , print_resources_cb , FALSE,
                          "resources", "prints resources",
                          "prints all resource definitions and along with "
                          "all their attributes. The data sources for the "
                          "printout are the internal data structures of the "
                          "resource library"),

});


static void print_zones_cb(mrp_console_t *c, void *user_data,
                           int argc, char **argv)
{
    const char **zone_names;
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    printf("Zones:\n");

    if ((zone_names = mrp_zone_get_all_names(0, NULL))) {

        for (i = 0;  zone_names[i];  i++)
            printf("   %s\n", zone_names[i]);


        mrp_free(zone_names);
    }
}


static void print_classes_cb(mrp_console_t *c, void *user_data,
                             int argc, char **argv)
{
    char buf[8192];

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_application_class_print(buf, sizeof(buf), false);

    printf("%s", buf);
}


static void print_sets_cb(mrp_console_t *c, void *user_data,
                          int argc, char **argv)
{
    static int size = 8192;
    char       buf[size];

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    if (mrp_application_class_print(buf, sizeof(buf), true) >= size)
        size *= 2;

    printf("%s", buf);
}


static void print_owners_cb(mrp_console_t *c, void *user_data,
                            int argc, char **argv)
{
    char buf[2048];

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_resource_owner_print(buf, sizeof(buf));

    printf("%s", buf);
}


static void print_resources_cb(mrp_console_t *c, void *user_data,
                               int argc, char **argv)
{
    const char **names;
    mrp_attr_t  *attrs, *a;
    mrp_attr_t   buf[ATTRIBUTE_MAX];
    uint32_t     resid;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    if (!(names = mrp_resource_definition_get_all_names(0, NULL))) {
        printf("Failed to read resource definitions.\n");
        return;
    }

    printf("Resource definitions:\n");
    for (resid = 0; names[resid]; resid++) {
        attrs = mrp_resource_definition_read_all_attributes(resid,
                                                            ATTRIBUTE_MAX, buf);
        printf("    Resource '%s'\n", names[resid]);
        for (a = attrs; a->name; a++) {
            printf("        attribute %s: ", a->name);
            switch (a->type) {
            case mqi_string:
                printf("'%s'\n", a->value.string);
                break;
            case mqi_integer:
                printf("%d\n", a->value.integer);
                break;
            case mqi_unsignd:
                printf("%u\n", a->value.unsignd);
                break;
            case mqi_floating:
                printf("%f\n", a->value.floating);
                break;
            default:
                printf("<unsupported type>\n");
                break;
            }
        }
    }

    mrp_free(names);
}


#if 0
static int set_default_configuration(void)
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
#endif

static void reply_with_array(client_t *client, mrp_msg_t *msg,
                             uint16_t tag, const char **arr)
{
    resource_data_t *data   = client->data;
    mrp_plugin_t    *plugin = data->plugin;
    uint16_t         dim;
    bool             s;

    for (dim = 0;  arr[dim];  dim++)
        ;

    s  = mrp_msg_append(msg, MRP_MSG_TAG_SINT16(RESPROTO_REQUEST_STATUS, 0));
    s &= mrp_msg_append(msg, MRP_MSG_TAG_STRING_ARRAY(tag, dim, arr));

    if (!s) {
        mrp_log_error("%s: failed to build reply", plugin->instance);
        return;
    }

    if (!mrp_transport_send(client->transp, msg))
        mrp_log_error("%s: failed to send reply", plugin->instance);
}

static void reply_with_status(client_t *client, mrp_msg_t *msg, int16_t err)
{
    if (!mrp_msg_append(msg,MRP_MSG_TAG_SINT16(RESPROTO_REQUEST_STATUS,err)) ||
        !mrp_transport_send(client->transp, msg))
    {
        resource_data_t *data   = client->data;
        mrp_plugin_t    *plugin = data->plugin;

        mrp_log_error("%s: failed to create or send reply", plugin->instance);
    }
}


static bool write_attributes(mrp_msg_t *msg, mrp_attr_t *attrs)
{
#define PUSH(m, tag, typ, val)    \
    mrp_msg_append(m, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))

    mrp_attr_t *a;
    bool ok;

    if (attrs) {
        for (a = attrs;  a->name;  a++) {
            if (!PUSH(msg, ATTRIBUTE_NAME, STRING, a->name))
                return false;;

            switch (a->type) {
            case mqi_string:
                ok = PUSH(msg, ATTRIBUTE_VALUE, STRING, a->value.string);
                break;
            case mqi_integer:
                ok = PUSH(msg, ATTRIBUTE_VALUE, SINT32, a->value.integer);
                break;
            case mqi_unsignd:
                ok = PUSH(msg, ATTRIBUTE_VALUE, UINT32, a->value.unsignd);
                break;
            case mqi_floating:
                ok = PUSH(msg, ATTRIBUTE_VALUE, DOUBLE, a->value.floating);
                break;
            default:
                ok = false;
                break;
            }

            if (!ok)
                return false;
        }
    }

    if (!PUSH(msg, SECTION_END, UINT8, 0))
        return false;

    return true;

#undef PUSH
}


static void query_resources_request(client_t *client, mrp_msg_t *req)
{
#define PUSH(m, tag, typ, val)    \
    mrp_msg_append(m, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))


    resource_data_t  *data   = client->data;
    mrp_plugin_t     *plugin = data->plugin;
    const char      **names;
    mrp_attr_t       *attrs;
    mrp_attr_t        buf[ATTRIBUTE_MAX];
    uint32_t          resid;

    if (!(names = mrp_resource_definition_get_all_names(0, NULL)))
        reply_with_status(client, req, ENOMEM);
    else {
        if (!PUSH(req, REQUEST_STATUS, SINT16, 0))
            goto failed;
        else {
            for (resid = 0;   names[resid];   resid++) {
                attrs = mrp_resource_definition_read_all_attributes(
                                                    resid, ATTRIBUTE_MAX, buf);

                if (!PUSH(req, RESOURCE_NAME, STRING, names[resid]) ||
                    !write_attributes(req, attrs))
                    goto failed;
            }

            if (!mrp_transport_send(client->transp, req))
                mrp_log_error("%s: failed to send reply", plugin->instance);

            mrp_free(names);
        }
    }

    return;

 failed:
    mrp_log_error("%s: can't build recource query reply message",
                  plugin->instance);
    mrp_free(names);


#undef PUSH
}

static void query_classes_request(client_t *client, mrp_msg_t *req)
{
    const char **names = mrp_application_class_get_all_names(0, NULL);

    if (!names)
        reply_with_status(client, req, ENOMEM);
    else {
        reply_with_array(client, req, RESPROTO_CLASS_NAME, names);
        mrp_free(names);
    }
}

static void query_zones_request(client_t *client, mrp_msg_t *req)
{
    const char **names = mrp_zone_get_all_names(0, NULL);

    if (!names)
        reply_with_status(client, req, ENOMEM);
    else {
        reply_with_array(client, req, RESPROTO_ZONE_NAME, names);
        mrp_free(names);
    }
}

static int read_attribute(mrp_msg_t *req, mrp_attr_t *attr, void **pcurs)
{
    uint16_t tag;
    uint16_t type;
    size_t size;
    mrp_msg_value_t value;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size))
        return ATTRIBUTE_ERROR;

    if (tag == RESPROTO_SECTION_END)
        return ATTRIBUTE_LAST;

    if (tag != RESPROTO_ATTRIBUTE_NAME || type != MRP_MSG_FIELD_STRING)
        return ATTRIBUTE_ERROR;

    attr->name = value.str;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_ATTRIBUTE_VALUE)
        return ATTRIBUTE_ERROR;

    switch (type) {
    case MRP_MSG_FIELD_STRING:
        attr->type = mqi_string;
        attr->value.string = value.str;
        break;
    case MRP_MSG_FIELD_SINT32:
        attr->type = mqi_integer;
        attr->value.integer = value.s32;
        break;
    case MRP_MSG_FIELD_UINT32:
        attr->type = mqi_unsignd;
        attr->value.unsignd = value.u32;
        break;
    case MRP_MSG_FIELD_DOUBLE:
        attr->type = mqi_floating;
        attr->value.floating = value.dbl;
        break;
    default:
        return ATTRIBUTE_ERROR;
    }

    {
        char str[256];

        switch (attr->type) {
        case mqi_string:
            snprintf(str, sizeof(str), "'%s'", attr->value.string);
            break;
        case mqi_integer:
            snprintf(str, sizeof(str), "%d", attr->value.integer);
            break;
        case mqi_unsignd:
            snprintf(str, sizeof(str), "%u", attr->value.unsignd);
            break;
        case mqi_floating:
            snprintf(str, sizeof(str), "%.2lf", attr->value.floating);
            break;
        default:
            snprintf(str, sizeof(str), "< ??? >");
            break;
        }

        mrp_log_info("      attribute %s:%s", attr->name, str);
    }

    return ATTRIBUTE_OK;
}


static int read_resource(mrp_resource_set_t *rset, mrp_msg_t *req,void **pcurs)
{
    uint16_t        tag;
    uint16_t        type;
    size_t          size;
    mrp_msg_value_t value;
    const char     *name;
    bool            mand;
    bool            shared;
    mrp_attr_t      attrs[ATTRIBUTE_MAX + 1];
    uint32_t        i;
    int             arst;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size))
        return RESOURCE_LAST;

    if (tag != RESPROTO_RESOURCE_NAME || type != MRP_MSG_FIELD_STRING)
        return RESOURCE_ERROR;

    name = value.str;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_FLAGS || type != MRP_MSG_FIELD_UINT32)
        return RESOURCE_ERROR;

    mand   = (value.u32 & RESPROTO_RESFLAG_MANDATORY) ? true : false;
    shared = (value.u32 & RESPROTO_RESFLAG_SHARED)    ? true : false;

    mrp_log_info("   resource: name:'%s' %s %s", name,
                 mand?"mandatory":"optional ", shared?"shared":"exclusive");

    for (i = 0, arst = 0;    i < ATTRIBUTE_MAX;    i++) {
        if ((arst = read_attribute(req, attrs + i, pcurs)))
            break;
    }

    memset(attrs + i, 0, sizeof(mrp_attr_t));

    if (arst > 0) {
        if (mrp_resource_set_add_resource(rset, name, shared, attrs, mand) < 0)
            arst = RESOURCE_ERROR;
        else
            arst = 0;
    }

    return arst;
}


static void create_resource_set_request(client_t *client, mrp_msg_t *req,
                                        uint32_t seqno, void **pcurs)
{
    static uint16_t reqtyp = RESPROTO_CREATE_RESOURCE_SET;

    resource_data_t        *data   = client->data;
    mrp_plugin_t           *plugin = data->plugin;
    mrp_resource_set_t     *rset   = 0;
    mrp_msg_t              *rpl;
    uint32_t                flags;
    uint32_t                priority;
    const char             *class;
    const char             *zone;
    uint16_t                tag;
    uint16_t                type;
    size_t                  size;
    mrp_msg_value_t         value;
    uint32_t                rsid;
    int                     arst;
    int32_t                 status;
    bool                    auto_release;
    bool                    auto_acquire;
    bool                    dont_wait;
    mrp_resource_event_cb_t event_cb;

    MRP_ASSERT(client, "invalid argument");
    MRP_ASSERT(client->rscli, "confused with data structures");

    rsid = MRP_RESOURCE_ID_INVALID;
    status = EINVAL;


    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_FLAGS || type != MRP_MSG_FIELD_UINT32)
        goto reply;

    flags = value.u32;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_PRIORITY || type != MRP_MSG_FIELD_UINT32)
        goto reply;

    priority = value.u32;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_CLASS_NAME || type != MRP_MSG_FIELD_STRING)
        goto reply;

    class = value.str;

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_ZONE_NAME || type != MRP_MSG_FIELD_STRING)
        goto reply;

    zone = value.str;

    mrp_log_info("resource-set flags:%u priority:%u class:'%s' zone:'%s'",
                 flags, priority, class, zone);

    auto_release = (flags & RESPROTO_RSETFLAG_AUTORELEASE);
    auto_acquire = (flags & RESPROTO_RSETFLAG_AUTOACQUIRE);
    dont_wait    = (flags & RESPROTO_RSETFLAG_DONTWAIT);

    if (flags & RESPROTO_RSETFLAG_NOEVENTS)
        event_cb = NULL;
    else
        event_cb = resource_event_handler;

    rset = mrp_resource_set_create(client->rscli, auto_release, dont_wait,
                                   priority, event_cb, client);
    if (!rset)
        goto reply;

    rsid = mrp_get_resource_set_id(rset);

    while ((arst = read_resource(rset, req, pcurs)) == 0)
        ;

    if (arst > 0) {
        if (auto_acquire)
            mrp_resource_set_acquire(rset,seqno);
        if (mrp_application_class_add_resource_set(class,zone,rset,seqno) == 0)
            status = 0;
    }

 reply:
    rpl = mrp_msg_create(MRP_MSG_TAG_UINT32( RESPROTO_SEQUENCE_NO    , seqno ),
                         MRP_MSG_TAG_UINT16( RESPROTO_REQUEST_TYPE   , reqtyp),
                         MRP_MSG_TAG_SINT16( RESPROTO_REQUEST_STATUS , status),
                         MRP_MSG_TAG_UINT32( RESPROTO_RESOURCE_SET_ID, rsid  ),
                         RESPROTO_MESSAGE_END                                );
    if (!rpl || !mrp_transport_send(client->transp, rpl)) {
        mrp_log_error("%s: failed to send reply", plugin->instance);
        return;
    }

    mrp_msg_unref(rpl);

    if (status != 0)
        mrp_resource_set_destroy(rset);
}

static void destroy_resource_set_request(client_t *client, mrp_msg_t *req,
                                         void **pcurs)
{
    uint16_t            tag;
    uint16_t            type;
    size_t              size;
    mrp_msg_value_t     value;
    uint32_t            rset_id;
    mrp_resource_set_t *rset;

    MRP_ASSERT(client, "invalid argument");
    MRP_ASSERT(client->rscli, "confused with data structures");

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_SET_ID || type != MRP_MSG_FIELD_UINT32)
    {
        reply_with_status(client, req, EINVAL);
        return;
    }

    rset_id = value.u32;

    if (!(rset = mrp_resource_client_find_set(client->rscli, rset_id))) {
        reply_with_status(client, req, ENOENT);
        return;
    }

    reply_with_status(client, req, 0);

    mrp_resource_set_destroy(rset);
}


static void acquire_resource_set_request(client_t *client, mrp_msg_t *req,
                                         uint32_t seqno, bool acquire,
                                         void **pcurs)
{
    uint16_t            tag;
    uint16_t            type;
    size_t              size;
    mrp_msg_value_t     value;
    uint32_t            rset_id;
    mrp_resource_set_t *rset;

    MRP_ASSERT(client, "invalid argument");
    MRP_ASSERT(client->rscli, "confused with data structures");

    if (!mrp_msg_iterate(req, pcurs, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_SET_ID || type != MRP_MSG_FIELD_UINT32)
    {
        reply_with_status(client, req, EINVAL);
        return;
    }

    rset_id = value.u32;

    if (!(rset = mrp_resource_client_find_set(client->rscli, rset_id))) {
        reply_with_status(client, req, ENOENT);
        return;
    }

    reply_with_status(client, req, 0);

    if (acquire)
        mrp_resource_set_acquire(rset, seqno);
    else
        mrp_resource_set_release(rset, seqno);
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

    mrp_log_info("%s: %s connected", plugin->instance, name);
}

static void closed_evt(mrp_transport_t *transp, int error, void *user_data)
{
    client_t        *client = (client_t *)user_data;
    resource_data_t *data   = client->data;
    mrp_plugin_t    *plugin = data->plugin;

    MRP_UNUSED(transp);

    if (error)
        mrp_log_error("%s: connection error %d (%s)",
                      plugin->instance, error, strerror(error));
    else
        mrp_log_info("%s: peer closed connection", plugin->instance);

    mrp_resource_client_destroy(client->rscli);

    mrp_list_delete(&client->list);
    mrp_free(client);

    mrp_transport_disconnect(transp);
    mrp_transport_destroy(transp);
}



static void recvfrom_msg(mrp_transport_t *transp, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    client_t               *client = (client_t *)user_data;
    resource_data_t        *data   = client->data;
    mrp_plugin_t           *plugin = data->plugin;
    void                   *cursor = NULL;
    uint32_t                seqno;
    mrp_resproto_request_t  reqtyp;
    uint16_t                tag;
    uint16_t                type;
    size_t                  size;
    mrp_msg_value_t         value;


    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    MRP_ASSERT(client->transp == transp, "confused with data structures");

    mrp_log_info("%s: received a message", plugin->instance);
    mrp_msg_dump(msg, stdout);


    if (mrp_msg_iterate(msg, &cursor, &tag, &type, &value, &size) &&
        tag == RESPROTO_SEQUENCE_NO && type == MRP_MSG_FIELD_UINT32)
        seqno = value.u32;
    else {
        mrp_log_warning("%s: malformed message. Bad or missing "
                        "sequence number", plugin->instance);
        return;
    }

    if (mrp_msg_iterate(msg, &cursor, &tag, &type, &value, &size) &&
        tag == RESPROTO_REQUEST_TYPE && type == MRP_MSG_FIELD_UINT16)
        reqtyp = value.u16;
    else {
        mrp_log_warning("%s: malformed message. Bad or missing "
                        "request type", plugin->instance);
        return;
    }

    switch (reqtyp) {

    case RESPROTO_QUERY_RESOURCES:
        query_resources_request(client, msg);
        break;

    case RESPROTO_QUERY_CLASSES:
        query_classes_request(client, msg);
        break;

    case RESPROTO_QUERY_ZONES:
        query_zones_request(client, msg);
        break;

    case RESPROTO_CREATE_RESOURCE_SET:
        create_resource_set_request(client, msg, seqno, &cursor);
        break;

    case RESPROTO_DESTROY_RESOURCE_SET:
        destroy_resource_set_request(client, msg, &cursor);
        break;

    case RESPROTO_ACQUIRE_RESOURCE_SET:
        acquire_resource_set_request(client, msg, seqno, true, &cursor);
        break;

    case RESPROTO_RELEASE_RESOURCE_SET:
        acquire_resource_set_request(client, msg, seqno, false, &cursor);
        break;

    default:
        mrp_log_warning("%s: unsupported request type %d",
                        plugin->instance, reqtyp);
        break;
    }
}

static void recv_msg(mrp_transport_t *transp, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(transp, msg, NULL, 0, user_data);
}


static void resource_event_handler(uint32_t reqid, mrp_resource_set_t *rset,
                                   void *userdata)
{
#define FIELD(tag, typ, val)      \
    RESPROTO_##tag, MRP_MSG_FIELD_##typ, val
#define PUSH(m, tag, typ, val)    \
    mrp_msg_append(m, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))

    client_t           *client = (client_t *)userdata;
    resource_data_t    *data;
    mrp_plugin_t       *plugin;
    uint16_t            reqtyp;
    uint16_t            state;
    mrp_resource_mask_t grant;
    mrp_resource_mask_t advice;
    mrp_resource_mask_t mask;
    mrp_resource_mask_t all;
    mrp_msg_t          *msg;
    mrp_resource_t     *res;
    uint32_t            id;
    const char         *name;
    void               *curs;
    mrp_attr_t          attrs[ATTRIBUTE_MAX + 1];

    MRP_ASSERT(rset && client, "invalid argument");

    data   = client->data;
    plugin = data->plugin;

    reqtyp = RESPROTO_RESOURCES_EVENT;
    id     = mrp_get_resource_set_id(rset);
    grant  = mrp_get_resource_set_grant(rset);
    advice = mrp_get_resource_set_advice(rset);

    if (mrp_get_resource_set_state(rset) == mrp_resource_acquire)
        state = RESPROTO_ACQUIRE;
    else
        state = RESPROTO_RELEASE;

    msg = mrp_msg_create(FIELD( SEQUENCE_NO    , UINT32, reqid  ),
                         FIELD( REQUEST_TYPE   , UINT16, reqtyp ),
                         FIELD( RESOURCE_SET_ID, UINT32, id     ),
                         FIELD( RESOURCE_STATE , UINT16, state  ),
                         FIELD( RESOURCE_GRANT , UINT32, grant  ),
                         FIELD( RESOURCE_ADVICE, UINT32, advice ),
                         RESPROTO_MESSAGE_END                   );

    if (!msg)
        goto failed;

    all = grant | advice;
    curs = NULL;

    while ((res = mrp_resource_set_iterate_resources(rset, &curs))) {
        mask = mrp_resource_get_mask(res);

        if (!(all & mask))
            continue;

        id = mrp_resource_get_id(res);
        name = mrp_resource_get_name(res);

         if (!PUSH(msg, RESOURCE_ID  , UINT32, id  ) ||
             !PUSH(msg, RESOURCE_NAME, STRING, name)  )
             goto failed;

         if (!mrp_resource_read_all_attributes(res, ATTRIBUTE_MAX + 1, attrs))
             goto failed;

         if (!write_attributes(msg, attrs))
                 goto failed;
    }

    if (!mrp_transport_send(client->transp, msg))
        goto failed;

    mrp_msg_unref(msg);

    return;

    failed:
         mrp_log_error("%s: failed to build/send message for resource event",
                       plugin->instance);
         mrp_msg_unref(msg);

#undef PUSH
#undef FIELD
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

    if (addr == NULL)
        addr = mrp_resource_get_default_address();

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

    mrp_log_info("%s: listening for connections on %s", plugin->instance,addr);

    return 0;
}


static void initiate_lua_configuration(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_resource_configuration_init();
}

static void event_cb(mrp_event_watch_t *w, uint32_t id, int format,
                     void *event_data, void *user_data)
{
    mrp_plugin_t     *plugin     = (mrp_plugin_t *)user_data;
#if 0
    mrp_plugin_arg_t *args     = plugin->args;
#endif
    resource_data_t  *data     = (resource_data_t *)plugin->data;
    const char       *event    = mrp_event_name(id);
    uint16_t          tag_inst = MRP_PLUGIN_TAG_INSTANCE;
    uint16_t          tag_name = MRP_PLUGIN_TAG_PLUGIN;
    const char       *inst;
    const char       *name;
    int               success;

    MRP_UNUSED(w);
    MRP_UNUSED(format);

    mrp_log_info("%s: got event 0x%x (%s):", plugin->instance, id, event);

    if (data && event) {
        if (!strcmp(event, MRP_PLUGIN_EVENT_STARTED)) {
            success = mrp_msg_get(event_data,
                                  MRP_MSG_TAG_STRING(tag_inst, &inst),
                                  MRP_MSG_TAG_STRING(tag_name, &name),
                                  MRP_MSG_END);
            if (success) {
                if (!strcmp(inst, plugin->instance)) {
#if 0
                    set_default_configuration();
                    mrp_log_info("%s: built-in default configuration "
                                 "is in use", plugin->instance);
#endif

                    initiate_lua_configuration(plugin);
                    initiate_transport(plugin);
                }
            }
        } /* if PLUGIN_STARTED */
    }
}


static int subscribe_events(mrp_plugin_t *plugin)
{
    resource_data_t  *data = (resource_data_t *)plugin->data;
    mrp_mainloop_t   *ml   = plugin->ctx->ml;
    mrp_event_bus_t  *bus  = mrp_event_bus_get(ml, MRP_PLUGIN_BUS);
    mrp_event_mask_t  events;

    if (bus == NULL)
        return FALSE;

    data->plugin_bus = bus;

    mrp_mask_init(&events);
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_LOADED));
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_STARTED));
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_FAILED));
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_STOPPING));
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_STOPPED));
    mrp_mask_set(&events, mrp_event_id(MRP_PLUGIN_EVENT_UNLOADED));

    data->w = mrp_event_add_watch_mask(bus, &events, event_cb, plugin);

    return (data->w != NULL);
}


static void unsubscribe_events(mrp_plugin_t *plugin)
{
    resource_data_t *data = (resource_data_t *)plugin->data;

    if (data->w) {
        mrp_event_del_watch(data->w);
        data->w = NULL;
    }
}


static void register_events(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    /* register the events that are sent on the resource state changes */

    mrp_event_register(MURPHY_RESOURCE_EVENT_CREATED);
    mrp_event_register(MURPHY_RESOURCE_EVENT_ACQUIRE);
    mrp_event_register(MURPHY_RESOURCE_EVENT_RELEASE);
    mrp_event_register(MURPHY_RESOURCE_EVENT_DESTROYED);
}


static int resource_init(mrp_plugin_t *plugin)
{
#if 0
    mrp_plugin_arg_t *args = plugin->args;
#endif
    resource_data_t  *data;

    mrp_log_info("%s() called for resource instance '%s'...", __FUNCTION__,
                 plugin->instance);

    if (!(data = mrp_allocz(sizeof(*data)))) {
        mrp_log_error("Failed to allocate private data for resource plugin "
                      "instance %s.", plugin->instance);
        return FALSE;
    }

    data->plugin = plugin;
    mrp_list_init(&data->clients);

    plugin->data = data;

    register_events(plugin);
    subscribe_events(plugin);
    initiate_lua_configuration(plugin);

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
#define DEF_ADDRESS          NULL

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX( ARG_ADDRESS, STRING, "address", DEF_ADDRESS ),
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
