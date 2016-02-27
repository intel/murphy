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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common.h>
#include <murphy/common/debug.h>
#include <murphy/core/lua-bindings/murphy.h>
#include <murphy/core/lua-utils/object.h>

#include "config-lua.h"
#include "resource-lua.h"
#include "config-api.h"
#include "client-api.h"
#include "manager-api.h"
#include "zone.h"
#include "application-class.h"
#include "resource.h"
#include "resource-set.h"
#include "resource-owner.h"
#include "attribute.h"

#define ZONE_CLASS             MRP_LUA_CLASS_SIMPLE(zone)
#define APPCLASS_CLASS         MRP_LUA_CLASS_SIMPLE(application_class)
#define ZONE_ATTR_CLASS        MRP_LUA_CLASS(zone, attributes)
#define RESCLASS_CLASS         MRP_LUA_CLASS(resource, class)
#define RESMETHOD_CLASS        MRP_LUA_CLASS_SIMPLE(resource)

#define ATTRIBUTE_CLASSID      MRP_LUA_CLASSID_ROOT "attribute"
#define RESOURCE_CLASSID       MRP_LUA_CLASSID_ROOT "resource.instance"

typedef enum   field_e         field_t;
typedef enum   attr_owner_e    attr_owner_t;
typedef struct appclass_s      appclass_t;
typedef struct zone_s          zone_t;
typedef struct resclass_s      resclass_t;
typedef struct resource_s      resource_t;
typedef struct attr_def_s      attr_def_t;
typedef struct attr_s          attr_t;

typedef bool (*attribute_access_t)(attr_t *, int, mrp_attr_t *);

enum field_e {
    ATTRIBUTES = 1,
    CLASS,
    NAME,
    PRIORITY,
    SHAREABLE,
    MANDATORY,
    SYNC_RELEASE,
    MODAL,
    SHARE,
    GRANT,
    ORDER,
    SHARED,
    METHOD,
    OWNERS,
    RECALC,
    VETO,
    ID
};

enum attr_owner_e {
    ZONE = 1,
    RESOURCE
};

struct appclass_s {
    const char *name;
};

struct zone_s {
    uint32_t  id;
    const char *name;
    int attr_tbl;
};

struct resclass_s {
    uint32_t id;
    const char *name;
    mrp_attr_def_t *attrs;
};

struct resource_s {
    uint32_t rsetid;
    uint32_t resid;
    const char *name;
    int attr_tbl;
};

struct attr_def_s {
    int nattr;
    mrp_attr_def_t *attrs;
};

struct attr_s {
    struct {
        attr_owner_t type;
        void *data;
    } owner;
    attr_def_t *def;
    attribute_access_t fetch;
    attribute_access_t update;
};


static void attributes_class_create(lua_State *);
static void appclass_class_create(lua_State *);
static void zone_class_create(lua_State *);
static void resclass_class_create(lua_State *);
static void resource_class_create(lua_State *);
static void resource_methods_create(lua_State *);


static int  attributes_create(lua_State *, attr_owner_t, void *,
                              attr_def_t *, attribute_access_t,
                              attribute_access_t);
static int  attributes_getvalue(lua_State *);
static int  attributes_setvalue(lua_State *);
static int  attributes_getlength(lua_State *);
static attr_t *check_attributes(lua_State *, int);
static int  push_attributes(lua_State *, int);

static int  appclass_create(lua_State *);
static int  appclass_getfield(lua_State *);
static int  appclass_setfield(lua_State *);
static void appclass_destroy(void *);
/* static appclass_t *check_appclass(lua_State *, int); */
static appclass_t *to_appclass(lua_State *, int);

static int  zone_create(lua_State *);
static int  zone_getfield(lua_State *);
static int  zone_setfield(lua_State *);
static void zone_destroy(void *);
/* static zone_t *check_zone(lua_State *, int); */
static zone_t *to_zone(lua_State *, int);

static int  zone_attr_create(lua_State *);
static int  zone_attr_getfield(lua_State *);
static int  zone_attr_setfield(lua_State *);
static void zone_attr_destroy(void *);
/* static attr_def_t *check_zone_attr(lua_State *, int); */
/* static attr_def_t *to_zone_attr(lua_State *, int); */
static bool fetch_zone_attribute(attr_t *, int, mrp_attr_t *);
static bool update_zone_attribute(attr_t *, int, mrp_attr_t *);

static int  resclass_create_from_lua(lua_State *);
static int  resclass_getfield(lua_State *);
static int  resclass_setfield(lua_State *);
static void resclass_destroy(void *);
/* static resclass_t *check_resclass(lua_State *, int); */
static resclass_t *to_resclass(lua_State *, int);

static int  resource_getfield(lua_State *);
static int  resource_setfield(lua_State *);
static resource_t *check_resource(lua_State *, int);
static bool fetch_resource_attribute(attr_t *, int, mrp_attr_t *);
static bool update_resource_attribute(attr_t *, int, mrp_attr_t *);

static mrp_lua_resmethod_t *resmethod_create_from_c(lua_State *);
static int  resmethod_create_from_lua(lua_State *);
static int  resmethod_getfield(lua_State *);
static int  resmethod_setfield(lua_State *);
static void resmethod_destroy(void *);
static mrp_lua_resmethod_t *to_resmethod(lua_State *, int);

static mrp_attr_def_t *check_attrdefs(lua_State *, int, int *);
static void free_attrdefs(mrp_attr_def_t *);
static mrp_attr_t *check_attrs(lua_State *, int, attr_def_t *);
static void free_attrs(mrp_attr_t *);
static int check_attrindex(lua_State *, int, attr_def_t *);
static int check_boolean(lua_State *, int);
static mrp_resource_order_t check_order(lua_State *, int);
static int push_order(lua_State *, mrp_resource_order_t);
static field_t field_check(lua_State *, int, const char **);
static field_t field_name_to_type(const char *, size_t);

