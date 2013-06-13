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
#include <sys/stat.h>
#include <signal.h>

#include <dbus/dbus.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/common/dbus.h>

#include <murphy/resource/client-api.h>


#define MURPHY_PATH_BASE "/org/murphy/resource"

#define MANAGER_IFACE "org.murphy.manager"
#define RSET_IFACE "org.murphy.resourceset"
#define RESOURCE_IFACE "org.murphy.resource"

#define MAX_PATH_LENGTH 64
#define MAX_DBUS_SIG_LENGTH 8


#define MANAGER_CREATE_RESOURCE_SET "createResourceSet"
#define MANAGER_GET_PROPERTIES      "getProperties"

#define RSET_SET_PROPERTY           "setProperty"
#define RSET_GET_PROPERTIES         "getProperties"
#define RSET_ADD_RESOURCE           "addResource"
#define RSET_REQUEST                "request"
#define RSET_RELEASE                "release"
#define RSET_DELETE                 "delete"

#define RESOURCE_SET_PROPERTY       "setProperty"
#define RESOURCE_GET_PROPERTIES     "getProperties"
#define RESOURCE_DELETE             "delete"

#define PROP_RESOURCE_SETS          "resourceSets"
#define PROP_AVAILABLE_RESOURCES    "availableResources"
#define PROP_NAME                   "name"
#define PROP_SHARED                 "shared"
#define PROP_MANDATORY              "mandatory"
#define PROP_CLASS                  "class"
#define PROP_RESOURCES              "resources"
#define PROP_STATUS                 "status"
#define PROP_ATTRIBUTES             "attributes"
#define PROP_ATTRIBUTES_CONF        "attributes_conf"

#define SIG_PROPERTYCHANGED         "propertyChanged"

enum {
    ARG_DR_BUS,
    ARG_DR_SERVICE,
    ARG_DR_TRACK_CLIENTS,
    ARG_DR_DEFAULT_ZONE,
    ARG_DR_DEFAULT_CLASS,
};

typedef struct manager_o_s manager_o_t;

typedef struct {
    /* configuration */
    mrp_dbus_t *dbus;
    const char *addr;
    const char *bus;
    const char *default_zone;
    const char *default_class;

    bool tracking;

    /* resource management */
    manager_o_t *mgr;

    /* murphy integration */
} dbus_data_t;

typedef struct property_o_s {
    /* dbus properties */
    char *path;
    char *interface;
    char *dbus_sig;

    /* data */
    char *name;
    void *value;
    bool writable; /* used later when we allow more access to properties */

    dbus_data_t *ctx;

    /* function to free the value */
    void (*free_data)(void *data);

    /* may be needed in the future? maybe not in this form */
    int (*compare)(struct property_o_s *a, struct property_o_s *b);
} property_o_t;

struct manager_o_s {
    uint32_t next_id; /* next resource set id */

    dbus_data_t *ctx;
    mrp_htbl_t *rsets;

    property_o_t *rsets_prop;

    /* resource library */
    const char *zone;
    mrp_resource_client_t *client;
};

typedef struct {
    uint32_t next_id; /* next resource id */
    char *path;
    char *owner;

    manager_o_t *mgr; /* backpointer */

    mrp_htbl_t *resources;

    property_o_t *resources_prop;
    property_o_t *available_resources_prop;
    property_o_t *class_prop;
    property_o_t *status_prop;

    /* resource library */
    bool locked; /* if the library allows the settings to be changed */
    bool acquired; /* set to true when we are starting the acquisition */
    mrp_resource_set_t *set;
} resource_set_o_t;

typedef struct {
    char *path;

    resource_set_o_t *rset; /* backpointer */

    property_o_t *status_prop;
    property_o_t *mandatory_prop;
    property_o_t *shared_prop;
    property_o_t *name_prop;
    property_o_t *arguments_prop;
    property_o_t *conf_prop;
} resource_o_t;

static int mgr_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static int rset_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static int resource_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static void dbus_name_cb(mrp_dbus_t *dbus, const char *name, int up,
                          const char *owner, void *user_data);


/* copy the keys in a hash map to a NULL-terminated array */

struct key_data_s {
    int curr_key;
    char **keys;
};


static int copy_keys_cb(void *key, void *object, void *user_data)
{
    struct key_data_s *kd = user_data;

    MRP_UNUSED(object);

    kd->keys[kd->curr_key] = mrp_strdup((char *) key);
    kd->curr_key++;

    return MRP_HTBL_ITER_MORE;
}


static int count_keys_cb(void *key, void *object, void *user_data)
{
    int *count = user_data;

    MRP_UNUSED(key);
    MRP_UNUSED(object);

    *count = *count + 1;

    return MRP_HTBL_ITER_MORE;
}


static char **htbl_keys(mrp_htbl_t *ht)
{
    char **keys;
    int len = 0;
    struct key_data_s kd;

    if (!ht)
        return NULL;

    mrp_htbl_foreach(ht, count_keys_cb, &len);

    keys = mrp_alloc_array(char *, len+1);

    kd.curr_key = 0;
    kd.keys = keys;

    mrp_htbl_foreach(ht, copy_keys_cb, &kd);

    keys[len] = NULL;

    return keys;
}

/* functions for freeing property values */

static void free_value(void *val) {
    mrp_free(val);
}


static void free_string_array(void *array) {

    char **i = array;

    if (!array)
        return;

    while (*i) {
        mrp_free(*i);
        i++;
    }

    mrp_free(array);
}


static void free_attr_array(mrp_attr_t *arr)
{
    /* only free the allocated members */
    mrp_attr_t *i = arr;

    while (i->name) {

        if (i->type == mqi_string)
            mrp_free((void *) i->value.string);

        mrp_free((void *) i->name);
        i++;
    }
}


static char **copy_string_array(const char **array)
{
    int count = 0, i;
    char **tmp = (char **) array;
    char **ret;

    if (!array)
        return NULL;

    while (*tmp) {
        count++;
        tmp++;
    }

    ret = mrp_alloc_array(char *, count+1);

    if (!ret)
        return NULL;

    for (i = 0; i < count; i++) {
        ret[i] = mrp_strdup(array[i]);
        if (!ret[i]) {
            free_string_array(ret);
            return NULL;
        }
    }

    ret[i] = NULL;

    return ret;
}


