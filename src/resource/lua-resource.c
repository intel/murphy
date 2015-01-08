/*
 * Copyright (c) 2013, Intel Corporation
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
#include <stdbool.h>
#include <errno.h>

#include <murphy/common.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>

#include <murphy/resource/client-api.h>

#define MAX_ATTRS 128

/*
 * Bindings to the resource library
 */

#define ATTRIBUTE_LUA_CLASS MRP_LUA_CLASS(attribute, lua)
#define RESOURCE_LUA_CLASS MRP_LUA_CLASS(resource, lua)
#define RESOURCE_SET_LUA_CLASS MRP_LUA_CLASS(resource_set, lua)

typedef struct resource_lua_s resource_lua_t;
typedef struct resource_set_lua_s resource_set_lua_t;
typedef struct attribute_lua_s attribute_lua_t;

struct attribute_lua_s {
    lua_State *L; /* Lua execution context */

    resource_set_lua_t *resource_set;

    int initialized;
    resource_lua_t *parent;
};

struct resource_lua_s {
    lua_State *L; /* Lua execution context */

    bool available; /* status */
    bool acquired; /* status*/
    bool shared; /* bookkeeping */
    bool mandatory; /* bookkeeping */
    char *resource_name; /* bookkeeping */

    resource_set_lua_t *parent; /* resource set the resource belongs to */

    int attributes; /* for lua bindings */
    attribute_lua_t *real_attributes; /* real attributes */
};

struct resource_set_lua_s {
    lua_State *L; /* Lua execution context */

    mrp_resource_set_t *resource_set; /* associated murphy resource set */

    int id; /* resource set internal id */
    int callback; /* callback */
    bool available; /* status */
    bool acquired; /* status */
    bool autorelease; /* input */
    bool dont_wait; /* input */
    char *zone; /* input */
    char *application_class; /* input */
    int32_t priority; /* input */

    bool committed; /* resource set is locked and cannot be edited anymore */
    bool initialized; /* resource set was returned to the Lua user */

    mrp_htbl_t *resources;
};

static mrp_resource_client_t *client = NULL;
uint32_t n_sets = 0;

/* resource set */

static int resource_set_lua_create(lua_State *L);

static int resource_set_get_resources(void *data, lua_State *L, int member,
        mrp_lua_value_t *v);
static int resource_set_get_id(void *data, lua_State *L, int member,
        mrp_lua_value_t *v);
static int resource_set_add_resource(lua_State *L);
static int resource_set_acquire(lua_State *L);
static int resource_set_release(lua_State *L);

static void resource_set_lua_changed(void *data, lua_State *L, int member);
static void resource_set_lua_destroy(void *data);
static int resource_set_lua_stringify(lua_State *L);

#define RS_OFFS(m)   MRP_OFFSET(resource_set_lua_t, m)
#define RS_RDONLY    MRP_LUA_CLASS_READONLY
#define RS_NOTIFY    MRP_LUA_CLASS_NOTIFY
#define RS_NOFLAGS   MRP_LUA_CLASS_NOFLAGS
#define RS_RAWGETTER MRP_LUA_CLASS_RAWGETTER

MRP_LUA_METHOD_LIST_TABLE(resource_set_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(resource_set_lua_create)
                          MRP_LUA_METHOD(addResource, resource_set_add_resource)
                          MRP_LUA_METHOD(acquire, resource_set_acquire)
                          MRP_LUA_METHOD(release, resource_set_release));

MRP_LUA_METHOD_LIST_TABLE(resource_set_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (resource_set_lua_create)
                          MRP_LUA_OVERRIDE_STRINGIFY(resource_set_lua_stringify));

