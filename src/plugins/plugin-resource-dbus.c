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


#define HAVE_TO_DEFINE_RESOURCES

enum {
    ARG_DBUS_SERVICE,
};


typedef struct manager_o_s manager_o_t;

typedef struct {
    /* configuration */
    mrp_dbus_t *dbus;
    const char *addr;

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

    manager_o_t *mgr; /* backpointer */

    mrp_htbl_t *resources;


    property_o_t *resources_prop;
    property_o_t *available_resources_prop;
    property_o_t *class_prop;
    property_o_t *status_prop;

    /* resource library */
    mrp_resource_set_t *set;
} resource_set_o_t;


typedef struct {
    char *path;

    resource_set_o_t *rset; /* backpointer */

    property_o_t *status_prop;
    property_o_t *mandatory_prop;
    property_o_t *shared_prop;
    property_o_t *name_prop;
} resource_o_t;


static int mgr_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static int rset_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static int resource_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);


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

    keys = mrp_allocz(len+1);

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

    while (*i) {
        mrp_free(*i);
        i++;
    }

    mrp_free(array);
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
            "propertyChanged");

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
            RSET_IFACE, "delete", resource_cb, resource->rset->mgr->ctx);
    mrp_dbus_remove_method(resource->rset->mgr->ctx->dbus, resource->path,
            RSET_IFACE, "setProperty", resource_cb, resource->rset->mgr->ctx);
    mrp_dbus_remove_method(resource->rset->mgr->ctx->dbus, resource->path,
            RSET_IFACE, "getProperties", resource_cb, resource->rset->mgr->ctx);

    destroy_property(resource->mandatory_prop);
    destroy_property(resource->shared_prop);
    destroy_property(resource->name_prop);
    destroy_property(resource->status_prop);

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

    mrp_log_info("Received event from policy engine");

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


static resource_o_t * create_resource(resource_set_o_t *rset,
        const char *resource_name, uint32_t id)
{
    char buf[MAX_PATH_LENGTH];
    int ret;
    char *name;
    dbus_bool_t *mandatory, *shared;

    resource_o_t *resource = mrp_allocz(sizeof(resource_o_t));

    if (!resource)
        goto error;

    ret = snprintf(buf, MAX_PATH_LENGTH, "%s/%u", rset->path, id);

    if (ret == MAX_PATH_LENGTH)
        goto error;

    mandatory = mrp_allocz(sizeof(bool));
    shared = mrp_allocz(sizeof(bool));
    name = mrp_strdup(resource_name);

    *mandatory = TRUE;
    *shared = FALSE;

    resource->mandatory_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "b", "mandatory", mandatory, free_value);
    resource->mandatory_prop->writable = TRUE;

    if (!resource->mandatory_prop)
        goto error;

    resource->shared_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "b", "shared", shared, free_value);
    resource->shared_prop->writable = TRUE;

    if (!resource->shared_prop)
        goto error;

    resource->name_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "s", "name", name, free_value);

    if (!resource->name_prop)
        goto error;

    resource->status_prop = create_property(rset->mgr->ctx, buf,
            RESOURCE_IFACE, "s", "status", "pending", NULL);

    if (!resource->status_prop)
        goto error;

    resource->path = mrp_strdup(buf);
    resource->rset = rset;

    return resource;

error:
    if (resource)
        destroy_resource(resource);

    return NULL;
}


static void destroy_rset(resource_set_o_t *rset)
{
    if (!rset)
        return;

    mrp_log_info("destroy rset %s", rset->path);

    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "delete", rset_cb, rset->mgr->ctx);
    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "release", rset_cb, rset->mgr->ctx);
    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "request", rset_cb, rset->mgr->ctx);
    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "addResource", rset_cb, rset->mgr->ctx);
    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "setProperty", rset_cb, rset->mgr->ctx);
    mrp_dbus_remove_method(rset->mgr->ctx->dbus, rset->path,
            RSET_IFACE, "getProperties", rset_cb, rset->mgr->ctx);

    if (rset->resources)
        mrp_htbl_destroy(rset->resources, TRUE);

    destroy_property(rset->class_prop);
    destroy_property(rset->status_prop);
    destroy_property(rset->resources_prop);
    destroy_property(rset->available_resources_prop);

    mrp_free(rset->path);

    mrp_free(rset);
}