static int dbus_value_cb(void *key, void *object, void *user_data)
{
    DBusMessageIter *iter = user_data;
    DBusMessageIter dict_iter;
    DBusMessageIter variant_iter;
    char *arg_name = key;
    mrp_attr_t *arg_value = object;

    dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL,
            &dict_iter);

    dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &arg_name);

    /* TODO: do this with get_property_entry or similar to deal with multiple
       types of different values? */

    switch(arg_value->type) {
        case mqi_string:
            dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT,
                    "s", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                    &arg_value->value.string);
            break;
        case mqi_integer:
            dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT,
                    "i", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32,
                    &arg_value->value.integer);
            break;
        case mqi_unsignd:
            dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT,
                    "u", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32,
                    &arg_value->value.unsignd);
            break;
        case mqi_floating:
            dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT,
                    "d", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_DOUBLE,
                    &arg_value->value.floating);
            break;
        default:
            mrp_log_error("unknown type %d in attributes", arg_value->type);
            break;
    }

    dbus_message_iter_close_container(&dict_iter, &variant_iter);

    dbus_message_iter_close_container(iter, &dict_iter);

    return MRP_HTBL_ITER_MORE;
}


static bool get_property_entry(property_o_t *prop, DBusMessageIter *dict_iter)
{
    DBusMessageIter variant_iter;

    /* FIXME: check return values */

    dbus_message_iter_append_basic(dict_iter, DBUS_TYPE_STRING, &prop->name);

    dbus_message_iter_open_container(dict_iter, DBUS_TYPE_VARIANT,
            prop->dbus_sig, &variant_iter);

    /* TODO: this might be remade to be generic? */

    if (strcmp(prop->dbus_sig, "s") == 0) {
        dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                &prop->value);
    }
    else if (strcmp(prop->dbus_sig, "b") == 0) {
        dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN,
                prop->value);
    }
    else if (strcmp(prop->dbus_sig, "as") == 0) {
        DBusMessageIter array_iter;
        char **i = prop->value;

        dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY,
                "s", &array_iter);

        while (*i) {
            dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING, i);
            i++;
        }

        dbus_message_iter_close_container(&variant_iter, &array_iter);
    }
    else if (strcmp(prop->dbus_sig, "ao") == 0) {
        DBusMessageIter array_iter;
        char **i = prop->value;

        dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY,
                "o", &array_iter);

        while (*i) {
            dbus_message_iter_append_basic(&array_iter,
                    DBUS_TYPE_OBJECT_PATH, i);
            i++;
        }

        dbus_message_iter_close_container(&variant_iter, &array_iter);
    }
    else if (strcmp(prop->dbus_sig, "a{sv}") == 0) {
        DBusMessageIter array_iter;

        dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "{sv}",
                &array_iter);

        /* iterate through the elements in the map */
        mrp_htbl_foreach(prop->value, dbus_value_cb, &array_iter);

        dbus_message_iter_close_container(&variant_iter, &array_iter);

    }
    else {
        mrp_log_error("Unknown sig '%s'", prop->dbus_sig);
        goto error;
    }

    dbus_message_iter_close_container(dict_iter, &variant_iter);

    return TRUE;

error:
    return FALSE;
}


static bool get_property_dict_entry(property_o_t *prop, DBusMessageIter *iter)
{
    DBusMessageIter dict_iter;
    bool ret;

    dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL,
            &dict_iter);

    ret = get_property_entry(prop, &dict_iter);

    dbus_message_iter_close_container(iter, &dict_iter);

    return ret;
}


static void trigger_property_changed_signal(dbus_data_t *ctx,
        property_o_t *prop)
{
    DBusMessage *sig;
    DBusMessageIter msg_iter;

    if (!prop)
        return;

    mrp_log_info("propertyChanged signal (%s)", prop->name);

    sig = dbus_message_new_signal(prop->path, prop->interface,
            SIG_PROPERTYCHANGED);

    dbus_message_iter_init_append(sig, &msg_iter);

    get_property_entry(prop, &msg_iter);

    mrp_dbus_send_msg(ctx->dbus, sig);
    dbus_message_unref(sig);
}


static void destroy_property(property_o_t *prop)
{
    if (!prop)
        return;

    mrp_free(prop->dbus_sig);
    mrp_free(prop->interface);
    mrp_free(prop->path);
    mrp_free(prop->name);

    if (prop->free_data)
        prop->free_data(prop->value);

    mrp_free(prop);
}


static property_o_t *create_property(dbus_data_t *ctx, char *path,
        const char *interface, const char *sig, const char *name, void *value,
        void (*free_data)(void *data))
{
    property_o_t *prop = mrp_allocz(sizeof(property_o_t));

    if (!prop)
        goto error;

    prop->dbus_sig = mrp_strdup(sig);
    prop->interface = mrp_strdup(interface);
    prop->path = mrp_strdup(path);
    prop->name = mrp_strdup(name);
    prop->writable = FALSE;
    prop->value = value;

    prop->ctx = ctx;

    prop->free_data = free_data;

    if (!prop->dbus_sig || !prop->name || !prop->value)
        goto error;

    trigger_property_changed_signal(ctx, prop);

    return prop;

error:
    if (prop) {
        destroy_property(prop);
    }
    else {
        if (free_data)
            free_data(value);
    }

    return NULL;
}


static void update_property(property_o_t *prop, void *value)
{
    /* the value is of the same type so we'll use the same function for
     * freeing it */

    if (prop->free_data)
        prop->free_data(prop->value);

    prop->value = value;

    trigger_property_changed_signal(prop->ctx, prop);
}


static void destroy_resource(resource_o_t *resource)
{
    if (!resource)
        return;

    mrp_log_info("destroy resource %s", resource->path);

    mrp_dbus_remove_method(resource->rset->mgr->ctx->dbus, resource->path,
            RESOURCE_IFACE, RESOURCE_GET_PROPERTIES, resource_cb,
            resource->rset->mgr->ctx);
    mrp_dbus_remove_method(resource->rset->mgr->ctx->dbus, resource->path,
            RESOURCE_IFACE, RESOURCE_SET_PROPERTY, resource_cb,
            resource->rset->mgr->ctx);
    mrp_dbus_remove_method(resource->rset->mgr->ctx->dbus, resource->path,
            RESOURCE_IFACE, RESOURCE_DELETE, resource_cb,
            resource->rset->mgr->ctx);

    destroy_property(resource->mandatory_prop);
    destroy_property(resource->shared_prop);
    destroy_property(resource->name_prop);
    destroy_property(resource->status_prop);
    destroy_property(resource->arguments_prop);
    destroy_property(resource->conf_prop);

    /* FIXME: resource library doesn't allow destroying resources? */

    mrp_free(resource->path);

    mrp_free(resource);
}


struct search_data_s {
    const char *name;
    resource_o_t *resource;
};


