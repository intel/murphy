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
#include <murphy/core/lua-utils/object.h>

#include "config-lua.h"
#include "config-api.h"
#include "manager-api.h"
#include "zone.h"
#include "application-class.h"
#include "resource.h"


#define ZONE_CLASS           MRP_LUA_CLASS_SIMPLE(zone)
#define APPCLASS_CLASS       MRP_LUA_CLASS_SIMPLE(application_class)
#define ZONE_ATTR_CLASS      MRP_LUA_CLASS(zone, attributes)
#define RESCLASS_CLASS       MRP_LUA_CLASS(resource, class)

#define ATTRIBUTE_CLASSID    MRP_LUA_CLASSID_ROOT "attribute"

typedef enum   field_e       field_t;
typedef enum   attr_owner_e  attr_owner_t;
typedef struct appclass_s    appclass_t;
typedef struct zone_s        zone_t;
typedef struct resclass_s    resclass_t;
typedef struct attr_def_s    attr_def_t;
typedef struct attr_s        attr_t;

typedef bool (*attribute_access_t)(attr_t *, int, mrp_attr_t *);

enum field_e {
    ATTRIBUTES = 1,
    NAME,
    PRIORITY,
    SHAREABLE,
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


static void attributes_class_create(lua_State *L);
static int  attributes_create(lua_State *, int, attr_owner_t, void *,
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
static appclass_t *check_appclass(lua_State *, int);
static appclass_t *to_appclass(lua_State *, int);

static int  zone_create(lua_State *);
static int  zone_getfield(lua_State *);
static int  zone_setfield(lua_State *);
static void zone_destroy(void *);
static zone_t *check_zone(lua_State *, int);
static zone_t *to_zone(lua_State *, int);

static int  zone_attr_create(lua_State *);
static int  zone_attr_getfield(lua_State *);
static int  zone_attr_setfield(lua_State *);
static void zone_attr_destroy(void *);
static attr_def_t *check_zone_attr(lua_State *, int);
static attr_def_t *to_zone_attr(lua_State *, int);
static bool fetch_zone_attribute(attr_t *, int, mrp_attr_t *);
static bool update_zone_attribute(attr_t *, int, mrp_attr_t *);

static int  resclass_create_from_lua(lua_State *);
static int  resclass_getfield(lua_State *);
static int  resclass_setfield(lua_State *);
static void resclass_destroy(void *);
static resclass_t *check_resclass(lua_State *, int);
static resclass_t *to_resclass(lua_State *, int);

static mrp_attr_def_t *check_attrdefs(lua_State *, int, int *);
static void free_attrdefs(mrp_attr_def_t *);
static mrp_attr_t *check_attrs(lua_State *, int, attr_def_t *);
static void free_attrs(mrp_attr_t *);
static int check_attrindex(lua_State *, int, attr_def_t *);
static field_t field_check(lua_State *, int, const char **);
static field_t field_name_to_type(const char *, size_t);


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

attr_def_t *zone_attr_defs;


void mrp_lua_create_application_class(lua_State *L)
{
    mrp_lua_create_object_class(L, APPCLASS_CLASS);
}

void mrp_lua_create_zone_class(lua_State *L)
{
    mrp_lua_create_object_class(L, ZONE_CLASS);
    mrp_lua_create_object_class(L, ZONE_ATTR_CLASS);

    attributes_class_create(L);

    zone_attr_defs = mrp_lua_create_object(L, ZONE_ATTR_CLASS, NULL);
    mrp_lua_set_object_name(L, ZONE_ATTR_CLASS, "attributes");
    lua_pop(L, 1);
}

void mrp_lua_create_resource_class_class(lua_State *L)
{
    mrp_lua_create_object_class(L, RESCLASS_CLASS);
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

static int attributes_create(lua_State *L, int tbl,
                             attr_owner_t type, void *data,
                             attr_def_t *def,
                             attribute_access_t fetch,
                             attribute_access_t update)
{
    attr_t *attr;

    tbl = (tbl < 0) ? lua_gettop(L) + tbl + 1 : tbl;

    luaL_checktype(L, tbl, LUA_TTABLE);

    attr = lua_newuserdata(L, sizeof(attr_t));

    attr->owner.type = type;
    attr->owner.data = data;
    attr->def = def;
    attr->fetch = fetch;
    attr->update = update;

    luaL_getmetatable(L, ATTRIBUTE_CLASSID);
    lua_setmetatable(L, -2);

    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static int attributes_getvalue(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    int idx = check_attrindex(L, 2, attr->def);
    mrp_attr_def_t *def = attr->def->attrs + idx;
    mrp_attr_t av;

    if (idx < 0) {
        lua_pushnil(L);
        return 1;
    }

    if (!(def->access & MRP_RESOURCE_READ)) {
        luaL_error(L, "attempt to read a non-readable attribute %s",
                   def->name);
        return 0;
    }

    if (!attr->fetch(attr, idx, &av)) {
        lua_pushnil(L);
        return 1;
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

    return 1;
}

static int attributes_setvalue(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    int idx = check_attrindex(L, 2, attr->def);
    mrp_attr_def_t *def = attr->def->attrs + idx;
    mrp_attr_t av;

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

    return 0;
}

static int attributes_getlength(lua_State *L)
{
    attr_t *attr = check_attributes(L, 1);
    attr_def_t *def = attr->def;

    lua_pushinteger(L, def ? def->nattr : 0);

    return 1;
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
    int priority;
    const char *name = NULL;

    printf("**** appclass create\n");

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case PRIORITY:
            priority = luaL_checkint(L, -1);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }
    }

    if (!name)
        luaL_error(L, "missing or wrong name field");
    if (priority < 0)
        luaL_error(L, "negative priority");
    if (!mrp_application_class_create(name, priority))
        luaL_error(L, "failed to create application class '%s'", name);

    appclass = (appclass_t *)mrp_lua_create_object(L, APPCLASS_CLASS, name);

    if (!appclass)
        luaL_error(L, "invalid or duplicate name '%s'", name);

    appclass->name = name;

    return 1;
}

static int appclass_getfield(lua_State *L)
{
    appclass_t *appclass = to_appclass(L, 1);
    field_t fld = field_check(L, 2, NULL);
    mrp_application_class_t *ac;

    lua_pop(L, 1);

    if (!appclass || !(ac = mrp_application_class_find(appclass->name)))
        lua_pushnil(L);
    else {
        switch (fld) {
        case NAME:       lua_pushstring(L, ac->name);         break;
        case PRIORITY:   lua_pushinteger(L, ac->priority);    break;
        default:         lua_pushnil(L);                      break;
        }
    }

    return 1;
}

static int appclass_setfield(lua_State *L)
{
    luaL_error(L, "can't modify application classes after definition");
    return 0;
}

static void appclass_destroy(void *data)
{
    appclass_t *appclass = (appclass_t *)data;
    mrp_free((void *)appclass->name);
}

static appclass_t *check_appclass(lua_State *L, int idx)
{
    return (appclass_t *)mrp_lua_check_object(L, APPCLASS_CLASS, idx);
}

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

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");

    if (!zone_attr_defs->attrs)
        luaL_error(L, "attempt to create zone before defining attributes");

    printf("**** zone create\n");

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

    zone = (zone_t *)mrp_lua_create_object(L, ZONE_CLASS, name);

    if (!zone)
        luaL_error(L, "invalid or duplicate name '%s'", name);

    zone->id = id;
    zone->name = name;
    zone->attr_tbl = attributes_create(L, -1, ZONE,zone, zone_attr_defs,
                                       fetch_zone_attribute,
                                       update_zone_attribute);
    return 1;
}

static int zone_getfield(lua_State *L)
{
    zone_t  *zone = to_zone(L, 1);
    field_t  fld  = field_check(L, 2, NULL);

    lua_pop(L, 1);

    if (!zone) {
        /* attempt to access a zone definition field */
        switch (fld) {
        case ATTRIBUTES:
            mrp_lua_push_object(L, zone_attr_defs);
            break;
        default:
            lua_pushnil(L);
            break;
        }
    }
    else {
        /* attempt to access a zone instance field */
        switch (fld) {
        case ATTRIBUTES:
            push_attributes(L, zone->attr_tbl);
            break;
        case NAME:
            lua_pushstring(L, zone->name);
        default:
            lua_pushnil(L);
            break;
        }
    }

    return 1;
}

static int zone_setfield(lua_State *L)
{
    zone_t *zone = to_zone(L, 1);
    field_t fld  = field_check(L, 2, NULL);

    if (zone || fld != ATTRIBUTES)
        luaL_error(L, "zones can't be exetended after definition");
    else {
        printf("**** zone.attributes setfield\n");
    }
    return 0;
}

static void zone_destroy(void *data)
{
    zone_t *zone = (zone_t *)data;
}

static zone_t *check_zone(lua_State *L, int idx)
{
    return (zone_t *)mrp_lua_check_object(L, ZONE_CLASS, idx);
}

static zone_t *to_zone(lua_State *L, int idx)
{
    return (zone_t *)mrp_lua_to_object(L, ZONE_CLASS, idx);
}

static int zone_attr_create(lua_State *L)
{
    zone_t *zone;
    mrp_attr_def_t *attrs;
    int nattr;

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");

    printf("**** zone attribute definitions\n");

    if (zone_attr_defs->attrs)
        luaL_error(L, "zone attributes already defined");
    else {
        attrs = check_attrdefs(L, 2, &nattr);

        mrp_zone_definition_create(attrs);

        zone_attr_defs->nattr = nattr;
        zone_attr_defs->attrs = attrs;
    }

    mrp_lua_push_object(L, zone_attr_defs);

    return 1;
}

static int  zone_attr_getfield(lua_State *L)
{
    zone_t *zone;
    int idx;

    MRP_ASSERT(zone_attr_defs, "invocation prior to initialization");


    if (!(zone = to_zone(L, 1))) {
        printf("**** zone attribute definition getfield\n");
        if ((idx = check_attrindex(L, 2, zone_attr_defs)) < 0)
            lua_pushnil(L);
        else
            lua_pushinteger(L, idx);
    }
    else {
        printf("**** zone attribute getfield\n");
        lua_pushnil(L);
    }

    return 1;
}

static int  zone_attr_setfield(lua_State *L)
{
    return 0;
}


static void zone_attr_destroy(void *data)
{
    attr_def_t *attr = (attr_def_t *)data;
}

static attr_def_t *check_zone_attr(lua_State *L, int idx)
{
    return (attr_def_t *)mrp_lua_check_object(L, ZONE_ATTR_CLASS, idx);
}

static attr_def_t *to_zone_attr(lua_State *L, int idx)
{
    return (attr_def_t *)mrp_lua_to_object(L, ZONE_ATTR_CLASS, idx);
}

static bool fetch_zone_attribute(attr_t *attr, int idx, mrp_attr_t *ret_value)
{
    mrp_zone_t *z;
    zone_t *zone;

    if (attr->owner.type == ZONE && (zone = (zone_t *)attr->owner.data)) {
        if ((z = mrp_zone_find_by_id(zone->id))) {
            if (mrp_zone_read_attribute(z, idx, ret_value))
                return true;
        }
    }

    return false;
}

static bool update_zone_attribute(attr_t *attr, int idx, mrp_attr_t *value)
{
    mrp_zone_t *z;
    zone_t *zone;

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
    int nattr;
    uint32_t id;
    const char *name = NULL;
    mrp_attr_def_t *attrs = NULL;
    bool shareable = false;
    mrp_resource_mgr_ftbl_t *ftbl = NULL;
    void *mgrdata = NULL;

    printf("**** resclass create\n");

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

    id = mrp_resource_definition_create(name, shareable, attrs,ftbl,mgrdata);

    if (id == MRP_RESOURCE_ID_INVALID)
        luaL_error(L, "failed to register resource class '%s'", name);

    resclass = (resclass_t *)mrp_lua_create_object(L, RESCLASS_CLASS, name);

    if (!resclass)
        luaL_error(L, "invalid or duplicate name '%s'", name);

    resclass->id = id;
    resclass->name = name;
    resclass->attrs = attrs;

    return 1;
}

static int resclass_getfield(lua_State *L)
{
    resclass_t *rc = to_resclass(L, 1);
    field_t fld = field_check(L, 2, NULL);
    mrp_resource_def_t *rd;

    lua_pop(L, 1);

    if (!rc || !(rd = mrp_resource_definition_find_by_name(rc->name)))
        lua_pushnil(L);
    else {
        switch (fld) {
        case NAME:       lua_pushstring(L, rd->name);         break;
        case SHAREABLE:  lua_pushboolean(L, rd->shareable);   break;
        default:         lua_pushnil(L);                      break;
        }
    }

    return 1;
}

static int resclass_setfield(lua_State *L)
{
    luaL_error(L, "can't modify resource classes after definition");
    return 0;
}

static void resclass_destroy(void *data)
{
    resclass_t *rc = (resclass_t *)data;
    mrp_free((void *)rc->name);
    free_attrdefs(rc->attrs);
}

static resclass_t *check_resclass(lua_State *L, int idx)
{
    return (resclass_t *)mrp_lua_check_object(L, RESCLASS_CLASS, idx);
}

static resclass_t *to_resclass(lua_State *L, int idx)
{
    return (resclass_t *)mrp_lua_to_object(L, RESCLASS_CLASS, idx);
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

    case 4:
        if (!strcmp(name, "name"))
            return NAME;
        break;

    case 8:
        if (!strcmp(name, "priority"))
            return PRIORITY;
        break;

    case 9:
        if (!strcmp(name, "shareable"))
            return SHAREABLE;
        break;

    case 10:
        if (!strcmp(name, "attributes"))
            return ATTRIBUTES;
        break;

    default:
        break;
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