static resource_set_o_t * create_rset(manager_o_t *mgr, uint32_t id)
{
    char buf[64];
    char resbuf[128];
    int ret;
    mrp_htbl_config_t resources_conf;
    resource_set_o_t *rset = mrp_allocz(sizeof(resource_set_o_t));
    char **resources_arr;
    char **available_resources_arr;

    MRP_UNUSED(mgr);

    if (!rset)
        goto error;

    ret = snprintf(buf, 64, "%s/%u", MURPHY_PATH_BASE, id);

    if (ret == 64)
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

    rset->resources_prop = create_property(rset->mgr->ctx, rset->path,
            RSET_IFACE, "ao", "resources", resources_arr, free_string_array);

    if (!rset->resources_prop)
        goto error;

    rset->class_prop = create_property(rset->mgr->ctx, rset->path,
            RSET_IFACE, "s", "class", mrp_strdup("default"), free_value);
    rset->class_prop->writable = TRUE;

    if (!rset->class_prop)
        goto error;

    rset->status_prop = create_property(rset->mgr->ctx, rset->path,
            RSET_IFACE, "s", "status", "pending", NULL);

    if (!rset->status_prop)
        goto error;

    /* TODO: what's the point in having a const char buf?!? */
    available_resources_arr = copy_string_array(
                mrp_resource_definition_get_all_names(128,
                        (const char **) resbuf));

    /* TODO: how to listen to the changes in available resources? */
    rset->available_resources_prop = create_property(rset->mgr->ctx,
            rset->path, RSET_IFACE, "as", "availableResources",
            available_resources_arr, free_string_array);

    if (!rset->available_resources_prop)
        goto error;

    rset->set = mrp_resource_set_create(rset->mgr->client, 0, event_cb, rset);

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


static int resource_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char *member = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *path = dbus_message_get_path(msg);

    DBusMessage *reply;
    char buf[64];

    dbus_data_t *ctx = data;

    uint32_t rset_id, resource_id;

    resource_set_o_t *rset;
    resource_o_t *resource;

    mrp_log_info("Resource callback called -- member: '%s', path: '%s',"
            " interface: '%s'", member, path, iface);

    /* parse the rset id and resource id */

    if (!parse_path(path, &rset_id, &resource_id)) {
        mrp_log_error("Failed to parse path");
        goto error_reply;
    }

    if (snprintf(buf, 64, "%s/%u", MURPHY_PATH_BASE, rset_id) == 64)
        goto error_reply;

    rset = mrp_htbl_lookup(ctx->mgr->rsets, buf);

    if (!rset)
        goto error_reply;

    resource = mrp_htbl_lookup(rset->resources, (void *) path);

    if (!resource)
        goto error_reply;

    if (strcmp(member, "getProperties") == 0) {
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

        dbus_message_iter_close_container(&msg_iter, &array_iter);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "setProperty") == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter variant_iter;

        const char *name;

        mrp_log_info("setProperty of resource %s", path);

        dbus_message_iter_init(msg, &msg_iter);

        if (!dbus_message_iter_get_arg_type(&msg_iter) == DBUS_TYPE_STRING) {
            goto error_reply;
        }

        dbus_message_iter_get_basic(&msg_iter, &name);

        if (!dbus_message_iter_has_next(&msg_iter)) {
            goto error_reply;
        }

        dbus_message_iter_next(&msg_iter);

        if (!dbus_message_iter_get_arg_type(&msg_iter) == DBUS_TYPE_VARIANT) {
            goto error_reply;
        }

        dbus_message_iter_recurse(&msg_iter, &variant_iter);

        if (strcmp(name, "mandatory") == 0) {
            dbus_bool_t value;
            dbus_bool_t *tmp = mrp_alloc(sizeof(bool));

            if (!dbus_message_iter_get_arg_type(&variant_iter)
                        == DBUS_TYPE_BOOLEAN) {
                goto error_reply;
            }

            dbus_message_iter_get_basic(&variant_iter, &value);

            *tmp = value;
            update_property(resource->mandatory_prop, tmp);
        }
        else if (strcmp(name, "shared") == 0) {
            dbus_bool_t value;
            dbus_bool_t *tmp = mrp_alloc(sizeof(bool));

            if (!dbus_message_iter_get_arg_type(&variant_iter)
                        == DBUS_TYPE_BOOLEAN) {
                goto error_reply;
            }

            dbus_message_iter_get_basic(&variant_iter, &value);

            *tmp = value;
            update_property(resource->shared_prop, tmp);
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
    else if (strcmp(member, "delete") == 0) {
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
    reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED,
                "Received invalid message");

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

    MRP_UNUSED(key);

    mrp_resource_set_add_resource(rset->set, r->name_prop->value,
            shared, NULL, mandatory);

    return MRP_HTBL_ITER_MORE;
}



static int rset_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char *member = dbus_message_get_member(msg);
    const char *iface = dbus_message_get_interface(msg);
    const char *path = dbus_message_get_path(msg);

    DBusMessage *reply;

    dbus_data_t *ctx = data;

    resource_set_o_t *rset = mrp_htbl_lookup(ctx->mgr->rsets, (void *) path);

    mrp_log_info("Resource set callback called -- member: '%s', path: '%s',"
            " interface: '%s'", member, path, iface);

    if (!rset) {
        mrp_log_error("Resource set '%s' not found, ignoring", path);
        goto error;
    }

    if (strcmp(member, "getProperties") == 0) {
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
    else if (strcmp(member, "addResource") == 0) {
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
                    RESOURCE_IFACE, "getProperties", resource_cb, ctx)) {
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, resource->path,
                    RESOURCE_IFACE, "setProperty", resource_cb, ctx)) {
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, resource->path,
                    RESOURCE_IFACE, "delete", resource_cb, ctx)) {
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
    else if (strcmp(member, "request") == 0) {
        dbus_bool_t success = TRUE;

        mrp_log_info("Requesting rset %s", path);

        /* add the resources */
        mrp_htbl_foreach(rset->resources, add_resource_cb, rset);

        mrp_application_class_add_resource_set((char *) rset->class_prop->value,
                rset->mgr->zone, rset->set);

        mrp_resource_set_acquire(rset->set, 0);

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success,
                DBUS_TYPE_INVALID);

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "release") == 0) {
        mrp_log_info("Releasing rset %s", path);

        mrp_resource_set_release(rset->set, 0);

        reply = dbus_message_new_method_return(msg);
        if (!reply) {
            goto error;
        }

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "delete") == 0) {
        mrp_log_info("Deleting rset %s", path);

        mrp_htbl_remove(ctx->mgr->rsets, (void *) path, TRUE);
        update_property(ctx->mgr->rsets_prop, htbl_keys(ctx->mgr->rsets));

        reply = dbus_message_new_method_return(msg);

        if (!reply)
            goto error;

        mrp_dbus_send_msg(dbus, reply);
        dbus_message_unref(reply);
    }
    else if (strcmp(member, "setProperty") == 0) {
        DBusMessageIter msg_iter;
        DBusMessageIter variant_iter;

        const char *name;
        const char *value;

        dbus_message_iter_init(msg, &msg_iter);

        if (!dbus_message_iter_get_arg_type(&msg_iter) == DBUS_TYPE_STRING) {
            goto error_reply;
        }

        dbus_message_iter_get_basic(&msg_iter, &name);

        if (!dbus_message_iter_has_next(&msg_iter)) {
            goto error_reply;
        }

        dbus_message_iter_next(&msg_iter);

        if (!dbus_message_iter_get_arg_type(&msg_iter) == DBUS_TYPE_VARIANT) {
            goto error_reply;
        }

        dbus_message_iter_recurse(&msg_iter, &variant_iter);

        if (strcmp(name, "class") == 0) {
            if (!dbus_message_iter_get_arg_type(&variant_iter)
                        == DBUS_TYPE_STRING) {
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
    reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED,
                "Received invalid message");

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

    if (strcmp(member, "getProperties") == 0) {
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
    else if (strcmp(member, "createResourceSet") == 0) {
        resource_set_o_t *rset = create_rset(ctx->mgr, ctx->mgr->next_id++);

        if (!rset)
            goto error_reply;

        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "getProperties", rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "setProperty", rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "addResource", rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "request", rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "release", rset_cb, ctx)) {
            destroy_rset(rset);
            goto error_reply;
        }
        if (!mrp_dbus_export_method(ctx->dbus, rset->path,
                    RSET_IFACE, "delete", rset_cb, ctx)) {
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
            MANAGER_IFACE, "createResourceSet", mgr_cb, mgr->ctx);

    mrp_dbus_remove_method(mgr->ctx->dbus, MURPHY_PATH_BASE,
            MANAGER_IFACE, "getProperties", mgr_cb, mgr->ctx);

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
            "ao", "resourceSets", rset_arr, free_string_array);

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

    mgr->zone = "default";

    return mgr;

error:

    destroy_manager(mgr);

    return NULL;
}