static int find_resource_cb(void *key, void *object, void *user_data)
{
    resource_o_t *r = object;
    struct search_data_s *s = user_data;
    MRP_UNUSED(key);

    if (strcmp(r->name_prop->value, s->name) == 0) {
        s->resource = r;
        return MRP_HTBL_ITER_STOP;
    }

    return MRP_HTBL_ITER_MORE;
}


static resource_o_t *get_resource_by_name(resource_set_o_t *rset,
        const char *name)
{
    struct search_data_s s;

    s.name = name;
    s.resource = NULL;

    mrp_htbl_foreach(rset->resources, find_resource_cb, &s);

    return s.resource;
}


static void event_cb(uint32_t request_id, mrp_resource_set_t *set, void *data)
{
    resource_set_o_t *rset = data;
    mrp_resource_t *resource;
    void *iter = NULL;

    mrp_resource_mask_t grant = mrp_get_resource_set_grant(set);
    mrp_resource_mask_t advice = mrp_get_resource_set_advice(set);

    MRP_UNUSED(request_id);

    mrp_log_info("Event for %s: grant 0x%08x, advice 0x%08x",
        rset->path, grant, advice);

    if (!rset->set || !rset->acquired) {
        /* We haven't yet returned from the create_set call, and this is before
         * acquiring the set, or we haven't started the acquitision yet. Filter
         * out! */
        mrp_log_info("Filtering out the event");

        return;
    }

    /* the resource API is bit awkward here */

    while ((resource = mrp_resource_set_iterate_resources(set, &iter))) {
        mrp_resource_mask_t mask;
        const char *name;
        resource_o_t *res;

        mask = mrp_resource_get_mask(resource);
        name = mrp_resource_get_name(resource);

        /* search the matching resource set object */

        res = get_resource_by_name(rset, name);

        if (!res) {
            mrp_log_error("Resource %s not found", name);
            continue;
        }

        if (mask & grant) {
            update_property(res->status_prop, "acquired");
        }
        else if (mask & advice) {
            update_property(res->status_prop, "available");
        }
        else {
            update_property(res->status_prop, "lost");
        }
    }

    if (grant) {
        update_property(rset->status_prop, "acquired");
    }
    else if (advice) {
        update_property(rset->status_prop, "available");
    }
    else {
        update_property(rset->status_prop, "lost");
    }
}


static void htbl_free_resources(void *key, void *object)
{
    resource_o_t *resource = object;

    MRP_UNUSED(key);

    destroy_resource(resource);
}


static void htbl_free_args(void *key, void *object)
{
    mrp_attr_t *attr = object;

    MRP_UNUSED(key);

    if (attr->type == mqi_string)
        mrp_free((void *) attr->value.string);

    mrp_free((void *) attr->name);
    mrp_free(attr);
}


static void free_map(void *object)
{
    mrp_htbl_t *ht = object;
    mrp_htbl_destroy(ht, TRUE);
}


static resource_o_t * create_resource(resource_set_o_t *rset,
        const char *resource_name, uint32_t id)
{
    char buf[MAX_PATH_LENGTH];
    int ret;
    char *name = NULL;
    dbus_bool_t *mandatory = NULL, *shared = NULL;

    /* attribute handling */
    mrp_attr_t attr_buf[128];
    mrp_attr_t *i;
    uint32_t resource_id;
    mrp_attr_t *attrs;
    mrp_attr_t *copy;

    mrp_htbl_config_t map_conf;
    mrp_htbl_t *conf;

    resource_o_t *resource = mrp_allocz(sizeof(resource_o_t));

    if (!resource)
        goto error;

    ret = snprintf(buf, MAX_PATH_LENGTH, "%s/%u", rset->path, id);

    if (ret < 0 || ret >= MAX_PATH_LENGTH)
        goto error;

    mandatory = mrp_allocz(sizeof(dbus_bool_t));
    shared = mrp_allocz(sizeof(dbus_bool_t));
    name = mrp_strdup(resource_name);

    if (!mandatory || !shared || !name) {
        mrp_free(mandatory);
        mrp_free(shared);
        mrp_free(name);
        goto error;
    }

    *mandatory = TRUE;
    *shared = FALSE;

    map_conf.comp = mrp_string_comp;
    map_conf.hash = mrp_string_hash;
    map_conf.free = htbl_free_args;
    map_conf.nbucket = 0;
    map_conf.nentry = 10;

    resource->mandatory_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "b", PROP_MANDATORY, mandatory, free_value);
    resource->mandatory_prop->writable = TRUE;

    if (!resource->mandatory_prop) {
        mrp_free(mandatory);
        mrp_free(shared);
        mrp_free(name);
        goto error;
    }

    resource->shared_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "b", PROP_SHARED, shared, free_value);
    resource->shared_prop->writable = TRUE;

    if (!resource->shared_prop) {
        mrp_free(shared);
        mrp_free(name);
        goto error;
    }

    resource->name_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "s", PROP_NAME, name, free_value);

    if (!resource->name_prop) {
        mrp_free(name);
        goto error;
    }

    resource->status_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "s", PROP_STATUS, "pending", NULL);

    if (!resource->status_prop)
        goto error;

    resource_id = mrp_resource_definition_get_resource_id_by_name(name);

    attrs = mrp_resource_definition_read_all_attributes(resource_id, 128,
            attr_buf);
    i = attrs;

    resource->rset = rset;
    resource->path = mrp_strdup(buf);

    if (!resource->path)
        goto error;

    conf = mrp_htbl_create(&map_conf);

    if (!conf)
        goto error;

    while (i->name != NULL) {

        copy = mrp_allocz(sizeof(mrp_attr_t));

        if (!copy)
            goto error_delete_conf;

        memcpy(copy, i, sizeof(mrp_attr_t));
        copy->name = mrp_strdup(i->name);

        if (!copy->name) {
            mrp_free(copy);
            goto error_delete_conf;
        }

        if (i->type == mqi_string) {
            copy->value.string = mrp_strdup(i->value.string);
            if (!copy->value.string) {
                mrp_free((void *) copy->name);
                mrp_free(copy);
                goto error_delete_conf;
            }
        }
        mrp_htbl_insert(conf, (void *) copy->name, copy);
        i++;
    }

    resource->conf_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "a{sv}", PROP_ATTRIBUTES_CONF, conf, free_map);

    if (!resource->conf_prop) {
        goto error;
    }

    resource->arguments_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "a{sv}", PROP_ATTRIBUTES, conf, NULL);

    if (!resource->arguments_prop) {
        goto error;
    }

    return resource;

error_delete_conf:
    mrp_htbl_destroy(conf, TRUE);

error:
    if (resource)
        destroy_resource(resource);

    return NULL;
}