static int method_recalc(lua_State *);


MRP_LUA_METHOD_LIST_TABLE (
    zone_attr_methods,       /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (zone_attr_create)
);

MRP_LUA_METHOD_LIST_TABLE (
    resclass_methods,         /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (resclass_create_from_lua)
);


MRP_LUA_METHOD_LIST_TABLE (
    attributes_overrides,    /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (attributes_getvalue)
    MRP_LUA_OVERRIDE_SETFIELD   (attributes_setvalue)
    MRP_LUA_OVERRIDE_GETLENGTH  (attributes_getlength)
);

MRP_LUA_METHOD_LIST_TABLE (
    zone_attr_overrides,     /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (zone_attr_create)
    MRP_LUA_OVERRIDE_GETFIELD   (zone_attr_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (zone_attr_setfield)
);

MRP_LUA_METHOD_LIST_TABLE (
    resclass_overrides,       /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (resclass_create_from_lua)
    MRP_LUA_OVERRIDE_GETFIELD   (resclass_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (resclass_setfield)
);

MRP_LUA_METHOD_LIST_TABLE (
    resource_overrides,       /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (resource_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (resource_setfield)
);

MRP_LUA_CLASS_DEF_SIMPLE (
    application_class,          /* main class & constructor name */
    appclass_t,                 /* userdata type */
    appclass_destroy,           /* userdata destructor */
    MRP_LUA_METHOD_LIST (       /* main class methods */
        MRP_LUA_METHOD_CONSTRUCTOR  (appclass_create)
    ),
    MRP_LUA_METHOD_LIST (       /* main class overrides */
        MRP_LUA_OVERRIDE_CALL       (appclass_create)
        MRP_LUA_OVERRIDE_GETFIELD   (appclass_getfield)
        MRP_LUA_OVERRIDE_SETFIELD   (appclass_setfield)
    )
);

MRP_LUA_CLASS_DEF_SIMPLE (
    zone,                       /* main class & constructor name */
    zone_t,                     /* userdata type */
    zone_destroy,               /* userdata destructor */
    MRP_LUA_METHOD_LIST (       /* main class methods */
        MRP_LUA_METHOD_CONSTRUCTOR  (zone_create)
    ),
    MRP_LUA_METHOD_LIST (       /* main class overrides */
        MRP_LUA_OVERRIDE_CALL       (zone_create)
        MRP_LUA_OVERRIDE_GETFIELD   (zone_getfield)
        MRP_LUA_OVERRIDE_SETFIELD   (zone_setfield)
    )
);

MRP_LUA_CLASS_DEF (
    zone,                       /* main class name */
    attributes,                 /* constructor name */
    attr_def_t,                 /* userdata type */
    zone_attr_destroy,          /* userdata destructor */
    zone_attr_methods,          /* class methods */
    zone_attr_overrides         /* class overrides */
);

MRP_LUA_CLASS_DEF (
    resource,                   /* main class name */
    class,                      /* constructor name */
    resclass_t,                 /* userdata type */
    resclass_destroy,           /* userdata destructor */
    resclass_methods,           /* class methods */
    resclass_overrides          /* class overrides */
);


MRP_LUA_CLASS_DEF_SIMPLE (
    resource,                   /* main class name */
    mrp_lua_resmethod_t,        /* userdata type */
    resmethod_destroy,          /* userdata destructor */
    MRP_LUA_METHOD_LIST (
        MRP_LUA_METHOD_CONSTRUCTOR  (resmethod_create_from_lua)
    ),
    MRP_LUA_METHOD_LIST (
        MRP_LUA_OVERRIDE_CALL       (resmethod_create_from_lua)
        MRP_LUA_OVERRIDE_GETFIELD   (resmethod_getfield)
        MRP_LUA_OVERRIDE_SETFIELD   (resmethod_setfield)
    )
);

attr_def_t           *zone_attr_defs;
attr_def_t           *resource_attr_defs[MRP_RESOURCE_MAX];
mrp_lua_resmethod_t  *resource_methods;


void mrp_resource_configuration_init(void)
{
    static bool initialised = false;

    lua_State *L;

    if (!initialised && (L =  mrp_lua_get_lua_state())) {

        appclass_class_create(L);
        zone_class_create(L);
        resclass_class_create(L);
        resource_class_create(L);

        mrp_resource_lua_init(L);

        resource_methods_create(L);

        mrp_debug("lua classes are ready for resource "
                  "configuration and management");

        initialised = true;
    }
}


mrp_lua_resmethod_t *mrp_lua_get_resource_methods(void)
{
    return resource_methods;
}

uint32_t mrp_lua_to_resource_id(lua_State *L, int t)
{
    resclass_t *rc = to_resclass(L, t);

    return rc ? rc->id : MRP_RESOURCE_ID_INVALID;
}

void mrp_lua_resclass_create_from_c(uint32_t id)
{
    lua_State *L;
    mrp_resource_def_t *rdef;
    resclass_t *resclass;
    attr_def_t *adef;
    mrp_attr_def_t *attrs;
    int nattr;

    if ((L = mrp_lua_get_lua_state())) {

        if (!(rdef = mrp_resource_definition_find_by_id(id)))
            luaL_error(L, "invalid resource definition ID %d", id);

        resclass = (resclass_t *)mrp_lua_create_object(L, RESCLASS_CLASS,
                                                       rdef->name,0);
        adef = mrp_allocz(sizeof(attr_def_t));
        nattr = rdef->nattr;
        attrs = mrp_allocz(sizeof(mrp_attr_def_t) * (nattr + 1));

        if (!nattr)
            mrp_attribute_copy_definitions(rdef->attrdefs, attrs);

        if (!resclass)
            luaL_error(L, "invalid or duplicate name '%s'", rdef->name);
        if (!adef)
            luaL_error(L,"can't to allocate memory for attribute definitions");

        resclass->id = id;
        resclass->name = mrp_strdup(rdef->name);
        resclass->attrs = attrs;

        adef->nattr = nattr;
        adef->attrs = attrs;

        resource_attr_defs[id] = adef;

        lua_pop(L, 1);

        mrp_log_info("resource class '%s' created", rdef->name);
    }
}

int mrp_lua_resource_create(lua_State *L, mrp_resource_t *res)
{
    mrp_resource_def_t *rdef;
    attr_def_t *adef;
    resource_t *r;

    MRP_LUA_ENTER;

    MRP_ASSERT(res, "invalid argument");

    if (!(rdef = res->def))
        lua_pushnil(L);
    else {
        adef = resource_attr_defs[rdef->id];

        MRP_ASSERT(resource_attr_defs[rdef->id], "can't find attribute defs");

        r = lua_newuserdata(L, sizeof(resource_t));

        r->rsetid = res->rsetid;
        r->resid = rdef->id;
        r->name = rdef->name;
        r->attr_tbl = attributes_create(L, RESOURCE,r, adef,
                                        fetch_resource_attribute,
                                        update_resource_attribute);

        luaL_getmetatable(L, RESOURCE_CLASSID);
        lua_setmetatable(L, -2);
    }

    MRP_LUA_LEAVE(1);
}


static void attributes_class_create(lua_State *L)
{
    /* create a metatable for attributes */
    luaL_newmetatable(L, ATTRIBUTE_CLASSID);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, attributes_overrides, 0);
}

static void appclass_class_create(lua_State *L)
{
    mrp_lua_create_object_class(L, APPCLASS_CLASS);
}

static void zone_class_create(lua_State *L)
{
    mrp_lua_create_object_class(L, ZONE_CLASS);
    mrp_lua_create_object_class(L, ZONE_ATTR_CLASS);

    attributes_class_create(L);

    zone_attr_defs = mrp_lua_create_object(L, ZONE_ATTR_CLASS, NULL,0);
    mrp_lua_set_object_name(L, ZONE_ATTR_CLASS, "attributes");
    lua_pop(L, 1);
}

static void resclass_class_create(lua_State *L)
{
    mrp_lua_create_object_class(L, RESCLASS_CLASS);
}

static int resource_destructor(lua_State *L)
{
    resource_t *r;

    if ((r = lua_touserdata(L, -1)) != NULL) {
        mrp_debug("destroying Lua resource %p", r);
        luaL_unref(L, LUA_REGISTRYINDEX, r->attr_tbl);
        r->attr_tbl = LUA_NOREF;
    }

    return 0;
}

static void resource_class_create(lua_State *L)
{
    /* create a metatable for resource instances */
    luaL_newmetatable(L, RESOURCE_CLASSID);
    lua_pushcfunction(L, resource_destructor);
    lua_setfield(L, -2, "__gc");
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, resource_overrides, 0);
}

static void resource_methods_create(lua_State *L)
{
    typedef struct {
        const char *name;
        lua_CFunction func;
    } method_def_t;

    static method_def_t method_defs[] = {
        { "recalc",  method_recalc },
        {   NULL  ,      NULL      }
    };

    method_def_t *md;

    mrp_lua_create_object_class(L, RESMETHOD_CLASS);
    resource_methods = resmethod_create_from_c(L);

    for (md = method_defs;  md->name;   md++) {
        lua_pushstring(L, md->name);
        lua_pushcfunction(L, md->func);
        lua_rawset(L, -3);
    }
}


static int attributes_create(lua_State *L,
                             attr_owner_t type, void *data,
                             attr_def_t *def,
                             attribute_access_t fetch,
                             attribute_access_t update)
{
    attr_t *attr;
    int tblref;

    MRP_LUA_ENTER;

    attr = lua_newuserdata(L, sizeof(attr_t));

    attr->owner.type = type;
    attr->owner.data = data;
    attr->def = def;
    attr->fetch = fetch;
    attr->update = update;

    luaL_getmetatable(L, ATTRIBUTE_CLASSID);
    lua_setmetatable(L, -2);

    tblref = luaL_ref(L, LUA_REGISTRYINDEX);

    MRP_LUA_LEAVE(tblref);
}

static int attributes_getvalue(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    int idx = check_attrindex(L, 2, attr->def);
    mrp_attr_def_t *def = attr->def->attrs + idx;
    mrp_attr_t av;

    MRP_LUA_ENTER;

    if (idx < 0) {
        lua_pushnil(L);
        return 1;
    }

    if (!(def->access & MRP_RESOURCE_READ)) {
        luaL_error(L, "attempt to read a non-readable attribute %s",
                   def->name);
        MRP_LUA_LEAVE(0);
    }

    if (!attr->fetch(attr, idx, &av)) {
        lua_pushnil(L);
        MRP_LUA_LEAVE(1);
    }

    switch (def->type) {
    case mqi_string:
        if (av.value.string)
            lua_pushstring(L, av.value.string);
        else
            lua_pushnil(L);
        break;
    case mqi_integer:
    case mqi_unsignd:
        lua_pushinteger(L, av.value.integer);
        break;
    case mqi_floating:
        lua_pushnumber(L, av.value.floating);
        break;
    default:
        lua_pushnil(L);
        break;
    }

    MRP_LUA_LEAVE(1);
}

static int attributes_setvalue(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    int idx = check_attrindex(L, 2, attr->def);
    mrp_attr_def_t *def = attr->def->attrs + idx;
    mrp_attr_t av;

    MRP_LUA_ENTER;

    if (idx < 0)
        luaL_error(L, "attribute %s dows not exist", def->name);

    if (!(def->access & MRP_RESOURCE_WRITE))
        luaL_error(L, "attempt to read a readonly attribute %s", def->name);

    switch (def->type) {
    case mqi_string:
        av.value.string = luaL_checkstring(L, 3);
        break;
    case mqi_integer:
        av.value.integer =  luaL_checkinteger(L, 3);
        break;
    case mqi_unsignd:
        if ((av.value.integer = luaL_checkinteger(L, 3)) < 0) {
            luaL_error(L, "attempt to update an unsigned attribute "
                       "with negative value");
        }
        break;
    case mqi_floating:
        av.value.floating = luaL_checknumber(L, 3);
        break;
    default:
        luaL_error(L, "internal error: invalid attribute type");
        break;
    }

    if (!attr->update(attr, idx, &av))
        luaL_error(L, "attribute update failed");

    MRP_LUA_LEAVE(0);
}

static int attributes_getlength(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    attr_def_t *def = attr->def;

    MRP_LUA_ENTER;

    lua_pushinteger(L, def ? def->nattr : 0);

    MRP_LUA_LEAVE(1);
}

static attr_t *check_attributes(lua_State *L, int idx)
{
    return (attr_t *)luaL_checkudata(L, idx, ATTRIBUTE_CLASSID);
}

static int push_attributes(lua_State *L, int attr_tbl)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, attr_tbl);
    return 1;
}