#ifdef HAVE_TO_DEFINE_RESOURCES
#include <murphy/resource/manager-api.h>
#include <murphy/resource/config-api.h>


static void tmp_create_resources()
{
    /* FIXME: this is the wrong place to do this */

    static mrp_resource_mgr_ftbl_t audio_playback_manager;
    static mrp_resource_mgr_ftbl_t audio_record_manager;

    mrp_zone_definition_create(NULL);

    mrp_zone_create("default", NULL);

    mrp_application_class_create("default", 0);
    mrp_application_class_create("player", 1);
    mrp_application_class_create("sound", 2);
    mrp_application_class_create("camera", 3);
    mrp_application_class_create("phone", 4);

    mrp_resource_definition_create("audio_playback", true, NULL,
                &audio_playback_manager, NULL);

    mrp_resource_definition_create("audio_record", true, NULL,
                &audio_record_manager, NULL);
}
#endif

static int dbus_resource_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    dbus_data_t *ctx = mrp_allocz(sizeof(dbus_data_t));

    ctx->addr = args[ARG_DBUS_SERVICE].str;
    ctx->dbus = mrp_dbus_connect(plugin->ctx->ml, "system", NULL);

    if (ctx->dbus == NULL) {
        mrp_log_error("Failed to connect to D-Bus");
        goto error;
    }