static void destroy_rset(resource_set_o_t *rset)
{
    dbus_data_t *ctx;

    if (!rset)
        return;

    ctx = rset->mgr->ctx;

    mrp_log_info("destroy rset %s", rset->path);

    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE, RSET_DELETE,
            rset_cb, ctx);
    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE, RSET_RELEASE,
            rset_cb, ctx);
    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE, RSET_REQUEST,
            rset_cb, ctx);
    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE, RSET_ADD_RESOURCE,
            rset_cb, ctx);
    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE, RSET_SET_PROPERTY,
            rset_cb, ctx);
    mrp_dbus_remove_method(ctx->dbus, rset->path, RSET_IFACE,
            RSET_GET_PROPERTIES, rset_cb, ctx);

    if (rset->resources)
        mrp_htbl_destroy(rset->resources, TRUE);

    destroy_property(rset->class_prop);
    destroy_property(rset->status_prop);
    destroy_property(rset->resources_prop);
    destroy_property(rset->available_resources_prop);

    if (ctx->tracking)
        mrp_dbus_forget_name(ctx->dbus, rset->owner, dbus_name_cb, rset);

    if (rset->set) {
        mrp_resource_set_destroy(rset->set);
        rset->set = NULL;
    }

    mrp_free(rset->path);
    mrp_free(rset->owner);

    mrp_free(rset);
}


static resource_set_o_t * create_rset(manager_o_t *mgr, uint32_t id,
            const char *sender)
{
    char buf[MAX_PATH_LENGTH];
    char resbuf[128];
    int ret;
    mrp_htbl_config_t resources_conf;
    resource_set_o_t *rset = NULL;
    char **resources_arr;
    char **available_resources_arr;

    if (!sender)
        goto error;

    rset = mrp_allocz(sizeof(resource_set_o_t));

    if (!rset)
        goto error;

    ret = snprintf(buf, MAX_PATH_LENGTH, "%s/%u", MURPHY_PATH_BASE, id);

    if (ret < 0 || ret >= MAX_PATH_LENGTH)
        goto error;

    rset->mgr = mgr;
    rset->path = mrp_strdup(buf);

    if (!rset->path)
        goto error;

    resources_conf.comp = mrp_string_comp;
    resources_conf.hash = mrp_string_hash;
    resources_conf.free = htbl_free_resources;
    resources_conf.nbucket = 0;
    resources_conf.nentry = 10;

    rset->resources = mrp_htbl_create(&resources_conf);

    if (!rset->resources)
        goto error;

    resources_arr = mrp_allocz(sizeof(char **));
    if (!resources_arr)
        goto error;
    resources_arr[0] = NULL;

    rset->resources_prop = create_property(mgr->ctx, rset->path,
            RSET_IFACE, "ao", PROP_RESOURCES, resources_arr, free_string_array);

    if (!rset->resources_prop)
        goto error;

    rset->class_prop = create_property(mgr->ctx, rset->path,
            RSET_IFACE, "s", PROP_CLASS,
            mrp_strdup(rset->mgr->ctx->default_class), free_value);
    rset->class_prop->writable = TRUE;

    if (!rset->class_prop)
        goto error;

    rset->status_prop = create_property(mgr->ctx, rset->path,
            RSET_IFACE, "s", PROP_STATUS, "pending", NULL);

    if (!rset->status_prop)
        goto error;

    available_resources_arr = copy_string_array(
                mrp_resource_definition_get_all_names(128,
                        (const char **) resbuf));

    rset->available_resources_prop = create_property(mgr->ctx,
            rset->path, RSET_IFACE, "as", PROP_AVAILABLE_RESOURCES,
            available_resources_arr, free_string_array);

    if (!rset->available_resources_prop)
        goto error;

    rset->owner = mrp_strdup(sender);

    if (!rset->owner)
        goto error;

    /* start following the owner */
    if (mgr->ctx->tracking)
        mrp_dbus_follow_name(mgr->ctx->dbus, rset->owner, dbus_name_cb, rset);

    rset->set = mrp_resource_set_create(mgr->client, 0, 0, 0, event_cb,
                rset);

    if (!rset->set) {
        mrp_log_error("Failed to create resource set");
        goto error;
    }

    return rset;

error:
    if (rset) {
        destroy_rset(rset);
    }

    return NULL;
}


static void dbus_name_cb(mrp_dbus_t *dbus, const char *name, int up,
                          const char *owner, void *user_data)
{
    mrp_log_info("dbus_name_cb: %s status %d, owner %s", name, up, owner);

    MRP_UNUSED(dbus);

    if (up == 0) {
        /* a client that we've been tracking has just died */
        resource_set_o_t *rset = user_data;
        manager_o_t *mgr = rset->mgr;
        mrp_htbl_remove(mgr->rsets, (void *) rset->path, TRUE);
        update_property(mgr->rsets_prop, htbl_keys(mgr->rsets));
    }
}


static void htbl_free_rsets(void *key, void *object)
{
    resource_set_o_t *rset = object;

    MRP_UNUSED(key);

    destroy_rset(rset);
}


static int parse_path(const char *path, uint32_t *rset_id,
        uint32_t *resource_id)
{
    *rset_id = -1;
    *resource_id = -1;

    int base_len = strlen(MURPHY_PATH_BASE);
    int path_len = strlen(path);

    char *p = (char *) path;
    char *first_sep = NULL;
    char *second_sep = NULL;
    char *guard = (char *) path + path_len;

    if (base_len < 3)
        return FALSE; /* parsing corner case */

    if (path_len < base_len + 4)
        return FALSE; /* need to have at least "/1/2" */

    if (strncmp(path, MURPHY_PATH_BASE, base_len) != 0)
        return FALSE;

    p += base_len;

    if (*p != '/')
        return FALSE;

    first_sep = p;

    p++;

    while (p != guard) {
        if (*p == '/') {
            second_sep = p;
        }
        p++;
    }

    if (!second_sep)
        return FALSE;

    if (second_sep + 1 == guard)
        return FALSE; /* missing resource id */

    /* ok, the rset_id is between first_sep and second_sep, and
     * resource_id is between second_sep and guard */

    p = NULL;

    *rset_id = strtol(first_sep + 1, &p, 10);

    if (p != second_sep)
        return FALSE;

    *resource_id = strtol(second_sep + 1, &p, 10);

    if (p != guard)
        return FALSE;

    return TRUE;
}


struct attr_iter_s {
    mrp_attr_t *attrs;
    int count;
};