static int appclass_create(lua_State *L)
{
    appclass_t *appclass;
    size_t fldnamlen;
    const char *fldnam;
    int priority = 0;
    int modal = -1;
    int share = -1;
    mrp_resource_order_t order = 0;
    const char *name = NULL;

    MRP_LUA_ENTER;

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case PRIORITY:
            priority = luaL_checkint(L, -1);
            break;

        case MODAL:
            modal = check_boolean(L, -1);
            break;

        case SHARE:
            share = check_boolean(L, -1);
            break;

        case ORDER:
            order = check_order(L, -1);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }
    }

    if (!name)
        luaL_error(L, "missing or wrong name field");
    if (modal < 0)
        luaL_error(L, "missing or wrong modal field");
    if (modal && share)
        luaL_error(L, "modal class can't share");
    if (share < 0)
        luaL_error(L, "missing or wrong share field");
    if (!order)
        luaL_error(L, "missing or wrong order field");
    if (priority < 0)
        luaL_error(L, "negative priority");
    if (!mrp_application_class_create(name, priority, modal, share, order))
        luaL_error(L, "failed to create application class '%s'", name);

    appclass = (appclass_t *)mrp_lua_create_object(L, APPCLASS_CLASS, name,0);

    if (!appclass)
        luaL_error(L, "invalid or duplicate name '%s'", name);
    else {
        appclass->name = name;
        mrp_log_info("application class '%s' created", name);
    }
    MRP_LUA_LEAVE(1);
}