MRP_LUA_MEMBER_LIST_TABLE(resource_set_lua_members,
    MRP_LUA_CLASS_INTEGER("id", RS_OFFS(id), NULL, resource_set_get_id, RS_RDONLY)
    MRP_LUA_CLASS_STRING("application_class", RS_OFFS(application_class), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_STRING("zone", RS_OFFS(zone), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_ANY("resources", RS_OFFS(resources), NULL, resource_set_get_resources, RS_RDONLY | RS_RAWGETTER)
    MRP_LUA_CLASS_BOOLEAN("dont_wait", RS_OFFS(dont_wait), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("autorelease", RS_OFFS(autorelease), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("available", RS_OFFS(available), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("acquired", RS_OFFS(acquired), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_INTEGER("priority", RS_OFFS(priority), NULL, NULL, RS_RDONLY)
    MRP_LUA_CLASS_LFUNC("callback", RS_OFFS(callback), NULL, NULL, RS_NOTIFY));

/* be careful! needs to be in the same order as the member list table */
typedef enum {
    RESOURCE_SET_MEMBER_ID,
    RESOURCE_SET_MEMBER_APPLICATION_CLASS,
    RESOURCE_SET_MEMBER_ZONE,
    RESOURCE_SET_MEMBER_RESOURCES,
    RESOURCE_SET_MEMBER_DONT_WAIT,
    RESOURCE_SET_MEMBER_AUTORELEASE,
    RESOURCE_SET_MEMBER_AVAILABLE,
    RESOURCE_SET_MEMBER_ACQUIRED,
    RESOURCE_SET_MEMBER_PRIORITY,
    RESOURCE_SET_MEMBER_CALLBACK,
} resource_set_member_t;

MRP_LUA_DEFINE_CLASS(resource_set, lua, resource_set_lua_t, resource_set_lua_destroy,
                          resource_set_lua_methods, resource_set_lua_overrides,
                          resource_set_lua_members, NULL, resource_set_lua_changed, NULL, NULL,
                          MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);

/* resource */

static int resource_lua_create(lua_State *L);
static void resource_lua_destroy(void *data);
static int resource_lua_stringify(lua_State *L);
static void resource_lua_changed(void *data, lua_State *L, int member);

/* get the whole attribute table */
static int resource_get_attributes(void *data, lua_State *L, int member, mrp_lua_value_t *v);
static int resource_set_attributes(void *data, lua_State *L, int member, mrp_lua_value_t *v);

#define R_OFFS(m)   MRP_OFFSET(resource_lua_t, m)
#define R_RDONLY    MRP_LUA_CLASS_READONLY
#define R_NOTIFY    MRP_LUA_CLASS_NOTIFY
#define R_NOFLAGS   MRP_LUA_CLASS_NOFLAGS
#define R_RAWGETTER MRP_LUA_CLASS_RAWGETTER
#define R_RAWSETTER MRP_LUA_CLASS_RAWSETTER

MRP_LUA_METHOD_LIST_TABLE(resource_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(resource_lua_create));

MRP_LUA_METHOD_LIST_TABLE(resource_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (resource_lua_create)
                          MRP_LUA_OVERRIDE_STRINGIFY(resource_lua_stringify));

MRP_LUA_MEMBER_LIST_TABLE(resource_lua_members,
    MRP_LUA_CLASS_ANY("attributes", R_OFFS(attributes), resource_set_attributes, resource_get_attributes, R_RAWGETTER | R_RAWSETTER)
    MRP_LUA_CLASS_STRING("resource_name", R_OFFS(resource_name), NULL, NULL, R_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("available", R_OFFS(available), NULL, NULL, R_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("acquired", R_OFFS(acquired), NULL, NULL, R_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("shared", R_OFFS(shared), NULL, NULL, R_RDONLY)
    MRP_LUA_CLASS_BOOLEAN("mandatory", R_OFFS(mandatory), NULL, NULL, R_RDONLY));

/* be careful! needs to be in the same order as the member list table */
typedef enum {
    RESOURCE_MEMBER_ATTRIBUTES,
    RESOURCE_MEMBER_RESOURCE_NAME,
    RESOURCE_MEMBER_AVAILABLE,
    RESOURCE_MEMBER_ACQUIRED,
    RESOURCE_MEMBER_SHARED,
    RESOURCE_MEMBER_MANDATORY,
} resource_member_t;

MRP_LUA_DEFINE_CLASS(resource, lua, resource_lua_t, resource_lua_destroy,
                          resource_lua_methods, resource_lua_overrides,
                          resource_lua_members, NULL, resource_lua_changed, NULL, NULL,
                          MRP_LUA_CLASS_NOFLAGS);


/* attribute table */

/* The problem is that attribute table is a member of resource, meaning that
   we will get events when the full attribute table is changed but won't get
   any when a unique element of the attribute table is accessed. The solution
   is to have the attributes handled as an object, whose constructor initializes
   the attributes from the resource library. Any access to an attribute will
   be seen as an access to a member and intercepted with "changed". */

static int attribute_lua_create(lua_State *L);
static void attribute_lua_destroy(void *data);
static int attribute_lua_stringify(lua_State *L);
static void attribute_lua_changed(void *data, lua_State *L, int member);
static int attribute_lua_getfield(lua_State *L);
static int attribute_lua_setfield(lua_State *L);

#define A_OFFS(m)   MRP_OFFSET(attribute_lua_t, m)
#define A_RDONLY    MRP_LUA_CLASS_READONLY
#define A_NOTIFY    MRP_LUA_CLASS_NOTIFY
#define A_NOFLAGS   MRP_LUA_CLASS_NOFLAGS
#define A_RAWGETTER MRP_LUA_CLASS_RAWGETTER
#define A_RAWSETTER MRP_LUA_CLASS_RAWSETTER

MRP_LUA_METHOD_LIST_TABLE(attribute_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(attribute_lua_create));

MRP_LUA_METHOD_LIST_TABLE(attribute_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (attribute_lua_create)
                          MRP_LUA_OVERRIDE_STRINGIFY(attribute_lua_stringify)
                          MRP_LUA_OVERRIDE_GETFIELD (attribute_lua_getfield)
                          MRP_LUA_OVERRIDE_SETFIELD (attribute_lua_setfield));

/* "initialized" is a placeholder */

MRP_LUA_MEMBER_LIST_TABLE(attribute_lua_members,
    MRP_LUA_CLASS_BOOLEAN("initialized", A_OFFS(initialized), NULL, NULL, A_RDONLY));

/* be careful! needs to be in the same order as the member list table */
typedef enum {
    ATTRIBUTE_MEMBER_INITIALIZED,
} attribute_member_t;

MRP_LUA_DEFINE_CLASS(attribute, lua, attribute_lua_t, attribute_lua_destroy,
                          attribute_lua_methods, attribute_lua_overrides,
                          attribute_lua_members, NULL, attribute_lua_changed, NULL, NULL,
                          MRP_LUA_CLASS_NOFLAGS);

/* resource set */

static inline resource_set_lua_t *resource_set_lua_check(lua_State *L, int idx)
{
    return (resource_set_lua_t *) mrp_lua_check_object(L, RESOURCE_SET_LUA_CLASS, idx);
}

static int resource_set_add_resource(lua_State *L)
{
    int narg;
    resource_set_lua_t *rset;
    resource_lua_t *resource;
    const char *resource_name;
    bool shared = FALSE;
    bool mandatory = TRUE;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs;

    mrp_debug("> add_resource");

    narg = lua_gettop(L);

    if (narg != 2)
        return luaL_error(L, "expecting one argument");

    rset = resource_set_lua_check(L, 1);

    if (!rset)
        goto error;

    /* the argument should be a table with at least "resource_name" index */

    if (!lua_istable(L, -1))
        return luaL_error(L, "argument error -- not a table");

    lua_pushstring(L, "resource_name");
    lua_gettable(L, -2);

    if (!lua_isstring(L, -1))
        return luaL_error(L, "'resource_name' is a mandatory field");

    resource_name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_pushstring(L, "mandatory");
    lua_gettable(L, -2);

    if (lua_isboolean(L, -1)) {
        mandatory = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_pushstring(L, "shared");
    lua_gettable(L, -2);

    if (lua_isboolean(L, -1)) {
        shared = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    /* create resource object and add it to the resource table in the resource
     * set object */

    resource = (resource_lua_t *) mrp_lua_create_object(L, RESOURCE_LUA_CLASS,
            NULL, 0);

    if (!resource)
        goto error;

    /* mrp_lua_object_ref_value(resource, L, 0); */

    resource->mandatory = mandatory;
    resource->shared = shared;
    resource->acquired = FALSE;
    resource->available = FALSE;
    resource->resource_name = mrp_strdup(resource_name);

    if (!resource->resource_name)
        goto error;

    resource->parent = rset;
    resource->L = L;

    resource->real_attributes = (attribute_lua_t *) mrp_lua_create_object(L,
            ATTRIBUTE_LUA_CLASS, NULL, 0);

    if (!resource->real_attributes)
        goto error;

    /* mrp_lua_object_ref_value(resource->real_attributes, L, 0); */

    resource->real_attributes->L = L;
    resource->real_attributes->parent = resource;
    resource->real_attributes->resource_set = rset;
    resource->real_attributes->initialized = TRUE;

    attrs = mrp_resource_set_read_all_attributes(rset->resource_set,
            resource->resource_name, MAX_ATTRS-1, attribute_list);

    if (mrp_resource_set_add_resource(rset->resource_set,
            resource->resource_name, shared, attrs, mandatory) < 0)
        goto error;

    /* add to resource map */

    mrp_debug("inserted resource %s to %p", resource->resource_name, rset);
    mrp_htbl_insert(rset->resources, resource->resource_name, resource);

    return 0;

error:
    /* TODO: clean up the already allocated objects */

    return luaL_error(L, "internal resource library error");
}

static int resource_set_acquire(lua_State *L)
{
    resource_set_lua_t *rset;

    mrp_debug("acquire");

    rset = resource_set_lua_check(L, 1);

    if (!rset)
        return luaL_error(L, "internal error");

    if (!rset->committed) {

        /* Application backend requires us to "commit" the resource set before
         * we can use the resource set. It can be done only after all resources
         * have been added to the resource set and all the attributes
         * configured. */

        if (mrp_application_class_add_resource_set(rset->application_class,
                rset->zone, rset->resource_set, 0) < 0)
                return luaL_error(L, "failed to commit the resource set");

        rset->committed = TRUE;
    }

    mrp_resource_set_acquire(rset->resource_set, 0);

    return 0;
}

static int resource_set_release(lua_State *L)
{
    resource_set_lua_t *rset;

    mrp_debug("> release");

    rset = resource_set_lua_check(L, 1);

    if (!rset)
        return luaL_error(L, "internal error");

    if (!rset->committed) {

        /* Committing the resource set here means that the resource set stays
         * in released state but already receives events. */

        if (mrp_application_class_add_resource_set(rset->application_class,
            rset->zone, rset->resource_set, 0) < 0)
            return luaL_error(L, "failed to commit the resource set");

        rset->committed = TRUE;
    }

    mrp_resource_set_release(rset->resource_set, 0);

    return 0;
}


void event_cb(uint32_t request_id, mrp_resource_set_t *resource_set, void *user_data)
{
    resource_set_lua_t *rset = (resource_set_lua_t *) user_data;
    mrp_resource_mask_t grant, advice;
    int                 top;

    MRP_UNUSED(request_id);
    MRP_UNUSED(resource_set);

    mrp_debug("> event_cb");

    top = lua_gettop(rset->L);

    grant = mrp_get_resource_set_grant(rset->resource_set);
    advice = mrp_get_resource_set_advice(rset->resource_set);

    /* update resource set */
    rset->acquired = !!grant;
    rset->available = !!advice;

    if (mrp_lua_object_deref_value(rset, rset->L, rset->callback, false)) {
        mrp_lua_push_object(rset->L, rset);

        if (lua_pcall(rset->L, 1, 0, 0) != 0)
            mrp_log_error("failed to invoke Lua resource set callback: %s",
                    lua_tostring(rset->L, -1));
    }

    lua_settop(rset->L, top);
}

static void htbl_free_resource(void *key, void *object)
{
    resource_lua_t *res = (resource_lua_t *) object;
    MRP_UNUSED(key);

    mrp_lua_destroy_object(res->L, NULL, 0, res);
#if 0

    mrp_debug("lua-resource: htbl_free_resource %p, res: '%s'", res,
            res->resource_name);

    MRP_UNUSED(key);
#endif
}

static int resource_set_lua_create(lua_State *L)
{
    char e[128] = "";
    resource_set_lua_t *rset;
    int narg;
    mrp_htbl_config_t conf;

    mrp_debug("create");

    narg = lua_gettop(L);

    rset = (resource_set_lua_t *) mrp_lua_create_object(L,
            RESOURCE_SET_LUA_CLASS, NULL, 0);

    if (!rset)
        return luaL_error(L, "could not create Lua object");

    rset->L = L;

    /* user can affect these values */
    rset->zone = mrp_strdup("default");
    rset->application_class = NULL;
    rset->autorelease = FALSE;
    rset->dont_wait = FALSE;
    rset->priority = 0;
    rset->committed = FALSE;
    rset->initialized = FALSE;

    switch (narg) {
    case 2:
        /* argument table */
        if (mrp_lua_init_members(rset, L, -2, e, sizeof(e)) != 1)
            return luaL_error(L, "failed to initialize resource members (%s)",
                    e);
        break;
    default:
        return luaL_error(L, "expecting a constructor argument, "
                          "got %d", narg);
    }

    if (rset->application_class == NULL)
        return luaL_error(L, "application_class is a mandatory parameter");

    if (rset->priority < 0)
        rset->priority = 0;

    /* initial state, these cannot be set by user */
    rset->available = FALSE;
    rset->acquired = FALSE;

    /* initialize resource map */
    conf.nbucket = 0;
    conf.nentry = 10;
    conf.comp = mrp_string_comp;
    conf.hash = mrp_string_hash;
    conf.free = htbl_free_resource;

    rset->resources = mrp_htbl_create(&conf);
    if (!rset->resources)
        goto error;

    /* do the actual resource work */

    if (!client) {
        /* create the resource client */

        client = mrp_resource_client_create("lua", NULL);

        if (!client)
            goto error;
    }

    rset->resource_set = mrp_resource_set_create(client, rset->autorelease,
            rset->dont_wait, rset->priority, event_cb, rset);

    if (rset->resource_set)
        n_sets++;
    else
        goto error;

    rset->initialized = TRUE;

    mrp_lua_push_object(L, rset);

    return 1;

error:
    return luaL_error(L, "internal resource library error");
}

static int resource_set_get_id(void *data, lua_State *L, int member,
        mrp_lua_value_t *v)
{
    resource_set_lua_t *rset;
    MRP_UNUSED(L);
    MRP_UNUSED(member);

    mrp_debug("> resource_set_get_id");

    if (!v)
        return 0;

    rset = (resource_set_lua_t *) data;

    v->s32 = mrp_get_resource_set_id(rset->resource_set);

    return 1;
}

static int resource_set_get_resources(void *data, lua_State *L, int member, mrp_lua_value_t *v)
{
    resource_set_lua_t *rset;
    void *iter = NULL;
    mrp_resource_t *resource;
    mrp_resource_mask_t grant, advice;

    MRP_UNUSED(member);
    MRP_UNUSED(v);

    mrp_debug("> resource_set_get_resources");

    rset = (resource_set_lua_t *) data;

    if (!rset)
        return luaL_error(L, "internal error");

    grant = mrp_get_resource_set_grant(rset->resource_set);
    advice = mrp_get_resource_set_advice(rset->resource_set);

    lua_newtable(L);

    /* push all resource objects to a table and return it */

    while ((resource = mrp_resource_set_iterate_resources(rset->resource_set, &iter))) {
        const char *name = mrp_resource_get_name(resource);
        mrp_resource_mask_t mask = mrp_resource_get_mask(resource);

        /* fetch and update the resource object */

        resource_lua_t *res =
                (resource_lua_t *) mrp_htbl_lookup(rset->resources,
                (void *) name);

        if (!res) {
            mrp_log_error("resources out of sync: %s not found", name);
            continue;
        }

        /* mrp_lua_object_ref_value(res, L, 0); */

        res->acquired = !!(mask & grant);
        res->available = !!(mask & advice);

        /* TODO: update attributes */

        /* push the resource to the table */
        lua_pushstring(L, res->resource_name);
        mrp_lua_push_object(L, res);
        lua_settable(L, -3);
    }

    return 1;
}

static int resource_set_lua_stringify(lua_State *L)
{
    resource_set_lua_t *rset;

    mrp_debug("> stringify");

    rset = resource_set_lua_check(L, 1);

    if (!rset) {
        lua_pushstring(L, "invalid resource set");
        return 1;
    }

    lua_pushfstring(L, "resource set '%s', acquired: %s, available: %s",
            rset->application_class,
            rset->acquired ? "yes" : "no",
            rset->available ? "yes" : "no");

    return 1;
}

static void resource_set_lua_destroy(void *data)
{
    resource_set_lua_t *rset = (resource_set_lua_t *) data;

    mrp_debug("lua destructor for rset %p", rset);

    /* remove resources from the resource set -- they are finally cleaned from
     * their own lua destructors */

    if (rset->resource_set)
        mrp_resource_set_destroy(rset->resource_set);

    if (rset->resources) {
        mrp_debug("deleting htbl at %p", rset->resources);
        mrp_htbl_destroy(rset->resources, TRUE);
        rset->resources = NULL;
    }

    mrp_free(rset->zone);
    mrp_free(rset->application_class);

    if (rset->initialized) {
       n_sets--;

        if (n_sets == 0) {
            mrp_resource_client_destroy(client);
            client = NULL;
        }
    }

    return;
}

static void resource_set_lua_changed(void *data, lua_State *L, int member)
{
    resource_set_lua_t *rset = (resource_set_lua_t *) data;

    MRP_UNUSED(L);
    MRP_UNUSED(member);

    mrp_debug("> changed");

    switch (member) {
        case RESOURCE_SET_MEMBER_CALLBACK:
            /* no special handling needed */
            break;
        default:
            /* trying to change readonly properties, should trigger an error */
            mrp_log_error("Trying to change a readonly property for resource set %s",
                    rset->application_class);
            break;
    }

    return;
}

/* resource */

static inline resource_lua_t *resource_lua_check(lua_State *L, int idx)
{
    return (resource_lua_t *) mrp_lua_check_object(L, RESOURCE_LUA_CLASS, idx);
}

static int resource_lua_create(lua_State *L)
{
    mrp_debug("> resource_lua_create");

    return luaL_error(L, "Resource objects are created with ResourceSet:addResource()");
}

static int resource_lua_stringify(lua_State *L)
{
    resource_lua_t *res;

    mrp_debug("> stringify");

    res = resource_lua_check(L, 1);

    if (!res) {
        lua_pushstring(L, "invalid resource set");
        return 1;
    }

    lua_pushfstring(L, "resource '%s', acquired: %s, available: %s, mandatory: %s, shared: %s",
            res->resource_name,
            res->acquired ? "yes" : "no",
            res->available ? "yes" : "no",
            res->mandatory ? "yes" : "no",
            res->shared ? "yes" : "no");

    return 1;
}

static void resource_lua_destroy(void *data)
{
    resource_lua_t *res = (resource_lua_t *) data;

    mrp_debug("lua destructor for resource %p (%s)", res, res->resource_name);

    mrp_free(res->resource_name);

    /* TODO: unref res->real_attributes ? */

    mrp_lua_destroy_object(res->L, NULL, 0, res->real_attributes);

    return;
}

static void resource_lua_changed(void *data, lua_State *L, int member)
{
    MRP_UNUSED(data);
    MRP_UNUSED(L);
    MRP_UNUSED(member);

    mrp_debug("> resource_changed");
}

#if 0
static int resource_get_attributes(void *data, lua_State *L, int member, mrp_lua_value_t *v)
{
    resource_lua_t *res = (resource_lua_t *) data;
    resource_set_lua_t *rset = res->parent;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs;

    mrp_debug("> resource_get_attributes");

    lua_newtable(L);

    attrs = mrp_resource_set_read_all_attributes(rset->resource_set,
                                     res->resource_name,
                                     MAX_ATTRS-1,
                                     attribute_list);

    while (attrs->name != NULL) {

        switch (attrs->type) {
            case mqi_string:
                lua_pushstring(L, attrs->name);
                lua_pushstring(L, attrs->value.string);
                lua_settable(L, -3);
                break;
            case mqi_integer:
                lua_pushstring(L, attrs->name);
                lua_pushinteger(L, attrs->value.integer);
                lua_settable(L, -3);
                break;
            case mqi_unsignd:
                if (attrs->value.unsignd > INT_MAX) {
                    /* too big! */
                    mrp_log_error("Sorry, we don't support big unsigned values right now");
                }
                else {
                    lua_pushstring(L, attrs->name);
                    lua_pushinteger(L, attrs->value.unsignd);
                    lua_settable(L, -3);
                }
                break;
            case mqi_floating:
                lua_pushstring(L, attrs->name);
                lua_pushnumber(L, attrs->value.floating);
                lua_settable(L, -3);
                break;
            default:
                mrp_log_error("Unhandled attribute type");
                break;
        }

        attrs++;
    }

    return 1;
}
#else
static int resource_get_attributes(void *data, lua_State *L, int member, mrp_lua_value_t *v)
{
    resource_lua_t *res = (resource_lua_t *) data;

    MRP_UNUSED(member);
    MRP_UNUSED(v);

    mrp_debug("> resource_get_attributes");

    mrp_lua_push_object(L, res->real_attributes);

    return 1;
}
#endif

static int resource_set_attributes(void *data, lua_State *L, int member, mrp_lua_value_t *v)
{
    resource_lua_t *res = (resource_lua_t *) data;
    resource_set_lua_t *rset = res->parent;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs, *orig;

    MRP_UNUSED(member);
    MRP_UNUSED(v);

    mrp_debug("> resource_set_attributes");

    if (!lua_istable(L, -1))
        return luaL_error(L, "argument error -- not a table");

    mrp_resource_set_read_all_attributes(rset->resource_set,
            res->resource_name, MAX_ATTRS-1, attribute_list);

    attrs = orig = attribute_list;

    while (attrs->name != NULL) {
        /* get the attribute from the lua table by name */

        lua_pushstring(L, attrs->name);
        lua_gettable(L, -2);

        /* update the attribute */

        switch (attrs->type) {
            case mqi_string:
                if (lua_isstring(L, -1)) {
                    attrs->value.string = lua_tostring(L, -1);
                    mrp_debug("updated attr '%s' to '%s'", attrs->name, attrs->value.string);
                }
                break;
            case mqi_integer:
                if (lua_isnumber(L, -1)) {
                    attrs->value.integer = lua_tointeger(L, -1);
                    mrp_debug("updated attr '%s' to '%i'", attrs->name, attrs->value.integer);
                }
                break;
            case mqi_unsignd:
                if (lua_isnumber(L, -1)) {
                    int val = lua_tointeger(L, -1);
                    if (val >= 0) {
                        attrs->value.unsignd = val;
                        mrp_debug("updated attr '%s' to '%u'", attrs->name, attrs->value.unsignd);
                    }
                }
                break;
            case mqi_floating:
                if (lua_isnumber(L, -1)) {
                    attrs->value.floating = lua_tonumber(L, -1);
                    mrp_debug("updated attr '%s' to '%f'", attrs->name, attrs->value.floating);
                }
                break;
            default:
                break;
        }

        lua_pop(L, 1);
        attrs++;
    }

    /* write the attributes back */
    mrp_resource_set_write_attributes(rset->resource_set, res->resource_name, orig);

    return 1;
}

static inline attribute_lua_t *attribute_lua_check(lua_State *L, int idx)
{
    return (attribute_lua_t *) mrp_lua_check_object(L, ATTRIBUTE_LUA_CLASS, idx);
}

static int attribute_lua_create(lua_State *L)
{
    mrp_debug("> attribute_create");

    return luaL_error(L, "Attribute objects are created with ResourceSet:addResource()");
}

static void attribute_lua_destroy(void *data)
{
    attribute_lua_t *attribute = (attribute_lua_t *) data;

    mrp_debug("lua destructor for attribute table %p", attribute);

    return;
}

static int attribute_lua_stringify(lua_State *L)
{
    attribute_lua_t *attribute = attribute_lua_check(L, 1);
    resource_lua_t *res = attribute->parent;
    resource_set_lua_t *rset = res->parent;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs;

    int available = 4095;
    char buf[available+1], *p;
    char numbuf[16];
    int vallen;

    mrp_debug("> attribute_stringify");

    memset(buf, 0, sizeof(buf));
    p = buf;

    mrp_resource_set_read_all_attributes(rset->resource_set,
            res->resource_name, MAX_ATTRS-1, attribute_list);

    attrs = attribute_list;

    while (attrs->name != NULL) {

        int keylen = strlen(attrs->name);

        /* we need space for 2 + null */
        available -= keylen + 3;

        if (available < 0)
            goto outofspace;

        strncpy(p, attrs->name, keylen);
        p += keylen;

        /*
         * we copy ": \0" and then proceed to only
         * move the pointer by two, thus we can
         * add one to the amount of available
         * space.
         */
        strncpy(p, ": ", 3);
        p += 2;
        available += 1;

        switch (attrs->type) {
            case mqi_string:
                vallen = strlen(attrs->value.string);
                available -= vallen + 1;
                if (available < 0)
                    goto outofspace;
                strncpy(p, attrs->value.string, vallen);
                p += vallen;
                *p = '\n';
                p += 1;
                break;
            case mqi_integer:
                snprintf(numbuf, sizeof(numbuf), "%d",
                        (int) attrs->value.integer);
                vallen = strlen(numbuf);
                available -= vallen + 1;
                if (available < 0)
                    goto outofspace;
                strncpy(p, numbuf, vallen);
                p += vallen;
                *p = '\n';
                p += 1;
                break;
            case mqi_unsignd:
                snprintf(numbuf, sizeof(numbuf), "%u", attrs->value.unsignd);
                vallen = strlen(numbuf);
                available -= vallen + 1;
                if (available < 0)
                    goto outofspace;
                strncpy(p, numbuf, vallen);
                p += vallen;
                *p = '\n';
                p += 1;
                break;
            case mqi_floating:
                snprintf(numbuf, sizeof(numbuf), "%f", attrs->value.floating);
                vallen = strlen(numbuf);
                available -= vallen + 1;
                if (available < 0)
                    goto outofspace;
                strncpy(p, numbuf, vallen);
                p += vallen;
                *p = '\n';
                p += 1;
                break;
            default:
                break;
        }

        attrs++;
    }
    lua_pushstring(L, buf);

    return 1;

outofspace:
    return luaL_error(L, "out of string buffer space");
}

static void attribute_lua_changed(void *data, lua_State *L, int member)
{
    MRP_UNUSED(data);
    MRP_UNUSED(L);
    MRP_UNUSED(member);

    mrp_debug("> attribute_changed");
    return;
}

static int attribute_lua_getfield(lua_State *L)
{
    attribute_lua_t *attribute = attribute_lua_check(L, 1);
    resource_lua_t *res = attribute->parent;
    resource_set_lua_t *rset = res->parent;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs;
    const char *key;

    mrp_debug("> attribute_lua_getfield");

    /* attributes are indexed by string */

    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "invalid attribute index type (needs to be string)");

    key = lua_tostring(L, 2);

    attrs = mrp_resource_set_read_all_attributes(rset->resource_set,
            res->resource_name, MAX_ATTRS-1, attribute_list);

    if (!attrs)
        return luaL_error(L, "internal resource library error");

    while (attrs->name != NULL) {

        if (strcmp(attrs->name, key) == 0) {

            switch (attrs->type) {
                case mqi_string:
                    lua_pushstring(L, attrs->value.string);
                    return 1;
                case mqi_integer:
                    lua_pushinteger(L, attrs->value.integer);
                    return 1;
                case mqi_unsignd:
                    if (attrs->value.unsignd > INT_MAX) {
                        /* too big! */
                        mrp_log_error("Sorry, we don't support big unsigned values right now");
                        return luaL_error(L, "too big value in attribute");
                    }
                    else {
                        lua_pushinteger(L, attrs->value.unsignd);
                    }
                    return 1;
                case mqi_floating:
                    lua_pushnumber(L, attrs->value.floating);
                    return 1;
                default:
                    mrp_log_error("Unhandled attribute type");
                    return 1;
            }
        }

        attrs++;
    }

    return luaL_error(L, "trying to get a non-existing attribute");
}

static int attribute_lua_setfield(lua_State *L)
{
    attribute_lua_t *attribute = attribute_lua_check(L, 1);
    resource_lua_t *res = attribute->parent;
    resource_set_lua_t *rset = res->parent;
    mrp_attr_t attribute_list[MAX_ATTRS], *attrs, *orig;
    const char *key;
    int new_type;

    mrp_debug("> attribute_lua_setfield");

    /* attributes are indexed by string */

    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "invalid attribute index type (needs to be string)");

    key = lua_tostring(L, 2);
    new_type = lua_type(L, 3);

    attrs = mrp_resource_set_read_all_attributes(rset->resource_set,
            res->resource_name, MAX_ATTRS-1, attribute_list);

    if (!attrs)
        return luaL_error(L, "internal resource library error");

    orig = attrs;

    while (attrs->name != NULL) {

        if (strcmp(attrs->name, key) == 0) {

            switch (attrs->type) {
                case mqi_string:
                    if (new_type != LUA_TSTRING)
                        return luaL_error(L, "type mismatch");
                    attrs->value.string = lua_tostring(L, 3);
                    break;
                case mqi_integer:
                {
                    int32_t i;
                    double dbl;

                    if (new_type != LUA_TNUMBER)
                        return luaL_error(L, "type mismatch");

                    if ((i = lua_tointeger(L, 3)) == (dbl = lua_tonumber(L, 3)))
                        attrs->value.integer = i;
                    else
                        return luaL_error(L, "type mismatch");

                    break;
                }
                case mqi_unsignd:
                {
                    int32_t i;
                    double dbl;

                    if (new_type != LUA_TNUMBER)
                        return luaL_error(L, "type mismatch");

                    if ((i = lua_tointeger(L, 3)) == (dbl = lua_tonumber(L, 3)) && i >= 0)
                        attrs->value.unsignd = i;
                    else
                        return luaL_error(L, "type mismatch");

                    break;
                }
                case mqi_floating:
                {
                    if (new_type != LUA_TNUMBER)
                        return luaL_error(L, "type mismatch");
                    attrs->value.floating = lua_tonumber(L, 3);
                    break;
                }
                default:
                    return luaL_error(L, "unhandled attribute type");
            }
            break; /* while */
        }

        attrs++;
    }

    mrp_resource_set_write_attributes(rset->resource_set, res->resource_name, orig);

    return 1;
}

#if 0
MURPHY_REGISTER_LUA_BINDINGS(murphy, RESOURCE_LUA_CLASS,
                             { "Resource", resource_lua_create });

MURPHY_REGISTER_LUA_BINDINGS(murphy, RESOURCE_SET_LUA_CLASS,
                             { "ResourceSet", resource_set_lua_create });

MURPHY_REGISTER_LUA_BINDINGS(murphy, ATTRIBUTE_LUA_CLASS,
                             { "Attribute", attribute_lua_create });
#else

static void register_murphy_bindings(void) MRP_INIT;

static void register_murphy_bindings(void) {
    static struct luaL_Reg resource_methods[] = {
        { "Resource", resource_lua_create },
        { NULL, NULL }
    };

    static mrp_lua_bindings_t resource_bindings = {
        .meta = "murphy",
        .methods = resource_methods,
        .classdef = &resource_lua_class_def,
    };

    static struct luaL_Reg resource_set_methods[] = {
        { "ResourceSet", resource_set_lua_create },
        { NULL, NULL }
    };

    static mrp_lua_bindings_t resource_set_bindings = {
        .meta = "murphy",
        .methods = resource_set_methods,
        .classdef = &resource_set_lua_class_def,
    };

    static struct luaL_Reg attribute_methods[] = {
        { "Attribute", attribute_lua_create },
        { NULL, NULL }
    };

    static mrp_lua_bindings_t attribute_bindings = {
        .meta = "murphy",
        .methods = attribute_methods,
        .classdef = &attribute_lua_class_def,
    };

    mrp_list_init(&attribute_bindings.hook);
    mrp_lua_register_murphy_bindings(&attribute_bindings);

    mrp_list_init(&resource_bindings.hook);
    mrp_lua_register_murphy_bindings(&resource_bindings);

    mrp_list_init(&resource_set_bindings.hook);
    mrp_lua_register_murphy_bindings(&resource_set_bindings);
}
#endif


/*

resourcehandler = function (rset)
    if rset.resources.screen.acquired == true then
        print("got it")
    else
        print("didn't get it")
    end
end

resourcehandler = function (rset) print("bar") end

rset = m:ResourceSet ( { zone = "driver", callback = resourceHandler, application_class = "player" } )

rset:addResource({ resource_name = "audio_playback", mandatory = true })

rset.resources.audio_playback.attributes.pid = "500"

rset:acquire()

rset:release()

*/