static int collect_attrs_cb(void *key, void *object, void *user_data)
{
    mrp_attr_t *attr = object;
    mrp_attr_t *copy;
    struct attr_iter_s *s = user_data;
    MRP_UNUSED(key);

    copy = &s->attrs[s->count];

    memcpy(copy, attr, sizeof(mrp_attr_t));

    if (attr->type == mqi_string) {
        copy->value.string = mrp_strdup(attr->value.string);
    }
    copy->name = mrp_strdup(attr->name);

    s->count++;

    return MRP_HTBL_ITER_MORE;
}


static void update_attributes(const char *resource_name,
        mrp_resource_set_t *set, mrp_htbl_t *attr_map)
{
    int count = 0;
    mrp_htbl_foreach(attr_map, count_keys_cb, &count);

    {
        struct attr_iter_s iter;
        mrp_attr_t attrs[count+1];

        memset(attrs, 0, (count+1)*sizeof(mrp_attr_t));

        iter.count = 0;
        iter.attrs = attrs;

        /* add the attributes */
        mrp_htbl_foreach(attr_map, collect_attrs_cb, &iter);

        /* FIXME: this breaks down if there are two resources of the same name
         * in a resource set */

        mrp_resource_set_write_attributes(set, resource_name, attrs);
        free_attr_array(attrs);
    }
}


static int update_conf_cb(void *key, void *object, void *user_data)
{
    mrp_htbl_t **confs = user_data;
    mrp_attr_t *old_attr = object;

    mrp_htbl_t *new_conf = confs[0];

    if (mrp_htbl_lookup(new_conf, key) == NULL) {
        /* copy the attribute */
        mrp_attr_t *attr = mrp_allocz(sizeof(mrp_attr_t));

        if (!attr) {
            goto error;
        }
        attr->name = mrp_strdup(old_attr->name);
        if (!attr->name) {
            mrp_free(attr);
            goto error;
        }
        attr->type = old_attr->type;
        switch (attr->type) {
            case mqi_string:
                attr->value.string = mrp_strdup(old_attr->value.string);
                if (!attr->value.string) {
                    mrp_free((void *) attr->name);
                    mrp_free(attr);
                    goto error;
                }
                break;
            case mqi_integer:
                attr->value.integer = old_attr->value.integer;
                break;
            case mqi_unsignd:
                attr->value.unsignd = old_attr->value.unsignd;
                break;
            case mqi_floating:
                attr->value.floating = old_attr->value.floating;
                break;
            default:
                goto error;
        }
        /* add the value to the conf */
        mrp_htbl_insert(new_conf, (void *) attr->name, attr);
    }

    confs[1] = new_conf; /* indicate success */

    return MRP_HTBL_ITER_MORE;

error:
    confs[1] = NULL; /* indicate error */
    return MRP_HTBL_ITER_STOP;
}