static int appclass_getfield(lua_State *L)
{
    appclass_t *appclass = to_appclass(L, 1);
    field_t fld = field_check(L, 2, NULL);
    mrp_application_class_t *ac;

    MRP_LUA_ENTER;

    lua_pop(L, 1);

    if (!appclass || !(ac = mrp_application_class_find(appclass->name)))
        lua_pushnil(L);
    else {
        switch (fld) {
        case NAME:       lua_pushstring(L, ac->name);         break;
        case PRIORITY:   lua_pushinteger(L, ac->priority);    break;
        case MODAL:      lua_pushboolean(L, ac->modal);       break;
        case SHARE:      lua_pushboolean(L, ac->share);       break;
        case ORDER:      push_order(L, ac->order);            break;
        default:         lua_pushnil(L);                      break;
        }
    }

    MRP_LUA_LEAVE(1);
}

static int appclass_setfield(lua_State *L)
{
    MRP_LUA_ENTER;

    luaL_error(L, "can't modify application classes after definition");

    MRP_LUA_LEAVE(0);
}

static void appclass_destroy(void *data)
{
    appclass_t *appclass = (appclass_t *)data;

    MRP_LUA_ENTER;

    mrp_free((void *)appclass->name);

    MRP_LUA_LEAVE_NOARG;
}

#if 0
static appclass_t *check_appclass(lua_State *L, int idx)
{
    return (appclass_t *)mrp_lua_check_object(L, APPCLASS_CLASS, idx);
}
#endif

static appclass_t *to_appclass(lua_State *L, int idx)
{
    return (appclass_t *)mrp_lua_to_object(L, APPCLASS_CLASS, idx);
}


static int zone_create(lua_State *L)
{
    zone_t *zone;
    size_t fldnamlen;
    const char *fldnam;
    uint32_t id;
    const char *name = NULL;
    mrp_attr_t *attrs = NULL;

    MRP_LUA_ENTER;

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");

    if (!zone_attr_defs->attrs)
        luaL_error(L, "attempt to create zone before defining attributes");

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case ATTRIBUTES:
            attrs = check_attrs(L, -1, zone_attr_defs);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }
    }

    if (!name)
        luaL_error(L, "missing or wrong name field");
    if ((id = mrp_zone_create(name,attrs)) == MRP_ZONE_ID_INVALID)
        luaL_error(L, "failed to create zone");

    free_attrs(attrs);

    zone = (zone_t *)mrp_lua_create_object(L, ZONE_CLASS, name,0);

    if (!zone)
        luaL_error(L, "invalid or duplicate name '%s'", name);
    else {
        zone->id = id;
        zone->name = name;
        zone->attr_tbl = attributes_create(L, ZONE,zone, zone_attr_defs,
                                           fetch_zone_attribute,
                                           update_zone_attribute);

        mrp_log_info("zone '%s' created", name);
    }

    MRP_LUA_LEAVE(1);
}

