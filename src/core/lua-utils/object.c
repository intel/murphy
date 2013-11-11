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
#include <stdbool.h>
#include <ctype.h>


#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/core/lua-utils/object.h>

typedef struct userdata_s userdata_t;

struct userdata_s {
    userdata_t *self;
    mrp_lua_classdef_t *def;
    int  luatbl;
    int  refcnt;
    bool dead;
    int  reftbl;
    int  exttbl;
    int  initializing : 1;
};

static bool valid_id(const char *);
static int  userdata_destructor(lua_State *);

static void object_create_reftbl(userdata_t *u, lua_State *L);
static void object_delete_reftbl(userdata_t *u, lua_State *L);
static void object_create_exttbl(userdata_t *u, lua_State *L);
static void object_delete_exttbl(userdata_t *u, lua_State *L);
static void init_members(userdata_t *u);
static int  override_setfield(lua_State *L);
static int  override_getfield(lua_State *L);

void mrp_lua_create_object_class(lua_State *L, mrp_lua_classdef_t *def)
{
    /* make a metatatable for userdata, ie for 'c' part of object instances*/
    luaL_newmetatable(L, def->userdata_id);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    lua_pushcfunction(L, userdata_destructor);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    /* define pre-declared members */
    {
        mrp_lua_class_member_t  *members  = def->members;
        int                      nmember  = def->nmember;
        char                   **natives  = def->natives;
        int                      nnative  = def->nnative;
        mrp_lua_class_notify_t   notify   = def->notify;
        int                      flags    = def->flags;

        def->members  = NULL;
        def->nmember  = 0;
        def->natives = NULL;
        def->nnative = 0;
        def->notify   = NULL;
        def->flags    = 0;

        if (mrp_lua_declare_members(def, flags, members, nmember,
                                    natives, nnative, notify) != 0) {
            luaL_error(L, "failed to create object class '%s'",
                       def->class_name);
        }
    }

    /* make the class table */
    luaL_openlib(L, def->constructor, def->methods, 0);

    /* make a metatable for class, ie. for LUA part of object instances */
    luaL_newmetatable(L, def->class_id);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, def->overrides, 0);
    lua_setmetatable(L, -2);
    
    lua_pop(L, 1);
}

void mrp_lua_get_class_table(lua_State *L, mrp_lua_classdef_t *def)
{
    const char *p;
    char *q;
    char tag[256];

    lua_pushvalue(L, LUA_GLOBALSINDEX);

    for (p = def->constructor, q = tag; *p;  p++) {
        if ((*q++ = *p) == '.') {
            q[-1] = '\0';
            lua_getfield(L, -1, tag);
            if (lua_type(L, -1) != LUA_TTABLE) {
                lua_pop(L, 2);
                lua_pushnil(L);
                return;
            }
            lua_remove(L, -2);
            q = tag;
        }            
    } /* for */

    *q = '\0';

    lua_getfield(L, -1, tag);
    lua_remove(L, -2);
}

void *mrp_lua_create_object(lua_State          *L,
                            mrp_lua_classdef_t *def,
                            const char         *name,
                            int                 idx)
{
    int class = 0;
    size_t size;
    userdata_t *userdata;

    if (name || idx) {
        if (name && !valid_id(name))
            return NULL;

        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);
        class = lua_gettop(L);
    }

    lua_createtable(L, 1, 1);

    luaL_openlib(L, NULL, def->methods, 0);

    luaL_getmetatable(L, def->class_id);
    lua_setmetatable(L, -2);

    lua_pushliteral(L, "userdata");

    size = sizeof(userdata_t) + def->userdata_size;
    userdata = (userdata_t *)lua_newuserdata(L, size);

    memset(userdata, 0, size);
    userdata->reftbl = LUA_NOREF;
    userdata->exttbl = LUA_NOREF;

    luaL_getmetatable(L, def->userdata_id);
    lua_setmetatable(L, -2);

    lua_rawset(L, -3);

    lua_pushvalue(L, -1);
    userdata->self   = userdata;
    userdata->def    = def;
    userdata->luatbl = luaL_ref(L, LUA_REGISTRYINDEX);
    userdata->refcnt = 1;

    if (name) {
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_rawset(L, class);
    }

    if (idx) {
        lua_pushvalue(L, -1);
        lua_rawseti(L, class, idx);
    }

    if (class)
        lua_remove(L, class);

    object_create_reftbl(userdata, L);
    if (def->flags & MRP_LUA_CLASS_EXTENSIBLE)
        object_create_exttbl(userdata, L);

    init_members(userdata);

    return (void *)(userdata + 1);
}


