/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/json.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-bindings/lua-json.h>

#define JSON_LUA_CLASS MRP_LUA_CLASS(json, lua)

/*
 * Lua JSON object
 */

typedef struct {
    mrp_json_t *json;
} json_lua_t;


/*
 * Lua JSON object methods
 */

static void json_lua_destroy(void *data);
static int  json_lua_getfield(lua_State *L);
static int  json_lua_setfield(lua_State *L);
static int  json_lua_stringify(lua_State *L);
static json_lua_t *json_lua_get(lua_State *L, int idx);
static mrp_json_t *json_lua_table_to_object(lua_State *L, int t);


/*
 * JSON Lua class
 */

MRP_LUA_METHOD_LIST_TABLE(json_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(mrp_json_lua_create));

MRP_LUA_METHOD_LIST_TABLE(json_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (mrp_json_lua_create)
                          MRP_LUA_OVERRIDE_GETFIELD (json_lua_getfield)
                          MRP_LUA_OVERRIDE_SETFIELD (json_lua_setfield)
                          MRP_LUA_OVERRIDE_STRINGIFY(json_lua_stringify));

MRP_LUA_CLASS_DEF_FLAGS(json, lua, json_lua_t,
                        json_lua_destroy, json_lua_methods, json_lua_overrides,
                        MRP_LUA_CLASS_DYNAMIC);


int mrp_json_lua_create(lua_State *L)
{
    json_lua_t *lson;
    mrp_json_t *json;
    int         t;

    switch (lua_gettop(L)) {
    noargs:
    case 0:
        lson = (json_lua_t *)mrp_lua_create_object(L, JSON_LUA_CLASS, NULL, 0);
        lson->json = mrp_json_create(MRP_JSON_OBJECT);
        break;

        /*
         * assume to be called as m:JSON(<initializer table>)
         *
         * Notes: We should check the argument to see if it happens
         *     to be of type murphy to catch calls of the form m.JSON()
         *     and redirect them to case 0.
         */
    case 1:
        if (lua_type(L, 1) == LUA_TUSERDATA)
            goto noargs;
        t = 1;
    init:
        if (lua_type(L, t) != LUA_TTABLE)
            return luaL_error(L, "invalid argument to JSON constructor");
        json = json_lua_table_to_object(L, t);
        lson = (json_lua_t *)mrp_lua_create_object(L, JSON_LUA_CLASS, NULL, 0);
        lson->json = json;
        break;

        /*
         * assume to be called as m:JSON(<initializer table>), ie.
         * m.JSON(m, <initializer table>)
         *
         * Notes: We should check the first argument and make sure it
         *     is of type murphy.
         */
    case 2:
        t = 2;
        goto init;

    default:
        return luaL_error(L, "invalid arguments to JSON constructor (%d)",
                          lua_gettop(L));
    }

    return 1;
}


void *mrp_json_lua_wrap(lua_State *L, mrp_json_t *json)
{
    json_lua_t *lson = mrp_lua_create_object(L, JSON_LUA_CLASS, NULL, 0);

    lson->json = mrp_json_ref(json);

    return lson;
}


int mrp_json_lua_push(lua_State *L, mrp_json_t *json)
{
    json_lua_t *lson;

    if ((lson = mrp_json_lua_wrap(L, json)) == NULL)
        lua_pushnil(L);

    return 1;
}


mrp_json_t *mrp_json_lua_get(lua_State *L, int idx)
{
    json_lua_t *lson = json_lua_get(L, idx);

    if (lson != NULL)
        return mrp_json_ref(lson->json);
    else
        return NULL;
}


mrp_json_t *mrp_json_lua_unwrap(void *lson)
{
    if (mrp_lua_pointer_of_type(lson, MRP_LUA_TYPE_ID(JSON_LUA_CLASS)))
        return mrp_json_ref(((json_lua_t *)lson)->json);
    else
        return NULL;
}


static void json_lua_destroy(void *data)
{
    json_lua_t *lson = (json_lua_t *)data;

    mrp_debug("destroying Lua JSON object %p (%p)", lson, lson->json);

    mrp_json_unref(lson->json);
}


static inline json_lua_t *json_lua_check(lua_State *L, int idx)
{
    return (json_lua_t *)mrp_lua_check_object(L, JSON_LUA_CLASS, idx);
}


static json_lua_t *json_lua_get(lua_State *L, int idx)
{
    json_lua_t *lson;
    void       *userdata;

    lua_pushvalue(L, idx);
    lua_pushliteral(L, "userdata");

    lua_rawget(L, -2);
    userdata = lua_touserdata(L, -1);
    lua_pop(L, 2);

    if (userdata != NULL)
        if ((lson = json_lua_check(L, idx)) != NULL)
            return lson;

    return NULL;
}


static int json_lua_getfield(lua_State *L)
{
    json_lua_t *lson = json_lua_check(L, 1);
    const char *key;
    int         idx;
    mrp_json_t *val;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        key = lua_tostring(L, 2);
        val = mrp_json_get(lson->json, key);
        break;

    case LUA_TNUMBER:
        if (mrp_json_get_type(lson->json) != MRP_JSON_ARRAY)
            return luaL_error(L, "trying to index non-array JSON object");

        idx = lua_tointeger(L, 2);
        val = mrp_json_array_get(lson->json, idx - 1);
        break;

    default:
        return luaL_error(L, "invalid JSON field/index type (%s).",
                          lua_typename(L, lua_type(L, 2)));
    }

    lua_pop(L, 2);

    switch (mrp_json_get_type(val)) {
    case MRP_JSON_STRING:
        lua_pushstring(L, mrp_json_string_value(val));
        break;

    case MRP_JSON_BOOLEAN:
        lua_pushboolean(L, mrp_json_boolean_value(val));
        break;

    case MRP_JSON_INTEGER:
        lua_pushinteger(L, mrp_json_integer_value(val));
        break;

    case MRP_JSON_DOUBLE:
        lua_pushnumber(L, mrp_json_double_value(val));
        break;

    case MRP_JSON_OBJECT:
    case MRP_JSON_ARRAY:
        mrp_json_lua_push(L, val);
        break;

    default:
        lua_pushnil(L);
    }

    return 1;
}