static int zone_getfield(lua_State *L)
{
    zone_t  *zone = to_zone(L, 1);
    field_t  fld  = field_check(L, 2, NULL);

    MRP_LUA_ENTER;

    lua_pop(L, 1);

    if (!zone) {
        /* attempt to access a zone definition field */
        switch (fld) {
        case ATTRIBUTES:    mrp_lua_push_object(L, zone_attr_defs);     break;
        default:            lua_pushnil(L);                             break;
        }
    }
    else {
        /* attempt to access a zone instance field */
        switch (fld) {
        case ATTRIBUTES:     push_attributes(L, zone->attr_tbl);        break;
        case ID:             lua_pushinteger(L, zone->id + 1);          break;
        case NAME:           lua_pushstring(L, zone->name);             break;
        default:             lua_pushnil(L);                            break;
        }
    }

    MRP_LUA_LEAVE(1);
}

static int zone_setfield(lua_State *L)
{
    zone_t *zone = to_zone(L, 1);
    field_t fld  = field_check(L, 2, NULL);

    MRP_LUA_ENTER;

    if (zone || fld != ATTRIBUTES)
        luaL_error(L, "zones can't be exetended after definition");

    MRP_LUA_LEAVE(0);
}

static void zone_destroy(void *data)
{
    /* zone_t *zone = (zone_t *)data; */

    MRP_UNUSED(data);

    MRP_LUA_ENTER;

    MRP_LUA_LEAVE_NOARG;
}

#if 0
static zone_t *check_zone(lua_State *L, int idx)
{
    return (zone_t *)mrp_lua_check_object(L, ZONE_CLASS, idx);
}
#endif

static zone_t *to_zone(lua_State *L, int idx)
{
    return (zone_t *)mrp_lua_to_object(L, ZONE_CLASS, idx);
}

static int zone_attr_create(lua_State *L)
{
    mrp_attr_def_t *attrs;
    int nattr;

    MRP_LUA_ENTER;

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");

    if (zone_attr_defs->attrs)
        luaL_error(L, "zone attributes already defined");
    else {
        attrs = check_attrdefs(L, 2, &nattr);

        mrp_zone_definition_create(attrs);

        zone_attr_defs->nattr = nattr;
        zone_attr_defs->attrs = attrs;
    }

    mrp_lua_push_object(L, zone_attr_defs);

    mrp_log_info("zone attributes defined");

    MRP_LUA_LEAVE(1);
}

static int  zone_attr_getfield(lua_State *L)
{
    zone_t *zone;
    int idx;

    MRP_LUA_ENTER;

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");

    if (!(zone = to_zone(L, 1))) {
        mrp_debug("zone attribute definition => attribute index");
        if ((idx = check_attrindex(L, 2, zone_attr_defs)) < 0)
            lua_pushnil(L);
        else
            lua_pushinteger(L, idx);
    }
    else {
        mrp_debug("zone attribute => nil");
        lua_pushnil(L);
    }

    MRP_LUA_LEAVE(1);
}

static int  zone_attr_setfield(lua_State *L)
{
    MRP_UNUSED(L);

    MRP_LUA_ENTER;

    MRP_LUA_LEAVE(0);
}


static void zone_attr_destroy(void *data)
{
    /* attr_def_t *attr = (attr_def_t *)data; */

    MRP_UNUSED(data);

    MRP_LUA_ENTER;

    MRP_LUA_LEAVE_NOARG;
}

#if 0
static attr_def_t *check_zone_attr(lua_State *L, int idx)
{
    return (attr_def_t *)mrp_lua_check_object(L, ZONE_ATTR_CLASS, idx);
}
#endif

#if 0
static attr_def_t *to_zone_attr(lua_State *L, int idx)
{
    return (attr_def_t *)mrp_lua_to_object(L, ZONE_ATTR_CLASS, idx);
}
#endif

static bool fetch_zone_attribute(attr_t *attr, int idx, mrp_attr_t *retval)
{
    mrp_zone_t *z;
    zone_t *zone;

    if (attr->owner.type == ZONE && (zone = (zone_t *)attr->owner.data)) {
        if ((z = mrp_zone_find_by_id(zone->id))) {
            if (mrp_zone_read_attribute(z, idx, retval))
                return true;
        }
    }

    return false;
}

static bool update_zone_attribute(attr_t *attr, int idx, mrp_attr_t *value)
{
    mrp_zone_t *z;
    zone_t *zone;

    MRP_UNUSED(idx);
    MRP_UNUSED(value);

    if (attr->owner.type == ZONE && (zone = (zone_t *)attr->owner.data)) {
        if ((z = mrp_zone_find_by_id(zone->id))) {
#if 0
            if (mrp_zone_write_attribute(z, idx, value))
                return true;
#endif
        }
    }

    return false;
}

static int resclass_create_from_lua(lua_State *L)
{
    resclass_t *resclass;
    size_t fldnamlen;
    const char *fldnam;
    int nattr = 0;
    uint32_t id;
    const char *name = NULL;
    mrp_attr_def_t *attrs = NULL;
    bool shareable = false;
    bool sync_release = false;
    mrp_resource_mgr_ftbl_t *ftbl = NULL;
    void *mgrdata = NULL;
    attr_def_t *adef;

    MRP_LUA_ENTER;

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case SHAREABLE:
            luaL_argcheck(L, lua_isboolean(L,-1), 2, "attempt to assign "
                          "non-boolean value to 'shareable' field");
            shareable = lua_toboolean(L, -1);
            break;

        case SYNC_RELEASE:
            luaL_argcheck(L, lua_isboolean(L,-1), 2, "attempt to assign "
                          "non-boolean value to 'sync_release' field");
            sync_release = lua_toboolean(L, -1);
            break;


        case ATTRIBUTES:
            attrs = check_attrdefs(L, -1, &nattr);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }
    }

    if (!name)
        luaL_error(L, "missing or wrong name field");

    id = mrp_resource_definition_create_with_sync_release(name, shareable, sync_release, attrs,ftbl,mgrdata);

    MRP_ASSERT(id < MRP_RESOURCE_MAX, "resource id is out of range");

    if (id == MRP_RESOURCE_ID_INVALID)
        luaL_error(L, "failed to register resource class '%s'", name);

    resclass = (resclass_t *)mrp_lua_create_object(L, RESCLASS_CLASS, name,0);
    adef = mrp_allocz(sizeof(attr_def_t));

    if (!resclass)
        luaL_error(L, "invalid or duplicate name '%s'", name);
    if (!adef)
        luaL_error(L, "failed to allocate memory for attribute definitions");

    resclass->id = id;
    resclass->name = name;
    resclass->attrs = attrs;

    adef->nattr = nattr;
    adef->attrs = attrs;

    resource_attr_defs[id] = adef;

    mrp_log_info("resource class '%s' created", name);

    MRP_LUA_LEAVE(1);
}