void mrp_lua_set_object_name(lua_State          *L,
                             mrp_lua_classdef_t *def,
                             const char         *name)
{
    if (valid_id(name)) {
        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_pushstring(L, name);
        lua_pushvalue(L, -3);

        lua_rawset(L, -3);
        lua_pop(L, 1);
    }
}

void mrp_lua_set_object_index(lua_State          *L,
                              mrp_lua_classdef_t *def,
                              int                 idx)
{
    mrp_lua_get_class_table(L, def);
    luaL_checktype(L, -1, LUA_TTABLE);

    lua_pushvalue(L, -2);

    lua_rawseti(L, -2, idx);

    lua_pop(L, 1);
}

void mrp_lua_destroy_object(lua_State *L, const char *name,int idx, void *data)
{
    userdata_t *userdata = (userdata_t *)data - 1;
    mrp_lua_classdef_t *def;

    if (data && userdata == userdata->self && !userdata->dead) {
        userdata->dead = true;
        def = userdata->def;

        object_delete_reftbl(userdata, L);
        object_delete_exttbl(userdata, L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->luatbl);
        lua_pushstring(L, "userdata");
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, -1);

        luaL_unref(L, LUA_REGISTRYINDEX, userdata->luatbl);

        if (name || idx) {
            mrp_lua_get_class_table(L, def);
            luaL_checktype(L, -1, LUA_TTABLE);

            if (name) {
                lua_pushstring(L, name);
                lua_pushnil(L);
                lua_rawset(L, -3);
            }

            if (idx) {
                lua_pushnil(L);
                lua_rawseti(L, -2, idx);
            }

            lua_pop(L, 1);
        }

    }
}

int mrp_lua_find_object(lua_State *L, mrp_lua_classdef_t *def,const char *name)
{
    if (!name)
        lua_pushnil(L);
    else {
        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_pushstring(L, name);
        lua_rawget(L, -2);

        lua_remove(L, -2);
    }

    return 1;
}


void *mrp_lua_check_object(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    userdata_t *userdata;
    char errmsg[256];

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_pushvalue(L, idx);
    lua_pushliteral(L, "userdata");
    lua_rawget(L, -2);

    if (!def)
        userdata = (userdata_t *)lua_touserdata(L, -1);
    else {
        userdata = (userdata_t *)luaL_checkudata(L, -1, def->userdata_id);

        if (!userdata || def != userdata->def) {
            snprintf(errmsg, sizeof(errmsg), "'%s' expected", def->class_name);
            luaL_argerror(L, idx, errmsg);
            userdata = NULL;
        }
    }

    if (userdata != userdata->self) {
        luaL_error(L, "invalid userdata");
        userdata = NULL;
    }

    lua_pop(L, 2);

    return userdata ? (void *)(userdata + 1) : NULL;
}

void *mrp_lua_to_object(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    userdata_t *userdata;
    int top = lua_gettop(L);

    idx = (idx < 0) ? lua_gettop(L) + idx + 1 : idx;

    if (!lua_istable(L, idx))
        return NULL;

    lua_pushliteral(L, "userdata");
    lua_rawget(L, idx);

    userdata = (userdata_t *)lua_touserdata(L, -1);

    if (!userdata || !lua_getmetatable(L, -1)) {
        lua_settop(L, top);
        return NULL;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, def->userdata_id);

    if (!lua_rawequal(L, -1, -2) || userdata != userdata->self)
        userdata = NULL;

    lua_settop(L, top);

    return userdata ? (void *)(userdata + 1) : NULL;
}



int mrp_lua_push_object(lua_State *L, void *data)
{
    userdata_t *userdata = (userdata_t *)data - 1;

    if (!data || userdata != userdata->self || userdata->dead)
        lua_pushnil(L);
    else
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->luatbl);

    return 1;
}