static int resource_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char *member = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *path = dbus_message_get_path(msg);
    char *error_msg = "Received invalid message";

    DBusMessage *reply;
    char buf[MAX_PATH_LENGTH];

    dbus_data_t *ctx = data;

    uint32_t rset_id, resource_id;

    resource_set_o_t *rset;
    resource_o_t *resource;

    int ret;

    mrp_log_info("Resource callback called -- member: '%s', path: '%s',"
            " interface: '%s'", member, path, iface);

    /* parse the rset id and resource id */

    if (!parse_path(path, &rset_id, &resource_id)) {
        mrp_log_error("Failed to parse path");
        goto error_reply;
    }

    ret = snprintf(buf, MAX_PATH_LENGTH, "%s/%u", MURPHY_PATH_BASE, rset_id);

    if (ret < 0 || ret >= MAX_PATH_LENGTH)
        goto error_reply;

    rset = mrp_htbl_lookup(ctx->mgr->rsets, buf);

    if (!rset)
        goto error_reply;

    resource = mrp_htbl_lookup(rset->resources, (void *) path);

    if (!resource)
        goto error_reply;

    if (strcmp(member, RESOURCE_GET_PROPERTIES) == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter array_iter;

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_log_info("getProperties of resource %s", path);

        dbus_message_iter_init_append(reply, &msg_iter);

        dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}",
                &array_iter);

        get_property_dict_entry(resource->name_prop, &array_iter);
        get_property_dict_entry(resource->status_prop, &array_iter);
        get_property_dict_entry(resource->mandatory_prop, &array_iter);
        get_property_dict_entry(resource->shared_prop, &array_iter);
        get_property_dict_entry(resource->arguments_prop, &array_iter);
        get_property_dict_entry(resource->conf_prop, &array_iter);

        dbus_message_iter_close_container(&msg_iter, &array_iter);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RESOURCE_SET_PROPERTY) == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter variant_iter;

        const char *name;

        mrp_log_info("setProperty of resource %s", path);

        dbus_message_iter_init(msg, &msg_iter);

        if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_STRING) {
            error_msg = "Missing resource property key";
            goto error_reply;
        }

        dbus_message_iter_get_basic(&msg_iter, &name);

        if (!dbus_message_iter_has_next(&msg_iter)) {
            error_msg = "Missing resource property value";
            goto error_reply;
        }

        dbus_message_iter_next(&msg_iter);

        if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_VARIANT) {
            error_msg = "Resource property value not wrapped in variant";
            goto error_reply;
        }

        dbus_message_iter_recurse(&msg_iter, &variant_iter);

        if (resource->rset->locked && strcmp(name, PROP_ATTRIBUTES_CONF) != 0) {
            error_msg = "Resource set cannot be changed after requesting";
            goto error_reply;
        }

        if (strcmp(name, PROP_MANDATORY) == 0) {
            dbus_bool_t value;
            dbus_bool_t *tmp = mrp_alloc(sizeof(dbus_bool_t));

            if (dbus_message_iter_get_arg_type(&variant_iter)
                        != DBUS_TYPE_BOOLEAN) {
                mrp_free(tmp);
                goto error_reply;
            }

            dbus_message_iter_get_basic(&variant_iter, &value);

            *tmp = value;
            update_property(resource->mandatory_prop, tmp);
        }
        else if (strcmp(name, PROP_SHARED) == 0) {
            dbus_bool_t value;
            dbus_bool_t *tmp = mrp_alloc(sizeof(dbus_bool_t));

            if (dbus_message_iter_get_arg_type(&variant_iter)
                        != DBUS_TYPE_BOOLEAN) {
                mrp_free(tmp);
                goto error_reply;
            }

            dbus_message_iter_get_basic(&variant_iter, &value);

            *tmp = value;
            update_property(resource->shared_prop, tmp);
        }
        else if (strcmp(name, PROP_ATTRIBUTES_CONF) == 0) {
            DBusMessageIter a_iter;
            DBusMessageIter d_iter;
            DBusMessageIter v_iter;
            mrp_htbl_config_t map_conf;
            mrp_htbl_t *conf;
            int value_type;
            int new_count = 0;
            int old_count = 0;

            map_conf.comp = mrp_string_comp;
            map_conf.hash = mrp_string_hash;
            map_conf.free = htbl_free_args;
            map_conf.nbucket = 0;
            map_conf.nentry = 10;

            conf = mrp_htbl_create(&map_conf);

            if (!conf)
                goto error_reply;

            if (dbus_message_iter_get_arg_type(&variant_iter)
                        != DBUS_TYPE_ARRAY) {
                mrp_htbl_destroy(conf, TRUE);
                error_msg = "Resource configuration attribute array missing";
                goto error_reply;
            }

            dbus_message_iter_recurse(&variant_iter, &a_iter);

            while (dbus_message_iter_get_arg_type(&a_iter)
                    != DBUS_TYPE_INVALID) {
                char *key;
                mrp_attr_t *prev_value;
                mrp_attr_t *new_value;

                if (dbus_message_iter_get_arg_type(&a_iter)
                            != DBUS_TYPE_DICT_ENTRY) {
                    mrp_htbl_destroy(conf, TRUE);
                    error_msg = "Configuration attribute array"
                                        "doesn't contain dictionary entries";
                    goto error_reply;
                }

                dbus_message_iter_recurse(&a_iter, &d_iter);

                if (dbus_message_iter_get_arg_type(&d_iter)
                            != DBUS_TYPE_STRING) {
                    mrp_htbl_destroy(conf, TRUE);
                    error_msg = "Configuration attribute key missing";
                    goto error_reply;
                }

                dbus_message_iter_get_basic(&d_iter, &key);

                prev_value = mrp_htbl_lookup(resource->conf_prop->value, key);
                if (!prev_value) {
                    mrp_log_error("no previous value %s in attributes", key);
                    error_msg = "Configuration attribute definition missing";
                    mrp_htbl_destroy(conf, TRUE);
                    goto error_reply;
                }

                if (!dbus_message_iter_has_next(&d_iter)) {
                    mrp_htbl_destroy(conf, TRUE);
                    error_msg = "Configuration attribute value missing";
                    goto error_reply;
                }
                dbus_message_iter_next(&d_iter);

                if (dbus_message_iter_get_arg_type(&d_iter)
                            != DBUS_TYPE_VARIANT) {
                    mrp_htbl_destroy(conf, TRUE);
                    error_msg = "Attribute value not wrapped in variant";
                    goto error_reply;
                }

                dbus_message_iter_recurse(&d_iter, &v_iter);

                new_value = mrp_allocz(sizeof(mrp_attr_t));
                if (!new_value) {
                    error_msg = "Internal error";
                    goto error_reply;
                }

                value_type = dbus_message_iter_get_arg_type(&v_iter);

                switch (value_type) {
                    case DBUS_TYPE_STRING:
                    {
                        char *value;

                        if (prev_value->type != mqi_string) {
                            mrp_htbl_destroy(conf, TRUE);
                            mrp_free(new_value);
                            error_msg = "Attribute value not string";
                            goto error_reply;
                        }

                        dbus_message_iter_get_basic(&v_iter, &value);
                        new_value->name = mrp_strdup(key);
                        new_value->type = mqi_string;
                        new_value->value.string = mrp_strdup(value);
                        break;
                    }
                    case DBUS_TYPE_INT32:
                    {
                        int32_t value;

                        if (prev_value->type != mqi_integer) {
                            mrp_htbl_destroy(conf, TRUE);
                            mrp_free(new_value);
                            error_msg = "Attribute value not int32";
                            goto error_reply;
                        }

                        dbus_message_iter_get_basic(&v_iter, &value);
                        new_value->name = mrp_strdup(key);
                        new_value->type = mqi_integer;
                        new_value->value.integer = value;
                        break;
                    }
                    case DBUS_TYPE_UINT32:
                    {
                        uint32_t value;

                        if (prev_value->type != mqi_unsignd) {
                            mrp_htbl_destroy(conf, TRUE);
                            mrp_free(new_value);
                            error_msg = "Attribute value not uint32";
                            goto error_reply;
                        }

                        dbus_message_iter_get_basic(&v_iter, &value);
                        new_value->name = mrp_strdup(key);
                        new_value->type = mqi_unsignd;
                        new_value->value.unsignd = value;
                        break;
                    }
                    case DBUS_TYPE_DOUBLE:
                    {
                        double value;

                        if (prev_value->type != mqi_floating) {
                            mrp_htbl_destroy(conf, TRUE);
                            mrp_free(new_value);
                            error_msg = "Attribute value not double";
                            goto error_reply;
                        }

                        dbus_message_iter_get_basic(&v_iter, &value);
                        new_value->name = mrp_strdup(key);
                        new_value->type = mqi_floating;
                        new_value->value.floating = value;
                        break;
                    }
                    default:
                        mrp_htbl_destroy(conf, TRUE);
                        mrp_free(new_value);
                        error_msg = "Attribute value unknown";
                        goto error_reply;
                }

                mrp_htbl_insert(conf, (void *) new_value->name, new_value);

                new_count++;
                dbus_message_iter_next(&a_iter);
            }

            /* What about if not all properties were set? Maybe
             * update_property should merge the old map with the new map.
             * For now, just check the the size is the same. */

            mrp_htbl_foreach(resource->conf_prop->value, count_keys_cb,
                    &old_count);

            if (old_count > new_count) {
                /* for every key in old conf, add the key to new conf if it's
                 * not there already */

                /* the second value is return value for errors */
                mrp_htbl_t *confs[2] = { conf, NULL };

                mrp_htbl_foreach(resource->conf_prop->value, update_conf_cb,
                        confs);

                if (confs[1] == NULL) {
                    mrp_htbl_destroy(conf, TRUE);
                    error_msg = "attribute merging failed";
                    goto error_reply;
                }
            }
            else if (old_count < new_count) {
                mrp_htbl_destroy(conf, TRUE);
                error_msg = "setting too many attributes";
                goto error_reply;
            }

            update_property(resource->conf_prop, conf);
            update_property(resource->arguments_prop, conf);

            if (resource->rset->locked) {
                /* if the resource set is already created for the library,
                 * we can set the attributes */
                update_attributes(resource->name_prop->value,
                        resource->rset->set, conf);
            }
        }
        else {
            error_msg = "Resource property read-only or missing";
            goto error_reply;
        }

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RESOURCE_DELETE) == 0) {
        mrp_log_info("Deleting resource %s", path);

        mrp_htbl_remove(rset->resources, (void *) path, TRUE);
        update_property(rset->resources_prop, htbl_keys(rset->resources));

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

    return TRUE;

error_reply:
    reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, error_msg);

    if (reply) {
        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

    return FALSE;

error:
    return FALSE;
}