static int resclass_getfield(lua_State *L)
{
    resclass_t *rc = to_resclass(L, 1);
    field_t fld = field_check(L, 2, NULL);
    mrp_resource_def_t *rd;

    MRP_LUA_ENTER;

    lua_pop(L, 1);

    if (!rc || !(rd = mrp_resource_definition_find_by_name(rc->name)))
        lua_pushnil(L);
    else {
        switch (fld) {
        case NAME:          lua_pushstring(L, rd->name);            break;
        case ID:            lua_pushinteger(L, rd->id + 1);         break;
        case SHAREABLE:     lua_pushboolean(L, rd->shareable);      break;
        case SYNC_RELEASE:  lua_pushboolean(L, rd->sync_release);   break;
        default:            lua_pushnil(L);                         break;
        }
    }

    MRP_LUA_LEAVE(1);
}

static int resclass_setfield(lua_State *L)
{
    MRP_LUA_ENTER;

    luaL_error(L, "can't modify resource classes after definition");

    MRP_LUA_LEAVE(1);
}

static void resclass_destroy(void *data)
{
    resclass_t *rc = (resclass_t *)data;

    MRP_LUA_ENTER;

    mrp_free((void *)rc->name);
    free_attrdefs(rc->attrs);

    MRP_LUA_LEAVE_NOARG;
}

#if 0
static resclass_t *check_resclass(lua_State *L, int idx)
{
    return (resclass_t *)mrp_lua_check_object(L, RESCLASS_CLASS, idx);
}
#endif

static resclass_t *to_resclass(lua_State *L, int idx)
{
    return (resclass_t *)mrp_lua_to_object(L, RESCLASS_CLASS, idx);
}

static int resource_getfield(lua_State *L)
{
    const char *name;
    resource_t *res = check_resource(L, 1);
    field_t fld = field_check(L, 2, &name);
    mrp_resource_set_t *s;
    mrp_resource_t *r;
    mrp_resource_mask_t m;

    MRP_LUA_ENTER;

    switch (fld) {

    case ATTRIBUTES:
        push_attributes(L, res->attr_tbl);
        break;

    case SHARED:
    case SHARE:
        if (!(r = mrp_resource_set_find_resource(res->rsetid, res->name)))
            lua_pushnil(L);
        else
            lua_pushboolean(L, r->shared);
        break;

    default:
        if (!(s = mrp_resource_set_find_by_id(res->rsetid))) {
            lua_pushnil(L);
            break;
        } else {
            m = ((mrp_resource_mask_t)1) << res->resid;
        }

        switch (fld) {
        case MANDATORY:
            lua_pushboolean(L, s->resource.mask.mandatory & m ? true : false);
            break;
        case GRANT:
            lua_pushboolean(L, s->resource.mask.grant & m ? true : false);
            break;
        default:
            lua_pushnil(L);
            break;
        }

        break;
    }

    MRP_LUA_LEAVE(1);
}

static int resource_setfield(lua_State *L)
{
    MRP_UNUSED(L);

    MRP_LUA_ENTER;

    MRP_LUA_LEAVE(0);
}

static resource_t *check_resource(lua_State *L, int idx)
{
    return (resource_t *)luaL_checkudata(L, idx, RESOURCE_CLASSID);
}

static bool fetch_resource_attribute(attr_t *attr, int idx, mrp_attr_t *retval)
{
    mrp_resource_set_t *rset;
    resource_t *resource;
    mrp_attr_t *a;

    if (attr->owner.type == RESOURCE) {
        if ((resource = (resource_t *)attr->owner.data)) {
            if ((rset = mrp_resource_set_find_by_id(resource->rsetid))) {
                a = mrp_resource_set_read_attribute(rset, resource->name,
                                                    idx, retval);
                return a ? true : false;
            }
        }
    }

    return false;
}


static bool update_resource_attribute(attr_t *attr, int idx, mrp_attr_t *value)
{
    mrp_resource_set_t *rset;
    resource_t *resource;
    mrp_attr_t values[2];
    int sts;

    MRP_UNUSED(idx);

    if (attr->owner.type == RESOURCE) {
        if ((resource = (resource_t *)attr->owner.data)) {
            if ((rset = mrp_resource_set_find_by_id(resource->rsetid))) {
                memcpy(values + 0, value, sizeof(mrp_attr_t));
                memset(values + 1, 0,     sizeof(mrp_attr_t));

                sts = mrp_resource_set_write_attributes(rset, resource->name,
                                                        values);
                return (sts < 0) ? false : true;
            }
        }
    }

    return false;
}

static mrp_lua_resmethod_t *resmethod_create_from_c(lua_State *L)
{
    mrp_lua_resmethod_t *method = mrp_lua_create_object(L, RESMETHOD_CLASS,
                                                        "method",0);

    if (!method)
        luaL_error(L, "invalid or duplicate name 'method'");

    return method;
}

static int resmethod_create_from_lua(lua_State *L)
{
    MRP_LUA_ENTER;

    luaL_error(L, "singleton object has already been created");

    lua_pushnil(L);

    MRP_LUA_LEAVE(1);
}