mrp_lua_classdef_t *mrp_lua_get_object_classdef(void *data)
{
    userdata_t *userdata = (userdata_t *)data - 1;
    mrp_lua_classdef_t *def;

    if (!data || userdata != userdata->self || userdata->dead)
        def = NULL;
    else
        def = userdata->def;

    return def;
}


static bool valid_id(const char *id)
{
    const char *p;
    char c;

    if (!(p = id) || !isalpha(*p))
        return false;

    while ((c = *p++)) {
        if (!isalnum(c) && (c != '_'))
            return false;
    }

    return true;
}

static int userdata_destructor(lua_State *L)
{
    userdata_t *userdata;
    mrp_lua_classdef_t *def;

    if (!(userdata = lua_touserdata(L, -1)) || !lua_getmetatable(L, -1))
        luaL_error(L, "attempt to destroy unknown type of userdata");
    else {
        def = userdata->def;
        lua_getfield(L, LUA_REGISTRYINDEX, def->userdata_id);
        if (!lua_rawequal(L, -1, -2))
            luaL_typerror(L, -2, def->userdata_id);
        else
            def->destructor((void *)(userdata + 1));
    }

    return 0;
}


static int default_setter(void *data, lua_State *L, int member,
                          mrp_lua_value_t *v)
{
    userdata_t             *u = (userdata_t *)data - 1;
    mrp_lua_class_member_t *m;
    mrp_lua_value_t        *vptr;

    m    = u->def->members + member;
    vptr = data + m->offs;

    if (L == NULL) {
        switch (m->type) {
        case MRP_LUA_STRING:
            vptr->str = NULL;
            goto ok;
        case MRP_LUA_LFUNC:
        case MRP_LUA_CFUNC:
        case MRP_LUA_ANY:
            vptr->any = LUA_NOREF;
            goto ok;
        default:
            goto error;
        }
    }

    switch (m->type) {
    case MRP_LUA_STRING:
        mrp_free((void *)vptr->str);
        vptr->str = v->str ? mrp_strdup(v->str) : NULL;

        if (vptr->str == NULL && v->str != NULL)
            goto error;
        else
            goto ok;

    case MRP_LUA_BOOLEAN:
        vptr->bln = v->bln;
        goto ok;

    case MRP_LUA_INTEGER:
        vptr->s32 = v->s32;
        goto ok;

    case MRP_LUA_DOUBLE:
        vptr->dbl = v->dbl;
        goto ok;

    case MRP_LUA_LFUNC:
        mrp_lua_object_unref_value(data, L, vptr->lfn);
        vptr->lfn = v->lfn;
        goto ok;

    case MRP_LUA_CFUNC:
        goto error;

    case MRP_LUA_ANY:
        mrp_lua_object_unref_value(data, L, vptr->any);
        vptr->any = v->any;
        goto ok;

    default:
        goto error;
    }

 ok:
    return 1;

 error:
    return -1;
}


static int default_getter(void *data, lua_State *L, int member,
                          mrp_lua_value_t *v)
{
    userdata_t             *u = (userdata_t *)data - 1;
    mrp_lua_class_member_t *m;
    mrp_lua_value_t        *vptr;

    MRP_UNUSED(L);

    m    = u->def->members + member;
    vptr = data + m->offs;

    switch (m->type) {
    case MRP_LUA_STRING:
        v->str = vptr->str;
        goto ok;

    case MRP_LUA_BOOLEAN:
        v->bln = vptr->bln;
        goto ok;

    case MRP_LUA_INTEGER:
        v->s32 = vptr->s32;
        goto ok;

    case MRP_LUA_DOUBLE:
        v->dbl = vptr->dbl;
        goto ok;

    case MRP_LUA_LFUNC:
        v->lfn = vptr->lfn;
        goto ok;

    case MRP_LUA_CFUNC:
        goto error;

    case MRP_LUA_ANY:
        v->any = vptr->any;
        goto ok;
    }

 ok:
    return 1;

 error:
    return -1;
}