static mrp_json_t *json_lua_table_to_object(lua_State *L, int t)
{
    json_lua_t *lson;
    mrp_json_t *json;
    const char *key;
    int         idx, i;
    double      dbl;
    mrp_json_t *val;

    if ((lson = json_lua_get(L, t)) != NULL)
        return mrp_json_clone(lson->json);

    json = NULL;
    val  = NULL;

    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
        switch (lua_type(L, -2)) {
        case LUA_TSTRING:
            if (json != NULL && mrp_json_get_type(json) != MRP_JSON_OBJECT)
                luaL_error(L, "trying to set member on a JSON array");

            key = lua_tostring(L, -2);
            idx = 0;
            break;

        case LUA_TNUMBER:
            if (json != NULL && mrp_json_get_type(json) != MRP_JSON_ARRAY)
                luaL_error(L, "trying to set array element on a JSON object");

            if ((idx = lua_tointeger(L, -2)) < 1)
                luaL_error(L, "invalid index (%d) for JSON array", idx);

            idx--;
            key = NULL;
            break;

        default:
            luaL_error(L, "invalid member (key) for JSON object");
            idx = 0;
            key = NULL;
        }

        switch (lua_type(L, -1)) {
        case LUA_TSTRING:
            val = mrp_json_create(MRP_JSON_STRING, lua_tostring(L, -1), -1);
            break;

        case LUA_TNUMBER:
            if ((i = lua_tointeger(L, -1)) == (dbl = lua_tonumber(L, -1)))
                val = mrp_json_create(MRP_JSON_INTEGER, i);
            else
                val = mrp_json_create(MRP_JSON_DOUBLE, dbl);
            break;

        case LUA_TBOOLEAN:
            val = mrp_json_create(MRP_JSON_BOOLEAN, lua_toboolean(L, -1));
            break;

        case LUA_TTABLE:
            val = json_lua_table_to_object(L, lua_gettop(L));
            break;

        case LUA_TNIL:
            goto next;

        default:
            luaL_error(L, "invalid value for JSON member");
        }

        if (val == NULL) {
            mrp_json_unref(json);
            luaL_error(L, "failed convert Lua value to JSON object");
        }

        if (json == NULL) {
            json = mrp_json_create(key ? MRP_JSON_OBJECT : MRP_JSON_ARRAY);

            if (json == NULL)
                luaL_error(L, "failed to create JSON object");
        }

        if (key != NULL)
            mrp_json_add(json, key, val);
        else
            if (!mrp_json_array_append(json, val))
                luaL_error(L, "failed to set JSON array element [%d]", idx);

    next:
        lua_pop(L, 1);
    }

    return json;
}


static int json_lua_setfield(lua_State *L)
{
    json_lua_t *lson = json_lua_check(L, 1);
    mrp_json_t *json = lson->json;
    const char *key;
    int         idx, i;
    double      dbl;
    mrp_json_t *val;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        if (mrp_json_get_type(json) != MRP_JSON_OBJECT)
            return luaL_error(L, "trying to set member on a JSON array");

        key = lua_tostring(L, 2);
        idx = 0;
        break;

    case LUA_TNUMBER:
        if (mrp_json_get_type(json) != MRP_JSON_ARRAY)
            return luaL_error(L, "trying to set array element on JSON object");

        if ((idx = lua_tointeger(L, 2)) < 1)
            return luaL_error(L, "invalid index (%d) for JSON array", idx);

        idx--;
        key = NULL;
        break;

    default:
        return luaL_error(L, "invalid member (key) for JSON object");
    }

    switch (lua_type(L, 3)) {
    case LUA_TSTRING:
        val = mrp_json_create(MRP_JSON_STRING, lua_tostring(L, 3), -1);
        break;

    case LUA_TNUMBER:
        if ((i = lua_tointeger(L, 3)) == (dbl = lua_tonumber(L, 3)))
            val = mrp_json_create(MRP_JSON_INTEGER, i);
        else
            val = mrp_json_create(MRP_JSON_DOUBLE, dbl);
        break;

    case LUA_TBOOLEAN:
        val = mrp_json_create(MRP_JSON_BOOLEAN, lua_toboolean(L, 3));
        break;

    case LUA_TTABLE:
        if ((val = json_lua_table_to_object(L, 3)) == json)
            return luaL_error(L, "can't set JSON object as a member of itself");
        break;

    case LUA_TNIL:
        if (key != NULL)
            mrp_json_del_member(json, key);
        else
            return luaL_error(L, "can't delete JSON array element by "
                              "setting to nil");
        goto out;

    default:
        return luaL_error(L, "invalid value for JSON member");
    }

    if (val == NULL)
        return luaL_error(L, "failed convert Lua value to JSON object");

    if (key != NULL)
        mrp_json_add(json, key, val);
    else
        if (!mrp_json_array_set(json, idx, val))
            return luaL_error(L, "failed to set set JSON array element [%d]",
                              idx);

 out:
    lua_pop(L, 3);

    return 0;
}


static int json_lua_stringify(lua_State *L)
{
    json_lua_t *lson = json_lua_check(L, 1);

    lua_pushstring(L, mrp_json_object_to_string(lson->json));

    return 1;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, JSON_LUA_CLASS,
                             { "JSON", mrp_json_lua_create });
