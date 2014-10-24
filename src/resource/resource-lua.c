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
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-utils/object.h>

#include "resource-lua.h"
#include "config-lua.h"
#include "zone.h"
#include "resource.h"
#include "resource-set.h"
#include "resource-owner.h"
#include "application-class.h"
#include "client-api.h"

#define OWNERS_CLASS         MRP_LUA_CLASS(resource, owners)
#define SETREF_CLASS         MRP_LUA_CLASS(resource, sets)

#define OWNERREF_CLASSID     MRP_LUA_CLASSID_ROOT "resource.ownerref"
#define OWNERREF_USERDATA    MRP_LUA_CLASSID_ROOT "resource.ownerref.userdata"

typedef enum   field_e       field_t;
typedef struct ownerref_s    ownerref_t;

enum field_e {
    APPLICATION_CLASS = 1,
    AUTO_RELEASE,
    RESOURCE_SET,
    ATTRIBUTES,
    DONT_WAIT,
    RESOURCE,
    STATE,
    ID
};

struct ownerref_s {
    uint32_t zoneid;
    uint32_t resid;
};

static void owners_class_create(lua_State *);
static void ownerref_class_create(lua_State *);
static void setref_class_create(lua_State *);

static mrp_resource_ownersref_t *owners_get(lua_State *, uint32_t);
static int  owners_create(lua_State *);
static int  owners_getfield(lua_State *);
static int  owners_setfield(lua_State *);
static void owners_destroy(void *);
static mrp_resource_ownersref_t *owners_check(lua_State *, int);
/* static mrp_resource_owners_s *to_owners(lua_State *, int); */

static ownerref_t *ownerref_create(lua_State *, uint32_t, uint32_t);
static int ownerref_getfield(lua_State *);
static int ownerref_setfield(lua_State *);
static mrp_resource_owner_t *ownerref_check(lua_State *, int);

static int  setref_getfield(lua_State *);
static int  setref_setfield(lua_State *);
static void setref_destroy(void *);
static mrp_resource_setref_t *setref_check(lua_State *, int);

static void init_id_hash(void);
static int  add_to_id_hash(mrp_resource_setref_t *);
static mrp_resource_setref_t *remove_from_id_hash(uint32_t);
static mrp_resource_setref_t *find_in_id_hash(uint32_t);

static field_t field_check(lua_State *, int, const char **);
static field_t field_name_to_type(const char *, size_t);


MRP_LUA_METHOD_LIST_TABLE (
    owners_methods,          /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (owners_create)
);

MRP_LUA_METHOD_LIST_TABLE (
    setref_methods            /* methodlist name */
);