static int resmethod_getfield(lua_State *L)
{
    const char *name;
    mrp_lua_resmethod_t *method = to_resmethod(L, 1);
    field_t fld = field_check(L, 2, &name);

    MRP_LUA_ENTER;

    lua_pop(L, 1);

    if (!method) {
        /* attempt to access a resclass or owners */
        switch (fld) {
        case METHOD:   mrp_lua_push_object(L, resource_methods);  break;
        case OWNERS:   lua_pushstring(L,name); lua_rawget(L,1);   break;
        default:       lua_pushnil(L);                            break;
        }
    }
    else {
        /* attempt to access a method member */
        if (!resource_methods)
            lua_pushnil(L);
        else {
            switch (fld) {
            case VETO:
            case RECALC:
                lua_pushstring(L, name);
                lua_rawget(L, 1);
                break;
            default:
                lua_pushnil(L);
                break;
            }
        }
    }

    MRP_LUA_LEAVE(1);
}

static int resmethod_setfield(lua_State *L)
{
    const char *name;
    mrp_lua_resmethod_t *method = to_resmethod(L, 1);
    field_t fld = field_check(L, 2, &name);

    MRP_LUA_ENTER;

    if (method) {
        switch (fld) {
        case VETO:
            lua_pushstring(L, name);
            lua_pushvalue(L, 3);
            method->veto = mrp_funcarray_check(L, -1);
            lua_rawset(L, 1);
            break;
        default:
            luaL_error(L, "invalid method '%s'", name);
            break;
        }
    }

    MRP_LUA_LEAVE(0);
}

static void resmethod_destroy(void *data)
{
    mrp_lua_resmethod_t *method = (mrp_lua_resmethod_t *)data;

    MRP_LUA_ENTER;

    method->veto = NULL;

    MRP_LUA_LEAVE_NOARG;
}

static mrp_lua_resmethod_t *to_resmethod(lua_State *L, int idx)
{
    return (mrp_lua_resmethod_t *)mrp_lua_to_object(L, RESMETHOD_CLASS, idx);
}


static mrp_attr_def_t *check_attrdefs(lua_State *L, int t, int *ret_len)
{
    mrp_attr_def_t attrdefs[128];
    mrp_attr_def_t *ad, *end, *dup;
    size_t i;
    const char *name;
    const char *string;
    const char *access;
    bool value_set;
    int len;
    size_t size;
    size_t namlen;

    t = (t < 0) ? lua_gettop(L) + t + 1 : t;

    luaL_checktype(L, t, LUA_TTABLE);

    end = (ad = attrdefs) + (MRP_ARRAY_SIZE(attrdefs) - 1);

    MRP_LUA_FOREACH_FIELD(L, t, name, namlen) {
        if (!name[0])
            luaL_error(L, "invalid attribute definition");
        if (ad >= end)
            luaL_error(L, "too many attributes");

        ad->name = mrp_strdup(name);
        ad->type = mqi_error;
        ad->access = MRP_RESOURCE_READ;

        value_set = false;

        luaL_checktype(L, -1, LUA_TTABLE);


        for (lua_pushnil(L);  lua_next(L, -2);  lua_pop(L, 1)) {
            if (lua_type(L, -2) != LUA_TNUMBER)
                goto error;

            i = lua_tointeger(L, -2);

            switch (i) {
            case 1:
                ad->type = lua_tointeger(L, -1);
                break;
            case 2:
                switch (ad->type) {
                case mqi_string:
                    if ((string = lua_tostring(L, -1))) {
                        ad->value.string = mrp_strdup(string);
                        value_set = true;
                    }
                    break;
                case mqi_integer:
                    ad->value.integer = lua_tointeger(L, -1);
                    value_set = true;
                    break;
                case mqi_unsignd:
                    ad->value.integer = lua_tointeger(L, -1);
                    value_set = true;
                    break;
                case mqi_floating:
                    ad->value.floating = lua_tonumber(L, -1);
                    value_set = true;
                    break;
                default:
                    break;
                }
                break;
            case 3:
                if (!(access = lua_tostring(L, -1)))
                    ad->type = mqi_error;
                else {
                    if (!strcasecmp(access, "read"))
                        ad->access = MRP_RESOURCE_READ;
                    else if (!strcasecmp(access, "write"))
                        ad->access = MRP_RESOURCE_WRITE;
                    else if (!strcasecmp(access, "rw"))
                        ad->access = MRP_RESOURCE_RW;
                    else
                        ad->type = mqi_error;
                }
                break;
            default:
                ad->type = mqi_error;
                break;
            }
        } /* for */

        if (!value_set ||
            (ad->type != mqi_string  &&
             ad->type != mqi_integer &&
             ad->type != mqi_unsignd &&
             ad->type != mqi_floating ))
            goto error;

        ad++;
    } /* foreach */

    memset(ad, 0, sizeof(mrp_attr_def_t));

    len  = ad - attrdefs;
    size = sizeof(mrp_attr_def_t) * (len+1);
    dup  = mrp_alloc(size);

    if (!dup)
        luaL_error(L, "failed to allocate %u byte memory", size);

    memcpy(dup, attrdefs, size);

    if (ret_len)
        *ret_len = len;

    return dup;

 error:
    luaL_argerror(L, t, "malformed attribute definition");
    return NULL;
}


static void free_attrdefs(mrp_attr_def_t *attrdefs)
{
    mrp_attr_def_t *ad;

    if (attrdefs) {
        for (ad = attrdefs;  ad->name;  ad++) {
            mrp_free((void *)ad->name);
            if (ad->type == mqi_string)
                mrp_free((void *)ad->value.string);
        }
        mrp_free(attrdefs);
    }
}

static int attr_name_to_index(const char *name, attr_def_t *def)
{
    mrp_attr_def_t *attrs = def->attrs;
    int idx;

    for (idx = 0;  idx < def->nattr;  idx++) {
        if (!strcmp(name, attrs[idx].name))
            return idx;
    }

    return -1;
}

