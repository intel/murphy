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

#include <murphy/core/lua-utils/object.h>

typedef struct userdata_s userdata_t;

struct userdata_s {
    userdata_t *self;
    mrp_lua_classdef_t *def;
    int  luatbl;
    int  refcnt;
    bool dead;
    char data[];
};

static bool valid_id(const char *);
static int  userdata_destructor(lua_State *);

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
                            const char         *name)
{
    int class = 0;
    size_t size;
    userdata_t *userdata;

    if (name) {
        if (!valid_id(name))
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

    return (void *)userdata->data;
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

void mrp_lua_destroy_object(lua_State *L, const char *name, void *data)
{
    static int offset = ((userdata_t *)0)->data - (char *)0;

    userdata_t *userdata = (userdata_t *)(data - offset);
    mrp_lua_classdef_t *def;

    if (data && userdata == userdata->self && userdata->dead) {
        def = userdata->def;

        luaL_unref(L, LUA_REGISTRYINDEX, userdata->luatbl);

        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_pushstring(L, name);
        lua_pushnil(L);

        lua_rawset(L, -3);

        lua_pop(L, 1);
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

    return userdata ? (void *)userdata->data : NULL;
}

void *mrp_lua_to_object(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    userdata_t *userdata;

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_pushvalue(L, idx);
    lua_pushliteral(L, "userdata");
    lua_rawget(L, -2);

    userdata = (userdata_t *)lua_touserdata(L, -1);

    if (!userdata || !lua_getmetatable(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, def->userdata_id);

    if (!lua_rawequal(L, -1, -2) || userdata != userdata->self)
        userdata = NULL;

    lua_pop(L, 3);

    return userdata ? (void *)userdata->data : NULL;
}



int mrp_lua_push_object(lua_State *L, void *data)
{
    static int offset = ((userdata_t *)0)->data - (char *)0;

    userdata_t *userdata = (userdata_t *)(data - offset);

    if (!data || userdata != userdata->self || userdata->dead)
        lua_pushnil(L);
    else
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->luatbl);

    return 1;
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
            def->destructor((void *)userdata->data);
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