#ifdef HAVE_TO_DEFINE_RESOURCES
    tmp_create_resources();
#endif

    ctx->mgr = create_manager(ctx);

    if (ctx->mgr == NULL) {
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
                MANAGER_IFACE, "createResourceSet", mgr_cb, ctx)) {
       mrp_log_error("Failed to register manager object");
       goto error;
    }

    if (!mrp_dbus_export_method(ctx->dbus, MURPHY_PATH_BASE,
                MANAGER_IFACE, "getProperties", mgr_cb, ctx)) {
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
    mrp_free(ctx->mgr);
    mrp_free(ctx);

    plugin->data = NULL;
}


#define DBUS_RESOURCE_DESCRIPTION "A plugin to implement D-Bus resource API."
#define DBUS_RESOURCE_HELP        "D-Bus resource manager backend"
#define DBUS_RESOURCE_VERSION     MRP_VERSION_INT(0, 0, 1)
#define DBUS_RESOURCE_AUTHORS     "Ismo Puustinen <ismo.puustinen@intel.com>"

static mrp_plugin_arg_t args[] = {
    MRP_PLUGIN_ARGIDX(ARG_DBUS_SERVICE, STRING, "dbus_service", "org.Murphy"),
};


MURPHY_REGISTER_PLUGIN("resource-dbus",
                       DBUS_RESOURCE_VERSION, DBUS_RESOURCE_DESCRIPTION,
                       DBUS_RESOURCE_AUTHORS, DBUS_RESOURCE_HELP,
                       MRP_SINGLETON, dbus_resource_init, dbus_resource_exit,
                       args, MRP_ARRAY_SIZE(args),
                       NULL, 0, NULL, 0, NULL);