static int patch_overrides(mrp_lua_classdef_t *def)
{
    luaL_reg set = { NULL, NULL }, get = { NULL, NULL }, *r, *overrides;
    int      i, n, extra;

    for (n = 0, r = def->overrides; r->name != NULL; r++, n++) {
        if (!strcmp(r->name, "__newindex")) {
            if (set.name != NULL) {
                mrp_log_error("Class with multiple SETFIELD overrides.");
                exit(1);
            }

            if (set.func == override_setfield) {
                mrp_log_error("SETFIELD already overridden to setfield!");
                exit(1);
            }

            set = *r;
            r->func = override_setfield;
            continue;
        }

        if (!strcmp(r->name, "__index")) {
            if (get.name != NULL) {
                mrp_log_info("Class with multiple GETFIELD overrides.");
                exit(1);
            }

            if (get.func == override_getfield) {
                mrp_log_error("GETFIELD already overridden to getfield!");
                exit(1);
            }

            get = *r;
            r->func = override_getfield;
            continue;
        }
    }

    if (set.func && get.func) {
        def->setfield = set.func;
        def->getfield = get.func;

        return 0;
    }

    extra = (set.func ? 0 : 1) + (get.func ? 0 : 1);

    /* XXX TODO: currently this is leaked if/when a classdef is destroyed */
    if ((overrides = mrp_allocz_array(typeof(*overrides), n+1 + extra)) == NULL)
        return -1;

    for (i = 0, r = def->overrides; r->name != NULL; i++, r++) {
        overrides[i].name = r->name;
        overrides[i].func = r->func;
    }

    if (set.func == NULL) {
        overrides[i].name = "__newindex";
        overrides[i].func = override_setfield;
        i++;
    }

    if (get.func == NULL) {
        overrides[i].name = "__index";
        overrides[i].func = override_getfield;
        i++;
    }

    def->overrides = overrides;

    return 0;
}


int mrp_lua_declare_members(mrp_lua_classdef_t *def, mrp_lua_class_flag_t flags,
                            mrp_lua_class_member_t *members, int nmember,
                            char **natives, int nnative,
                            mrp_lua_class_notify_t notify)
{
    mrp_lua_class_member_t *m;
    int                     i;

    def->flags = flags;

    if (members == NULL || nmember <= 0) {
        if (def->flags & MRP_LUA_CLASS_EXTENSIBLE)
            goto update_overrides;
        else
            return 0;
    }

    def->members = mrp_allocz_array(typeof(*def->members), nmember);

    if (def->members == NULL)
        return -1;

    for (i = 0, m = def->members; i < nmember; i++, m++) {
        if (members[i].flags & MRP_LUA_CLASS_NOTIFY) {
            if (notify == NULL) {
                mrp_log_error("member '%s' needs a non-NULL notifier",
                              members[i].name);
                goto fail;
            }
        }

        if ((m->name = mrp_strdup(members[i].name)) == NULL)
            goto fail;

        m->type   = members[i].type;
        m->offs   = members[i].offs;
        m->setter = members[i].setter ? members[i].setter : default_setter;
        m->getter = members[i].getter ? members[i].getter : default_getter;
        m->flags  = members[i].flags | (flags & MRP_LUA_CLASS_READONLY);

        def->nmember++;
    }

    def->flags  = flags;
    def->notify = notify;

    if (natives == NULL || nnative == 0)
        goto update_overrides;

    def->natives = mrp_allocz_array(typeof(*def->natives), nnative);

    if (def->natives == NULL)
        goto fail;

    for (i = 0; i < nnative; i++) {
        if ((def->natives[i] = mrp_strdup(natives[i])) == NULL)
            goto fail;

        def->nnative++;
    }

 update_overrides:
    if (!(def->flags & MRP_LUA_CLASS_NOOVERRIDE))
        patch_overrides(def);

    return 0;

 fail:
    for (i = 0, m = def->members; i < def->nmember; i++, m++)
        mrp_free(m->name);
    mrp_free(def->members);

    def->members = NULL;
    def->nmember = 0;

    for (i = 0; i < def->nnative; i++)
        mrp_free(def->natives[i]);
    mrp_free(def->natives);

    def->natives = NULL;
    def->nnative = 0;

    return -1;
}