static mrp_attr_t *check_attrs(lua_State *L, int t, attr_def_t *defs)
{
    mrp_attr_t attr[128];
    mrp_attr_t *at, *end, *dup;
    const char *name;
    size_t namlen;
    size_t len;
    size_t size;
    int i;

    t = (t < 0) ? lua_gettop(L) + t + 1 : t;

    luaL_checktype(L, t, LUA_TTABLE);

    end = (at = attr) + (MRP_ARRAY_SIZE(attr) - 1);

    MRP_LUA_FOREACH_FIELD(L, t, name, namlen) {
        if (!name[0])
            luaL_error(L, "invalid attribute definition");
        if (at >= end)
            luaL_error(L, "too many attributes");
        if ((i = attr_name_to_index(name, defs)) < 0)
            luaL_error(L, "attribute %s do not exist", name);

        at->name = mrp_strdup(name);

        switch ((at->type = defs->attrs[i].type)) {
        case mqi_string:
            at->value.string = mrp_strdup(luaL_checkstring(L,-1));
            break;
        case mqi_integer:
            at->value.integer = luaL_checkinteger(L,-1);
            break;
        case mqi_unsignd:
            if ((at->value.integer = luaL_checkinteger(L,-1)) < 0)
                luaL_error(L, "attempt to give negative value to an "
                           "unsigned integer");
            break;
        default:
            luaL_error(L, "Internal error: invalid type for attribute");
            break;
        }

        at++;
    }

    memset(at, 0, sizeof(mrp_attr_t));

    len  = at - attr;
    size = sizeof(mrp_attr_t) * (len + 1);

    dup = mrp_alloc(size);
    memcpy(dup, attr, size);

    return dup;
}


static void free_attrs(mrp_attr_t *attrs)
{
    mrp_attr_t *at;

    if (attrs) {
        for (at = attrs;  at->name;  at++) {
            mrp_free((void *)at->name);
            if (at->type == mqi_string)
                mrp_free((void *)at->value.string);
        }
        mrp_free(attrs);
    }
}


static int check_attrindex(lua_State *L, int arg, attr_def_t *def)
{
    const char *name;
    int idx;

    if (!def || !def->attrs)
        return -1;

    switch (lua_type(L, arg)) {
    case LUA_TNUMBER:
        idx = lua_tointeger(L, arg);
        return (idx >= 0 && idx < def->nattr) ? idx : -1;
    case LUA_TSTRING:
        name = lua_tostring(L, arg);
        return attr_name_to_index(name, def);
    default:
        return -1;
    }
}


static int check_boolean(lua_State *L, int idx)
{
    if (!lua_isboolean(L, idx))
        luaL_argerror(L, idx, "expected boolean");
    return lua_toboolean(L, idx) ? 1 : 0;
}

static mrp_resource_order_t check_order(lua_State *L, int idx)
{
    const char *str = luaL_checkstring(L, idx);

    if (!strcasecmp(str, "fifo"))
        return MRP_RESOURCE_ORDER_FIFO;

    if (!strcasecmp(str, "lifo"))
        return MRP_RESOURCE_ORDER_LIFO;

    luaL_error(L, "invalid value for order ('fifo' or 'lifo' accepted only)");

    return MRP_RESOURCE_ORDER_UNKNOWN;
}

static int push_order(lua_State *L, mrp_resource_order_t order)
{
    const char *str;

    switch (order) {
    case MRP_RESOURCE_ORDER_FIFO:   str = "fifo";        break;
    case MRP_RESOURCE_ORDER_LIFO:   str = "lifo";        break;
    default:                        str = "<unknown>";   break;
    }

    lua_pushstring(L, str);

    return 1;
}


static field_t field_check(lua_State *L, int idx, const char **ret_fldnam)
{
    const char *fldnam;
    size_t fldnamlen;
    field_t fldtyp;

    if (!(fldnam = lua_tolstring(L, idx, &fldnamlen)))
        fldtyp = 0;
    else
        fldtyp = field_name_to_type(fldnam, fldnamlen);

    if (ret_fldnam)
        *ret_fldnam = fldnam;

    return fldtyp;
}


static field_t field_name_to_type(const char *name, size_t len)
{
    switch (len) {

    case 2:
        if (!strcmp(name, "id"))
            return ID;
        break;

    case 4:
        if (!strcmp(name, "name"))
            return NAME;
        if (!strcmp(name, "veto"))
            return VETO;
        break;

    case 5:
        if (!strcmp(name, "class"))
            return CLASS;
        if (!strcmp(name, "modal"))
            return MODAL;
        if (!strcmp(name, "share"))
            return SHARE;
        if (!strcmp(name, "grant"))
            return GRANT;
        if (!strcmp(name, "order"))
            return ORDER;
        break;

    case 6:
        if (!strcmp(name, "method"))
            return METHOD;
        if (!strcmp(name, "owners"))
            return OWNERS;
        if (!strcmp(name, "shared"))
            return SHARED;
        if (!strcmp(name, "recalc"))
            return RECALC;
        break;

    case 8:
        if (!strcmp(name, "priority"))
            return PRIORITY;
        break;

    case 9:
        if (!strcmp(name, "mandatory"))
            return MANDATORY;
        if (!strcmp(name, "shareable"))
            return SHAREABLE;
        break;

    case 10:
        if (!strcmp(name, "attributes"))
            return ATTRIBUTES;
        break;

    case 12:
        if (!strcmp(name, "sync_release"))
            return SYNC_RELEASE;
        break;

    default:
        break;
    }

    return 0;
}

static int method_recalc(lua_State *L)
{
    const char *zone_name;
    mrp_zone_t *zone;

    if (lua_type(L, 1) == LUA_TSTRING && (zone_name = lua_tostring(L, 1))) {

        if (!(zone = mrp_zone_find_by_name(zone_name))) {
            luaL_error(L, "can't recalculate resources in zone '%s': "
                       "no such zone", zone_name);
        }

        mrp_resource_owner_recalc(zone->id);
    }

    return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