static int add_resource_cb(void *key, void *object, void *user_data)
{
    resource_o_t *r = object;
    resource_set_o_t *rset = user_data;

    bool shared = *(dbus_bool_t *) r->shared_prop->value;
    bool mandatory = *(dbus_bool_t *) r->mandatory_prop->value;
    char *name = r->name_prop->value;

    int count = 0;

    MRP_UNUSED(key);

    /* count the attributes */
    mrp_htbl_foreach(r->conf_prop->value, count_keys_cb, &count);

    if (mrp_resource_set_add_resource(rset->set, name, shared, NULL, mandatory)
                >= 0) {
        update_attributes(name, rset->set, r->conf_prop->value);
    }

    return MRP_HTBL_ITER_MORE;
}


static int rset_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char *member = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *path = dbus_message_get_path(msg);
    char *error_msg = "Received invalid message";

    DBusMessage *reply;

    dbus_data_t *ctx = data;

    resource_set_o_t *rset = mrp_htbl_lookup(ctx->mgr->rsets, (void *) path);

    mrp_log_info("Resource set callback called -- member: '%s', path: '%s',"
            " interface: '%s'", member, path, iface);

    if (!rset) {
        mrp_log_error("Resource set '%s' not found, ignoring", path);
        goto error;
    }

    if (strcmp(member, RSET_GET_PROPERTIES) == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter array_iter;

        /* FIXME: check return values */

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_log_info("getProperties of rset %s", path);

        dbus_message_iter_init_append(reply, &msg_iter);

        dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}",
                &array_iter);

        get_property_dict_entry(rset->class_prop, &array_iter);
        get_property_dict_entry(rset->status_prop, &array_iter);
        get_property_dict_entry(rset->resources_prop, &array_iter);
        get_property_dict_entry(rset->available_resources_prop, &array_iter);

        dbus_message_iter_close_container(&msg_iter, &array_iter);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RSET_ADD_RESOURCE) == 0) {
        const char *name;

        resource_o_t *resource;

        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID)) {
            goto error_reply;
        }

        resource = create_resource(rset, name, rset->next_id++);

        if (!resource)
            goto error_reply;

        if (!mrp_dbus_export_method(ctx->dbus, resource->path,
                    RESOURCE_IFACE, RESOURCE_GET_PROPERTIES, resource_cb,
                    ctx)) {
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, resource->path,
                    RESOURCE_IFACE, RESOURCE_SET_PROPERTY, resource_cb, ctx)) {
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, resource->path,
                    RESOURCE_IFACE, RESOURCE_DELETE, resource_cb, ctx)) {
            goto error_reply;
        }

        mrp_htbl_insert(rset->resources, (void *) resource->path,
                resource);
        update_property(rset->resources_prop, htbl_keys(rset->resources));

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &resource->path,
                DBUS_TYPE_INVALID);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RSET_REQUEST) == 0) {
        mrp_log_info("Requesting rset %s", path);

        if (!rset->locked) {
            /* add the resources */
            mrp_htbl_foreach(rset->resources, add_resource_cb, rset);

            if (mrp_application_class_add_resource_set(
                    (char *) rset->class_prop->value,
                    rset->mgr->zone, rset->set, 0) < 0) {
                /* This is actually quite serious, since most likely we cannot
                 * ever get this to work. The zone is most likely not defined.
                 * The resource library is known to crash if the rset->set
                 * pointer is used for acquiring.
                 */
                goto error;
            }
        }

        rset->acquired = TRUE;
        mrp_resource_set_acquire(rset->set, 0);

        /* Due to limitations in resource library, this resource set cannot
         * be changed anymore. This might change in the future.
         */
        rset->locked = TRUE;

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RSET_RELEASE) == 0) {
        mrp_log_info("Releasing rset %s", path);

        mrp_resource_set_release(rset->set, 0);

        reply = dbus_message_new_method_return(msg);
        if (!reply) {
            goto error;
        }

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RSET_DELETE) == 0) {
        mrp_log_info("Deleting rset %s", path);

        mrp_htbl_remove(ctx->mgr->rsets, (void *) path, TRUE);
        update_property(ctx->mgr->rsets_prop, htbl_keys(ctx->mgr->rsets));

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, RSET_SET_PROPERTY) == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter variant_iter;

        const char *name;
        const char *value;

        if (rset->locked) {
            error_msg = "Resource set cannot be changed after requesting";
            goto error_reply;
        }

        dbus_message_iter_init(msg, &msg_iter);

        if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_STRING) {
            goto error_reply;
        }

        dbus_message_iter_get_basic(&msg_iter, &name);

        if (!dbus_message_iter_has_next(&msg_iter)) {
            goto error_reply;
        }

        dbus_message_iter_next(&msg_iter);

        if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_VARIANT) {
            goto error_reply;
        }

        dbus_message_iter_recurse(&msg_iter, &variant_iter);

        if (strcmp(name, PROP_CLASS) == 0) {
            if (dbus_message_iter_get_arg_type(&variant_iter)
                        != DBUS_TYPE_STRING) {
                goto error_reply;
            }

            dbus_message_iter_get_basic(&variant_iter, &value);

            update_property(rset->class_prop, mrp_strdup(value));
        }
        else {
            goto error_reply;
        }

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

    return TRUE;

error_reply:
    reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, error_msg);

    if (reply) {
        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

error:
    return FALSE;
}


static int mgr_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char *member = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *path = dbus_message_get_path(msg);

    DBusMessage *reply;

    dbus_data_t *ctx = data;

    mrp_log_info("Manager callback called -- member: '%s', path: '%s',"
            " interface: '%s'", member, path, iface);

    if (strcmp(member, MANAGER_GET_PROPERTIES) == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter array_iter;

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_log_info("getProperties of manager %s", path);

        dbus_message_iter_init_append(reply, &msg_iter);

        dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}",
                &array_iter);

        get_property_dict_entry(ctx->mgr->rsets_prop, &array_iter);

        dbus_message_iter_close_container(&msg_iter, &array_iter);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, MANAGER_CREATE_RESOURCE_SET) == 0) {
        const char *sender = dbus_message_get_sender(msg);
        resource_set_o_t *rset = create_rset(ctx->mgr, ctx->mgr->next_id++,
                sender);

        if (!rset)
            goto error_reply;

        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_GET_PROPERTIES, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_SET_PROPERTY, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_ADD_RESOURCE, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_REQUEST, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_RELEASE, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, RSET_DELETE, rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }

        mrp_htbl_insert(ctx->mgr->rsets, (void *) rset->path, rset);
        update_property(ctx->mgr->rsets_prop, htbl_keys(ctx->mgr->rsets));

        reply = dbus_message_new_method_return(msg);
        if (!reply) {
            destroy_rset(rset);
            goto error;
        }

        dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &rset->path,
                DBUS_TYPE_INVALID);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

    return TRUE;