static void init_members(userdata_t *u)
{
    void                   *data = (void *)(u + 1);
    mrp_lua_class_member_t *members = u->def->members;
    int                     nmember = u->def->nmember;
    mrp_lua_class_member_t *m;
    int                     i;

    u->initializing = true;
    for (i = 0, m = members; i < nmember; i++, m++) {
        mrp_debug("initializing %s.%s of Lua object %p(%p)", u->def->class_name,
                  m->name, data, u);
        m->setter(data, NULL, i, NULL);

    }
    u->initializing = false;
}


static int class_member(userdata_t *u, lua_State *L, int index)
{
    mrp_lua_class_member_t *members = u->def->members;
    int                     nmember = u->def->nmember;
    mrp_lua_class_member_t *m;
    int                     i;
    const char              *name;

    if (lua_type(L, index) != LUA_TSTRING)
        return -1;

    name = lua_tostring(L, index);

    for (i = 0, m = members; i < nmember; i++, m++)
        if (!strcmp(m->name, name))
            return i;

    return -1;
}


static int seterr(char *err, size_t esize, const char *format, ...)
{
    va_list ap;

    if (err != NULL) {
        va_start(ap, format);
        vsnprintf(err, esize, format, ap);
        va_end(ap);
    }

    return -1;
}


static void object_create_reftbl(userdata_t *u, lua_State *L)
{
    lua_newtable(L);
    u->reftbl = luaL_ref(L, LUA_REGISTRYINDEX);
}


static void object_delete_reftbl(userdata_t *u, lua_State *L)
{
    int tidx;

    if (u->reftbl == LUA_NOREF)
        return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->reftbl);
    tidx = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, tidx) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, u->reftbl);
    lua_pop(L, 1);
    u->reftbl = LUA_NOREF;
}


int mrp_lua_object_ref_value(void *data, lua_State *L, int idx)
{
    userdata_t *u = (userdata_t *)data - 1;
    int         ref;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->reftbl);
    lua_pushvalue(L, idx > 0 ? idx : idx - 1);
    ref = luaL_ref(L, -2);
    lua_pop(L, 1);

    return ref;
}


void mrp_lua_object_unref_value(void *data, lua_State *L, int ref)
{
    userdata_t *u = (userdata_t *)data - 1;

    if (ref != LUA_NOREF && ref != LUA_REFNIL) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, u->reftbl);
        luaL_unref(L, -1, ref);
        lua_pop(L, 1);
    }
}


int mrp_lua_object_deref_value(void *data, lua_State *L, int ref, int pushnil)
{
    userdata_t *u = (userdata_t *)data - 1;

    if (ref != LUA_NOREF) {
        if (ref != LUA_REFNIL) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, u->reftbl);
            lua_rawgeti(L, -1, ref);
            lua_remove(L, -2);
        }
        else
        nilref:
            lua_pushnil(L);

        return 1;
    }
    else {
        if (pushnil)
            goto nilref;
        else
            return 0;
    }
}


static void object_create_exttbl(userdata_t *u, lua_State *L)
{
    lua_newtable(L);
    u->exttbl = luaL_ref(L, LUA_REGISTRYINDEX);
}


static void object_delete_exttbl(userdata_t *u, lua_State *L)
{
    int tidx;

    if (u->exttbl == LUA_NOREF)
        return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->exttbl);
    tidx = lua_gettop(L);

    lua_pushnil(L);
    while (lua_next(L, tidx) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, u->exttbl);
    lua_pop(L, 1);
    u->exttbl = LUA_NOREF;
}


int mrp_lua_object_setext(void *data, lua_State *L, const char *name,
                          int vidx, char *err, size_t esize)
{
    userdata_t *u = (userdata_t *)data - 1;

    if (u->exttbl == LUA_NOREF) {
        if (err)
            return seterr(err, esize, "trying to set user-defined field %s "
                          "for non-extensible object %s", name,
                          u->def->class_name);
        else
            return luaL_error(L, "trying to set user-defined field %s "
                              "for non-extensible object %s", name,
                              u->def->class_name);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->exttbl);
    lua_pushvalue(L, vidx > 0 ? vidx : vidx - 1);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);

    return 1;
}