MRP_LUA_METHOD_LIST_TABLE (
    owners_overrides,         /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (owners_create)
    MRP_LUA_OVERRIDE_GETFIELD   (owners_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (owners_setfield)
);

MRP_LUA_METHOD_LIST_TABLE (
    ownerref_overrides,       /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (ownerref_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (ownerref_setfield)
);

MRP_LUA_METHOD_LIST_TABLE (
    setref_overrides,         /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (setref_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (setref_setfield)
);

MRP_LUA_CLASS_DEF (
    resource,                 /* main class name */
    owners,                   /* constructor name */
    mrp_resource_ownersref_t, /* userdata type */
    owners_destroy,           /* userdata destructor */
    owners_methods,           /* class methods */
    owners_overrides          /* class overrides */
);

MRP_LUA_CLASS_DEF (
    resource,                 /* main class name */
    sets,                     /* constructor name */
    mrp_resource_setref_t,    /* userdata type */
    setref_destroy,           /* userdata destructor */
    setref_methods,           /* class methods */
    setref_overrides          /* class overrides */
);

static mrp_resource_ownersref_t *resource_owners[MRP_ZONE_MAX];
static mrp_htbl_t *id_hash;

void mrp_resource_lua_init(lua_State *L)
{
    static bool initialised = false;

    if (!initialised) {
        owners_class_create(L);
        ownerref_class_create(L);
        setref_class_create(L);

        init_id_hash();
    }
}

bool mrp_resource_lua_veto(mrp_zone_t *zone,
                           mrp_resource_set_t *rset,
                           mrp_resource_owner_t *owners,
                           mrp_resource_mask_t grant,
                           mrp_resource_set_t *reqset)
{
    lua_State *L = mrp_lua_get_lua_state();
    mrp_lua_resmethod_t *methods = mrp_lua_get_resource_methods();
    mrp_funcarray_t *veto;
    mrp_resource_setref_t *sref, *rref;
    mrp_resource_ownersref_t *oref;
    mrp_funcbridge_value_t args[16];
    int i, top;
    bool success;

    if (L == NULL)
        return true;

    success = true;
    top = lua_gettop(L);

    if (zone && rset && owners && methods &&
        (sref = find_in_id_hash(rset->id)) &&
        (oref = owners_get(L, zone->id)))
    {
        rref = reqset ? find_in_id_hash(reqset->id) : NULL;
        oref->owners = owners;

        if ((veto = methods->veto)) {
            args[i=0].string  = zone->name;
            args[++i].pointer = sref;
            args[++i].integer = grant;
            args[++i].pointer = oref;
            args[++i].pointer = rref;

            success = mrp_funcarray_call_from_c(L, veto, "sodoo", args);

            goto out;
        }
    }

 out:
    lua_settop(L, top);

    return success;
}

void mrp_resource_lua_set_owners(mrp_zone_t *zone,mrp_resource_owner_t *owners)
{
    lua_State *L = mrp_lua_get_lua_state();
    mrp_resource_ownersref_t *ref;

    if (L && zone && owners && (ref = owners_get(L, zone->id)))
        ref->owners = owners;
}

void mrp_resource_lua_register_resource_set(mrp_resource_set_t *rset)
{
    lua_State *L = mrp_lua_get_lua_state();
    mrp_resource_setref_t *ref;

    MRP_ASSERT(rset, "invalid argument");

    if ((ref = mrp_lua_create_object(L, SETREF_CLASS, NULL,rset->id))) {
        ref->rset = rset;
        add_to_id_hash(ref);
    }
}

void mrp_resource_lua_unregister_resource_set(mrp_resource_set_t *rset)
{
    lua_State *L = mrp_lua_get_lua_state();
    mrp_resource_setref_t *ref;

    MRP_ASSERT(rset, "invalid argument");

    if ((ref = remove_from_id_hash(rset->id))) {
        MRP_ASSERT(rset == ref->rset, "confused with data structures");
        mrp_lua_destroy_object(L, NULL,rset->id, ref);
        ref->rset = NULL;
    }
}

void mrp_resource_lua_add_resource_to_resource_set(mrp_resource_set_t *rset,
                                                   mrp_resource_t *res)
{
    lua_State *L = mrp_lua_get_lua_state();
    mrp_resource_setref_t *ref;
    mrp_resource_def_t *def;

    MRP_ASSERT(rset && res, "invalid argument");

    ref = find_in_id_hash(rset->id);
    def = res->def;

    if (ref && def) {
        MRP_ASSERT(rset == ref->rset, "confused with data structures");

        mrp_lua_push_object(L, ref);

        lua_pushstring(L, def->name);
        mrp_lua_resource_create(L, res);

        lua_rawset(L, -3);
    }
}

static void owners_class_create(lua_State *L)
{
    mrp_lua_create_object_class(L, OWNERS_CLASS);
}

static void ownerref_class_create(lua_State *L)
{
    luaL_newmetatable(L, OWNERREF_USERDATA);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    lua_pop(L, 1);

    luaL_newmetatable(L, OWNERREF_CLASSID);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, ownerref_overrides, 0);
    lua_pop(L, 1);
}

static void setref_class_create(lua_State *L)
{
    mrp_lua_create_object_class(L, SETREF_CLASS);
}

static mrp_resource_ownersref_t *owners_get(lua_State *L, uint32_t zoneid)
{
    mrp_resource_ownersref_t *owner = NULL;
    mrp_zone_t *zone;

    if (zoneid < MRP_ZONE_MAX) {
        if (!(owner = resource_owners[zoneid])) {
            if ((zone = mrp_zone_find_by_id(zoneid))) {
                owner = mrp_lua_create_object(L, OWNERS_CLASS, zone->name,0);
                owner->zoneid = zoneid;
                resource_owners[zoneid] = owner;
            }
        }
    }

    return owner;
}

static int owners_create(lua_State *L)
{
    luaL_error(L, "can't create resource owner from LUA");
    return 0;
}

static int owners_getfield(lua_State *L)
{
    mrp_resource_ownersref_t *ref = owners_check(L, 1);
    uint32_t resid;

    MRP_LUA_ENTER;

    switch (lua_type(L, 2)) {

    case LUA_TSTRING:
        if (mrp_lua_findtable(L, MRP_LUA_GLOBALTABLE, "resource.class", 0)) {
            lua_pushnil(L);
            break;
        }
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        if (lua_isnil(L, -1))
            break;
        resid = mrp_lua_to_resource_id(L, -1);
        goto create_reference;

    case LUA_TNUMBER:
        resid = lua_tointeger(L, 2) - 1;

    create_reference:
        if (resid >= MRP_RESOURCE_MAX || !ref->owners[resid].class)
            lua_pushnil(L);
        else
            ownerref_create(L, ref->zoneid, resid);
        break;

    default:
        lua_pushnil(L);
        break;
    }

    MRP_LUA_LEAVE(1);
}

static int owners_setfield(lua_State *L)
{
    MRP_LUA_ENTER;

    luaL_error(L, "attempt to write read-only resource owners");

    MRP_LUA_LEAVE(0);
}

static void owners_destroy(void *data)
{
    mrp_resource_ownersref_t *owners = (mrp_resource_ownersref_t *)data;

    MRP_LUA_ENTER;

    if (owners->zoneid < MRP_ZONE_MAX)
        resource_owners[owners->zoneid] = NULL;

    memset(owners, 0, sizeof(mrp_resource_ownersref_t));

    MRP_LUA_LEAVE_NOARG;
}

static mrp_resource_ownersref_t *owners_check(lua_State *L, int t)
{
    return (mrp_resource_ownersref_t*)mrp_lua_check_object(L, OWNERS_CLASS, t);
}


static ownerref_t *ownerref_create(lua_State *L,uint32_t zoneid,uint32_t resid)
{
    int table;
    ownerref_t *or;

    lua_createtable(L, 0, 0);
    table = lua_gettop(L);

    luaL_getmetatable(L, OWNERREF_CLASSID);
    lua_setmetatable(L, table);

    lua_pushliteral(L, "userdata");

    or = (ownerref_t *)lua_newuserdata(L, sizeof(ownerref_t));
    memset(or, 0, sizeof(ownerref_t));

    luaL_getmetatable(L, OWNERREF_USERDATA);
    lua_setmetatable(L, -2);

    lua_rawset(L, table);

    or->zoneid = zoneid;
    or->resid = resid;

    return or;
}

static int ownerref_getfield(lua_State *L)
{
    mrp_resource_owner_t *owner = ownerref_check(L, 1);
    const char *name;
    field_t field;

    MRP_LUA_ENTER;

    if (!owner || lua_type(L, 2) != LUA_TSTRING)
        lua_pushnil(L);
    else {
        field = field_check(L, 2, &name);

        switch (field) {
        case APPLICATION_CLASS:  lua_pushstring(L, owner->class->name);  break;
        case RESOURCE_SET:       lua_pushinteger(L, owner->rset->id);    break;
        case RESOURCE:           lua_pushnil(L);                         break;
        default:                 lua_pushnil(L);                         break;
        }
    }

    MRP_LUA_LEAVE(1);
}

static int ownerref_setfield(lua_State *L)
{
    /* mrp_resource_owner_t *owner = ownerref_check(L, 1); */

    MRP_UNUSED(L);

    MRP_LUA_ENTER;

    printf("*** ownerref setfield\n");

    MRP_LUA_LEAVE(1);
}

static mrp_resource_owner_t *ownerref_check(lua_State *L, int t)
{
    ownerref_t *or;
    mrp_resource_ownersref_t *ro;
    mrp_resource_owner_t *owner = NULL;

    t = (t < 0) ? lua_gettop(L) + t + 1 : t;

    luaL_checktype(L, t, LUA_TTABLE);

    lua_pushliteral(L, "userdata");
    lua_rawget(L, t);

    or = luaL_checkudata(L, -1, OWNERREF_USERDATA);
    luaL_argcheck(L, or != NULL, t, "'resource owner' expected");

    lua_pop(L, 1);

    if ((ro = resource_owners[or->zoneid]))
        owner = ro->owners + or->resid;

    return owner;
}

static int setref_getfield(lua_State *L)
{
    mrp_resource_setref_t *ref = setref_check(L, 1);
    mrp_resource_set_t *rset;
    field_t field;
    const char *state;

    MRP_LUA_ENTER;

    if (!ref || !(rset = ref->rset))
        lua_pushnil(L);
    else {
        field = field_check(L, 2, NULL);

        switch (field) {

        case ID:
            lua_pushinteger(L, rset->id);
            break;

        case STATE:
            switch (rset->state) {
            case mrp_resource_no_request:  state = "no_request";  break;
            case mrp_resource_release:     state = "release";     break;
            case mrp_resource_acquire:     state = "acquire";     break;
            default:                       state = "<invalid>";   break;
            }
            lua_pushstring(L, state);
            break;

        case DONT_WAIT:
            lua_pushboolean(L, rset->dont_wait.current);
            break;

        case AUTO_RELEASE:
            lua_pushboolean(L, rset->auto_release.current);
            break;

        case APPLICATION_CLASS:
            lua_pushstring(L, rset->class.ptr->name);
            break;

        default:
            lua_pushnil(L);
            break;
        }
    }

    MRP_LUA_LEAVE(1);
}

static int setref_setfield(lua_State *L)
{
    mrp_resource_setref_t *ref = setref_check(L, 1);
    mrp_resource_set_t *rset;
    field_t field;

    MRP_LUA_ENTER;

    if (ref && (rset = ref->rset)) {
        field = field_check(L, 2, NULL);

        switch (field) {

        case DONT_WAIT:
            rset->dont_wait.current = lua_toboolean(L, 3);
            break;

        case AUTO_RELEASE:
            rset->auto_release.current = lua_toboolean(L, 3);
            break;

        default:
            break;
        }
    }

    MRP_LUA_LEAVE(0);
}

static void setref_destroy(void *data)
{
    mrp_resource_setref_t *ref = (mrp_resource_setref_t *)data;
    mrp_resource_set_t *rset;

    MRP_LUA_ENTER;

    if (ref && (rset = ref->rset))
        remove_from_id_hash(rset->id);

    MRP_LUA_LEAVE_NOARG;
}

static mrp_resource_setref_t *setref_check(lua_State *L, int idx)
{
    return (mrp_resource_setref_t *)mrp_lua_check_object(L, SETREF_CLASS, idx);
}


static uint32_t ref_hash(const void *key)
{
    return (uint32_t)(key - NULL);
}

static int ref_comp(const void *key1, const void *key2)
{
    uint32_t k1 = key1 - NULL;
    uint32_t k2 = key2 - NULL;

    return (k1 == k2) ? 0 : ((k1 > k2) ? 1 : -1);
}

static void init_id_hash(void)
{
    mrp_htbl_config_t cfg;

    cfg.nentry  = 32;
    cfg.comp    = ref_comp;
    cfg.hash    = ref_hash;
    cfg.free    = NULL;
    cfg.nbucket = cfg.nentry;

    id_hash = mrp_htbl_create(&cfg);

    MRP_ASSERT(id_hash, "failed to make id_hash for resource set refs");
}

static int add_to_id_hash(mrp_resource_setref_t *ref)
{
    mrp_resource_set_t *rset;

    MRP_ASSERT(ref, "invalid argument");

    rset = ref->rset;

    MRP_ASSERT(rset, "confused with data structures");

    if (!mrp_htbl_insert(id_hash, NULL + rset->id, ref))
        return -1;

    return 0;
}

static mrp_resource_setref_t *remove_from_id_hash(uint32_t id)
{
    return id_hash ? mrp_htbl_remove(id_hash, NULL + id, false) : NULL;
}

static mrp_resource_setref_t *find_in_id_hash(uint32_t id)
{
    return id_hash ? mrp_htbl_lookup(id_hash, NULL + id) : NULL;
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

    case 5:
        if (!strcmp(name, "state"))
            return STATE;
        break;

    case 8:
        if (!strcmp(name, "resource"))
            return RESOURCE;
        break;

    case 9:
        if (!strcmp(name, "dont_wait"))
            return DONT_WAIT;
        break;

    case 10:
        if (!strcmp(name, "attributes"))
            return ATTRIBUTES;
        break;

    case 12:
        if (!strcmp(name, "auto_release"))
            return AUTO_RELEASE;
        if (!strcmp(name, "resource_set"))
            return RESOURCE_SET;
        break;

    case 17:
        if (!strcmp(name, "application_class"))
            return APPLICATION_CLASS;
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