error_reply:
    reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED,
                "Received invalid message");
    if (reply) {
        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }

error:
    return FALSE;
}


static void destroy_manager(manager_o_t *mgr)
{
    if (!mgr)
        return;

    mrp_dbus_remove_method(mgr->ctx->dbus, MURPHY_PATH_BASE,
            MANAGER_IFACE, MANAGER_CREATE_RESOURCE_SET, mgr_cb, mgr->ctx);

    mrp_dbus_remove_method(mgr->ctx->dbus, MURPHY_PATH_BASE,
            MANAGER_IFACE, MANAGER_GET_PROPERTIES, mgr_cb, mgr->ctx);

    mrp_htbl_destroy(mgr->rsets, TRUE);
    destroy_property(mgr->rsets_prop);

    mrp_resource_client_destroy(mgr->client);

    mrp_free(mgr);
}


static manager_o_t *create_manager(dbus_data_t *ctx)
{
    manager_o_t *mgr = mrp_allocz(sizeof(manager_o_t));
    char **rset_arr = NULL;
    mrp_htbl_config_t rsets_conf;

    if (!mgr)
        goto error;

    mgr->ctx = ctx;

    rset_arr = mrp_allocz(sizeof(char **));
    if (!rset_arr)
        goto error;
    rset_arr[0] = NULL;

    /* FIXME: duplication of code? */

    mgr->rsets_prop = create_property(ctx, MURPHY_PATH_BASE, MANAGER_IFACE,
            "ao", PROP_RESOURCE_SETS, rset_arr, free_string_array);

    if (!mgr->rsets_prop)
        goto error;

    rsets_conf.comp = mrp_string_comp;
    rsets_conf.hash = mrp_string_hash;
    rsets_conf.free = htbl_free_rsets;
    rsets_conf.nbucket = 0;
    rsets_conf.nentry = 10;

    mgr->rsets = mrp_htbl_create(&rsets_conf);

    if (!mgr->rsets)
        goto error;

    mgr->client = mrp_resource_client_create("dbus", ctx);

    if (!mgr->client)
        goto error;

    mgr->zone = ctx->default_zone;

    return mgr;

error:

    destroy_manager(mgr);

    return NULL;
}


static int dbus_resource_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    dbus_data_t *ctx = mrp_allocz(sizeof(dbus_data_t));

    if (!ctx)
        goto error;

    ctx->addr = args[ARG_DR_SERVICE].str;
    ctx->tracking = args[ARG_DR_TRACK_CLIENTS].bln;
    ctx->default_zone = args[ARG_DR_DEFAULT_ZONE].str;
    ctx->default_class = args[ARG_DR_DEFAULT_CLASS].str;
    ctx->bus = args[ARG_DR_BUS].str;
    ctx->dbus = mrp_dbus_connect(plugin->ctx->ml, ctx->bus, NULL);

    if (!ctx->dbus) {
        mrp_log_error("Failed to connect to D-Bus");
        goto error;
    }

    ctx->mgr = create_manager(ctx);

    if (!ctx->mgr) {
        mrp_log_error("Failed to create manager");
        goto error;
    }

    if (!mrp_dbus_acquire_name(ctx->dbus, ctx->addr, NULL)) {
        mrp_log_error("Failed to acquire name '%s' on D-Bus", ctx->addr);
        goto error;
    }

    /* in the beginning we only export the manager interface -- the
     * rest is created dynamically
     */

    if (!mrp_dbus_export_method(ctx->dbus, MURPHY_PATH_BASE,
                MANAGER_IFACE, MANAGER_CREATE_RESOURCE_SET, mgr_cb, ctx)) {
       mrp_log_error("Failed to register manager object");
       goto error;
    }

    if (!mrp_dbus_export_method(ctx->dbus, MURPHY_PATH_BASE,
                MANAGER_IFACE, MANAGER_GET_PROPERTIES, mgr_cb, ctx)) {
       mrp_log_error("Failed to register manager object");
       goto error;
    }

    return TRUE;

error:
    if (ctx) {
        destroy_manager(ctx->mgr);
    }

    return FALSE;
}


static void dbus_resource_exit(mrp_plugin_t *plugin)
{
    dbus_data_t *ctx = plugin->data;

    mrp_dbus_release_name(ctx->dbus, ctx->addr, NULL);
    mrp_dbus_unref(ctx->dbus);
    ctx->dbus = NULL;

    mrp_htbl_destroy(ctx->mgr->rsets, TRUE);
    destroy_manager(ctx->mgr);
    mrp_free(ctx);

    plugin->data = NULL;
}


#define DBUS_RESOURCE_DESCRIPTION "A plugin to implement D-Bus resource API."
#define DBUS_RESOURCE_HELP        "D-Bus resource manager backend"
#define DBUS_RESOURCE_VERSION     MRP_VERSION_INT(0, 0, 1)
#define DBUS_RESOURCE_AUTHORS     "Ismo Puustinen <ismo.puustinen@intel.com>"

/* TODO: more arguments needed, such as:
 *    - security settings?
 */
static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_DR_BUS, STRING, "dbus_bus", "system"),
    MRP_PLUGIN_ARGIDX(ARG_DR_SERVICE, STRING, "dbus_service", "org.Murphy"),
    MRP_PLUGIN_ARGIDX(ARG_DR_DEFAULT_ZONE, STRING, "default_zone", "default"),
    MRP_PLUGIN_ARGIDX(ARG_DR_DEFAULT_CLASS, STRING, "default_class", "default"),
    MRP_PLUGIN_ARGIDX(ARG_DR_TRACK_CLIENTS, BOOL, "dbus_track", TRUE),
};


MURPHY_REGISTER_PLUGIN("resource-dbus",
                       DBUS_RESOURCE_VERSION, DBUS_RESOURCE_DESCRIPTION,
                       DBUS_RESOURCE_AUTHORS, DBUS_RESOURCE_HELP,
                       MRP_MULTIPLE, dbus_resource_init, dbus_resource_exit,
                       args, MRP_ARRAY_SIZE(args),
                       NULL, 0, NULL, 0, NULL);