void mrp_lua_object_getext(void *data, lua_State *L, const char *name)
{
    userdata_t *u = (userdata_t *)data - 1;

   if (u->exttbl == LUA_NOREF) {
       lua_pushnil(L);
       return;
   }

   lua_rawgeti(L, LUA_REGISTRYINDEX, u->exttbl);
   lua_getfield(L, -1, name);
   lua_remove(L, -2);
}


void mrp_lua_object_setiext(void *data, lua_State *L, int idx, int val)
{
    userdata_t *u = (userdata_t *)data - 1;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->exttbl);
    lua_pushvalue(L, val > 0 ? val : val - 1);
    lua_rawseti(L, -2, idx);
    lua_pop(L, 1);
}


void mrp_lua_object_getiext(void *data, lua_State *L, int idx)
{
    userdata_t *u = (userdata_t *)data - 1;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->exttbl);
    lua_rawgeti(L, -1, idx);
    lua_remove(L, -2);
}


int mrp_lua_set_member(void *data, lua_State *L, char *err, size_t esize)
{
    userdata_t             *u = (userdata_t *)data - 1;
    int                     midx = class_member(u, L, -2);
    mrp_lua_class_member_t *m;
    mrp_lua_value_t         v;
    int                     vtype;

    if (midx < 0)
        goto notfound;

    m     = u->def->members + midx;
    vtype = lua_type(L, -1);

    mrp_debug("setting %s.%s of Lua object %p(%p)", u->def->class_name,
              m->name, data, u);

    if (m->flags & MRP_LUA_CLASS_READONLY && !u->initializing)
        return seterr(err, esize, "%s.%s of Lua object is readonly",
                      u->def->class_name, m->name);

    switch (m->type) {
    case MRP_LUA_STRING:
        if (vtype != LUA_TSTRING && vtype != LUA_TNIL)
            return seterr(err, esize, "%s.%s expects string or nil, got %s",
                          u->def->class_name, m->name,
                          lua_typename(L, vtype), m->name);

        v.str = lua_tostring(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_BOOLEAN:
        v.bln = lua_toboolean(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_INTEGER:
        if (vtype != LUA_TNUMBER)
            return seterr(err, esize, "%s.%s expects number, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

        v.s32 = lua_tointeger(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_DOUBLE:
        if (vtype != LUA_TNUMBER)
            return seterr(err, esize, "%s.%s expects number, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

        v.dbl = lua_tonumber(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_LFUNC:
        if (vtype != LUA_TFUNCTION && vtype != LUA_TNIL)
            return seterr(err, esize, "%s.%s expects function, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

        v.lfn = mrp_lua_object_ref_value(data, L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_CFUNC:
        seterr(err, esize, "CFUNC is not implemented");
        goto error;

    case MRP_LUA_ANY:
        v.any = mrp_lua_object_ref_value(data, L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    default:
        seterr(err, esize, "type %d not implemented");
        break;
    }

 ok:
    if ((m->flags & MRP_LUA_CLASS_NOTIFY) && u->def->notify)
        u->def->notify(data, L, midx);
    return 1;

 notfound:
    return 0;

 error:
    return -1;
}


int mrp_lua_get_member(void *data, lua_State *L, char *err, size_t esize)
{
    userdata_t             *u = (userdata_t *)data - 1;
    int                     midx = class_member(u, L, -1);
    mrp_lua_class_member_t *m;
    mrp_lua_value_t         v;

    if (midx < 0)
        goto notfound;

    m = u->def->members + midx;

    if (m->getter(data, L, midx, &v) != 1)
        goto error;

    switch (m->type) {
    case MRP_LUA_STRING:
        if (v.str != NULL)
            lua_pushstring(L, v.str);
        else
            lua_pushnil(L);
        goto ok;

    case MRP_LUA_BOOLEAN:
        lua_pushboolean(L, v.bln);
        goto ok;

    case MRP_LUA_INTEGER:
        lua_pushinteger(L, v.s32);
        goto ok;

    case MRP_LUA_DOUBLE:
        lua_pushnumber(L, v.dbl);
        goto ok;

    case MRP_LUA_LFUNC:
        mrp_lua_object_deref_value(data, L, v.lfn, true);
        goto ok;

    case MRP_LUA_CFUNC:
        seterr(err, esize, "CFUNC is not implemented");
        goto error;

    case MRP_LUA_ANY:
        mrp_lua_object_deref_value(data, L, v.any, true);
        goto ok;
    }

 ok:
    return 1;

 notfound:
    return 0;

 error:
    return -1;
}


int mrp_lua_init_members(void *data, lua_State *L, int idx,
                         char *err, size_t esize)
{
    userdata_t *u = (userdata_t *)data - 1;
    const char *n;
    size_t      l;

    if (idx < 0)
        idx = lua_gettop(L) + idx + 1;

    if (lua_type(L, idx) != LUA_TTABLE)
        return 0;

    u->initializing = true;
    MRP_LUA_FOREACH_FIELD(L, idx, n, l) {
        mrp_debug("initializing %s.%s", u->def->class_name, n);

        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        switch (mrp_lua_set_member(data, L, err, esize)) {
        case -1:
            lua_pop(L, 2 + 1);
            goto error;
        case 0:
            if (u->def->flags & MRP_LUA_CLASS_EXTENSIBLE) {
                if (mrp_lua_object_setext(data, L, n, -1, err, esize) < 0)
                    goto error;
            }
            else {
                seterr(err, esize, "trying toinitialize unknown member %s.%s",
                       u->def->class_name, n);
                lua_pop(L, 2 + 1);
                goto error;
            }
            break;
        case 1:
            break;
        }
    }
    u->initializing = false;
    return 1;

 error:
    u->initializing = false;
    return -1;

}


static inline int is_native(userdata_t *u, const char *name)
{
    int i;

    for (i = 0; i < u->def->nnative; i++)
        if (u->def->natives[i][0] == name[0] &&
            !strcmp(u->def->natives[i] + 1, name + 1))
            return 1;

    return 0;
}


static int override_setfield(lua_State *L)
{
    void       *data = mrp_lua_check_object(L, NULL, 1);
    char        err[128] = "";
    userdata_t *u = (userdata_t *)data - 1;
    const char *name;

    if (data == NULL)
        return luaL_error(L, "failed to find class userdata");

    mrp_debug("setting field for object of type '%s'", u->def->class_name);

    switch (mrp_lua_set_member(data, L, err, sizeof(err))) {
    case 0:  break;
    case 1:  goto out;
    default: return luaL_error(L, "failed to set member (%s)", err);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        name = lua_tostring(L, 2);
        break;
    case LUA_TNUMBER:
        name = NULL;
        break;
    default:
        return luaL_argerror(L, 2, "expecting string or integer");
    }

    luaL_checkany(L, 3);

    if (name != NULL) {
        if (is_native(u, name))
            return u->def->setfield(L);
        else
            mrp_lua_object_setext(data, L, name, 3, NULL, 0);
    }
    else {
        mrp_lua_object_setiext(data, L, lua_tointeger(L, 2), 3);
    }

 out:
    lua_pop(L, 3);

    return 0;
}


static int override_getfield(lua_State *L)
{
    void       *data = mrp_lua_check_object(L, NULL, 1);
    char        err[128] = "";
    userdata_t *u = (userdata_t *)data - 1;
    const char *name;

    mrp_debug("getting field for object of type '%s'", u->def->class_name);

    switch (mrp_lua_get_member(data, L, err, sizeof(err))) {
    case 0:  break;
    case 1:  goto out;
    default: return luaL_error(L, "failed to set member (%s)", err);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        name = lua_tostring(L, 2);
        break;
    case LUA_TNUMBER:
        name = NULL;
        break;
    default:
        return luaL_argerror(L, 2, "expecting string or integer");
    }

    if (name != NULL) {
        if (is_native(u, name))
            return u->def->setfield(L);
        else
            mrp_lua_object_getext(data, L, name);
    }
    else {
        mrp_lua_object_getiext(data, L, 2);
    }

 out:
    lua_remove(L, -2);

    return 1;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
