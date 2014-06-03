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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common.h>

#include <murphy/core/lua-utils/error.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/lua-utils.h>

#define FUNCBRIDGE_METATABLE             "LuaBook.funcbridge"
#define FUNCBRIDGE_USERDATA_METATABLE    "LuaBook.funcbridge.userdata"

#define FUNCARRAY_METATABLE              "LuaBook.funcarray"
#define FUNCARRAY_USERDATA_METATABLE     "LuaBook.funcarray.userdata"

static mrp_funcbridge_t *create_funcbridge(lua_State *, int, int);
static mrp_funcbridge_t *check_funcbridge(lua_State *, int);
static int call_funcbridge_from_lua(lua_State *);
static int get_funcbridge_field(lua_State *);
static int set_funcbridge_field(lua_State *);
static int funcbridge_destructor(lua_State *);

static int call_funcarray_from_lua(lua_State *);
static int get_funcarray_field(lua_State *);
static int set_funcarray_field(lua_State *);
static mrp_funcarray_t *to_funcarray(lua_State *, int);
static int funcarray_destructor(lua_State *);

static int make_lua_call(lua_State *, mrp_funcbridge_t *, int);


void mrp_create_funcbridge_class(lua_State *L)
{
    static const struct luaL_reg class_methods [] = {
        { NULL, NULL }
    };

    static const struct luaL_reg override_methods [] = {
        { "__call"    , call_funcbridge_from_lua },
        { "__index"   , get_funcbridge_field     },
        { "__newindex", set_funcbridge_field     },
        {      NULL   ,     NULL                 }
    };

    luaL_newmetatable(L, FUNCBRIDGE_USERDATA_METATABLE);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    lua_pushcfunction(L, funcbridge_destructor);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, FUNCBRIDGE_METATABLE);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, override_methods, 0);
    lua_pop(L, 1);

    luaL_openlib(L, "builtin.method", class_methods, 0);
    lua_pushvalue(L, -2);
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}


void mrp_create_funcarray_class(lua_State *L)
{
    static const struct luaL_reg override_methods [] = {
        { "__call"    , call_funcarray_from_lua },
        { "__index"   , get_funcarray_field     },
        { "__newindex", set_funcarray_field     },
        {      NULL   ,     NULL                }
    };

    luaL_newmetatable(L, FUNCARRAY_USERDATA_METATABLE);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    lua_pushcfunction(L, funcarray_destructor);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, FUNCARRAY_METATABLE);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, override_methods, 0);
    lua_pop(L, 1);
}


int parse_signature(const char *signature, char **sigp, mrp_lua_type_t **typep)
{
    char            type[256];
    char           *sigs;
    mrp_lua_type_t *types, t;
    int             ntype;
    int             len;
    size_t          l;

    *sigp  = sigs  = NULL;
    *typep = types = NULL;
    ntype  = 0;

    if (signature == NULL)
        return 0;

    if ((len = strlen(signature)) == 0)
        return 0;

    if ((sigs = mrp_allocz(len + 1)) == NULL)
        return -1;

    if ((types = mrp_allocz_array(typeof(*types), len + 1)) == NULL) {
        mrp_free(sigs);
        return -1;
    }

    *sigp  = sigs;
    *typep = types;

    while (*signature) {
        switch (*signature) {
        case MRP_FUNCBRIDGE_STRING:
        case MRP_FUNCBRIDGE_INTEGER:
        case MRP_FUNCBRIDGE_FLOATING:
        case MRP_FUNCBRIDGE_BOOLEAN:
        case MRP_FUNCBRIDGE_POINTER:
        case MRP_FUNCBRIDGE_OBJECT:
            *sigs++ = *signature++;
            break;

        case MRP_FUNCBRIDGE_ARRAY:
            *sigs++ = *signature;

            if (!signature[1] || signature[2] != MRP_FUNCBRIDGE_ARRAY_END) {
                mrp_log_error("Invalid array reference in signature at '%s'.",
                              signature);
                errno = EINVAL;
                goto fail;
            }

            switch (signature[1]) {
            case MRP_FUNCBRIDGE_STRING:
                types[ntype++] = MRP_LUA_STRING_ARRAY;
                break;
            case MRP_FUNCBRIDGE_INTEGER:
                types[ntype++] = MRP_LUA_INTEGER_ARRAY;
                break;
            case MRP_FUNCBRIDGE_FLOATING:
                types[ntype++] = MRP_LUA_DOUBLE_ARRAY;
                break;
            case MRP_FUNCBRIDGE_BOOLEAN:
                types[ntype++] = MRP_LUA_BOOLEAN_ARRAY;
                break;
            case MRP_FUNCBRIDGE_ANY:
                types[ntype++] = MRP_LUA_ANY;
                break;
            default:
                mrp_log_error("Invalid array type in signature at '%s'.",
                              signature);
                errno = EINVAL;
                goto fail;
            }

            signature += 3;
            break;

        case MRP_FUNCBRIDGE_MRPLUATYPE:
            *sigs++ = *signature;

            if (signature[1] != '(' || !signature[2]) {
                mrp_log_error("Invalid object signagure at '%s'.", signature);
                errno = EINVAL;
                goto fail;
            }

            signature += 2;

            l = 0;
            while (*signature && *signature != ')' && l < sizeof(type) - 1) {
                type[l++] = *signature++;
            }

            if (*signature != ')' || l >= sizeof(type) - 1) {
                errno = E2BIG;
                goto fail;
            }
            type[l] = '\0';

            if ((t = mrp_lua_class_type(type)) == MRP_LUA_NONE) {
                if (type[0] == MRP_FUNCBRIDGE_ANY && type[1] == '\0')
                    t = MRP_LUA_ANY;
            }

            if (t == MRP_LUA_NONE) {
                mrp_log_error("Function bridge signature references "
                              "unknown type '%s'.", type);
                errno = EINVAL;
                goto fail;
            }

            types[ntype++] = t;
            signature++;
            break;

        default:
            mrp_log_error("Invalid type in signature at '%s'.", signature);
            errno = EINVAL;
            goto fail;
        }
    }

    types[ntype++] = MRP_LUA_NONE;
    *sigs = '\0';

    mrp_realloc(types, sizeof(*types) * ntype);

    return 0;

 fail:
    mrp_free(*sigp);
    mrp_free(*typep);

    *sigp  = NULL;
    *typep = NULL;

    return -1;
}



mrp_funcbridge_t *mrp_funcbridge_create_cfunc(lua_State *L, const char *name,
                                              const char *signature,
                                              mrp_funcbridge_cfunc_t func,
                                              void *data)
{
    int builtin, top;
    mrp_funcbridge_t *fb;

    top = lua_gettop(L);

    if (mrp_lua_findtable(L, MRP_LUA_GLOBALTABLE, "builtin.method", 20))
        fb = NULL;
    else {
        builtin = lua_gettop(L);

        fb = create_funcbridge(L, 0, 1);

        fb->type = MRP_C_FUNCTION;
        if (parse_signature(signature, &fb->c.signature, &fb->c.sigtypes) < 0) {
            mrp_log_error("Failed to parse signature '%s'.", signature);
            fb->c.signature = mrp_strdup(signature);
        }
        else
            mrp_debug("signature '%s' parsed into '%s'", signature,
                      fb->c.signature);

        fb->c.func = func;
        fb->c.data = data;

        lua_pushstring(L, name);
        lua_pushvalue(L, -2);

        lua_rawset(L, builtin);

        lua_settop(L, top);
    }

    return fb;
}


mrp_funcbridge_t *mrp_funcbridge_create_luafunc(lua_State *L, int f)
{
    mrp_funcbridge_t *fb;

    if (f < 0 && f > LUA_REGISTRYINDEX)
        f = lua_gettop(L) + f + 1;

    switch (lua_type(L, f)) {

    case LUA_TTABLE:
        fb = check_funcbridge(L, f);
        break;

    case LUA_TFUNCTION:
        fb = create_funcbridge(L, 1, 1);
        lua_pushvalue(L, f);
        lua_rawseti(L, -2, 1);
        lua_pop(L, 1);
        fb->type = MRP_LUA_FUNCTION;
        break;

    default:
        luaL_argcheck(L, false, f < 0 ? lua_gettop(L) + f + 1 : f,
                      "'builtin.method.xxx' or lua function expected");
        fb = NULL;
        break;
    }

    return fb;
}


mrp_funcbridge_t *mrp_funcbridge_ref(lua_State *L, mrp_funcbridge_t *fb)
{
    if (fb->dead)
        return NULL;

    MRP_UNUSED(L);

    fb->refcnt++;

    return fb;
}

void mrp_funcbridge_unref(lua_State *L, mrp_funcbridge_t *fb)
{
    if (fb == NULL)
        return;

    if (fb->refcnt > 1)
        fb->refcnt--;
    else {
        free((void *)fb->c.signature);
        fb->c.signature = NULL;

        if (fb->luatbl) {
            luaL_unref(L, LUA_REGISTRYINDEX, fb->luatbl);
            fb->luatbl = 0;
        }
    }
}

bool mrp_funcbridge_call_from_c(lua_State *L,
                                mrp_funcbridge_t *fb,
                                const char *signature,
                                mrp_funcbridge_value_t *args,
                                char *ret_type,
                                mrp_funcbridge_value_t *ret_value)
{
    char t;
    int i;
    int sp;
    mrp_funcbridge_value_t *a;
    int sts;
    bool success;

    if (!fb)
        success = false;
    else {
        mrp_lua_checkstack(L, -1);

        switch (fb->type) {

        case MRP_C_FUNCTION:
            if (!strcmp(signature, fb->c.signature))
                success = fb->c.func(L, fb->c.data, signature, args, ret_type,
                                     ret_value);
            else {
                char errmsg[256];

                snprintf(errmsg, sizeof(errmsg),
                         "mismatching signature @ C invocation ('%s' != '%s')",
                         signature, fb->c.signature);

                *ret_type = MRP_FUNCBRIDGE_STRING;
                ret_value->string = mrp_strdup(errmsg);
                success = false;
            }
            break;

        case MRP_LUA_FUNCTION:
            sp = lua_gettop(L);
            mrp_funcbridge_push(L, fb);
            lua_rawgeti(L, -1, 1);
            luaL_checktype(L, -1, LUA_TFUNCTION);
            for (i = 0;   (t = signature[i]);   i++) {
                a = args + i;
                switch (t) {
                case MRP_FUNCBRIDGE_STRING:
                    lua_pushstring(L, a->string);
                    break;
                case MRP_FUNCBRIDGE_INTEGER:
                    lua_pushinteger(L, a->integer);
                    break;
                case MRP_FUNCBRIDGE_FLOATING:
                    lua_pushnumber(L, a->floating);
                    break;
                case MRP_FUNCBRIDGE_BOOLEAN:
                    lua_pushboolean(L, a->boolean);
                    break;
                case MRP_FUNCBRIDGE_OBJECT:
                    mrp_lua_push_object(L, a->pointer);
                    break;
                case MRP_FUNCBRIDGE_MRPLUATYPE:
                    mrp_lua_push_object(L, a->pointer);
                    break;
                default:
                    success = false;
                    goto done;
                }

                if (!(i % 20) && i)
                    mrp_lua_checkstack(L, -1);
            }

            sts = lua_pcall(L, i, 1, 0);

            MRP_ASSERT(!sts || (sts && lua_type(L, -1) == LUA_TSTRING),
                       "lua pcall did not return error string when failed");

            switch (lua_type(L, -1)) {
            case LUA_TSTRING:
                *ret_type = MRP_FUNCBRIDGE_STRING;
                ret_value->string = mrp_strdup(lua_tolstring(L, -1, NULL));
                break;
            case LUA_TNUMBER:
                *ret_type = MRP_FUNCBRIDGE_FLOATING;
                ret_value->floating = lua_tonumber(L, -1);
                break;
            case LUA_TBOOLEAN:
                *ret_type = MRP_FUNCBRIDGE_BOOLEAN;
                ret_value->boolean = lua_toboolean(L, -1);
                break;
            case LUA_TTABLE:
            {
                int size = 4; /* array size (initial) */
                int item_size = 0; /* array item size (calculated) */
                void *items = NULL;
                int allowed_type = LUA_TNIL;
                bool first = true;
                int j = 0;

                *ret_type = MRP_FUNCBRIDGE_ARRAY;

                /* push NIL to stack as the first key */
                lua_pushnil(L);

                while (lua_next(L, -2)) {
                    if (first == true) {
                        first = false;
                        allowed_type = lua_type(L, -1);
                        switch (allowed_type) {
                        case LUA_TNUMBER:
                            ret_value->array.type = MRP_FUNCBRIDGE_FLOATING;
                            item_size = sizeof(lua_Number);
                            break;
                        case LUA_TBOOLEAN:
                            ret_value->array.type = MRP_FUNCBRIDGE_BOOLEAN;
                            item_size = sizeof(int);
                            break;
                        case LUA_TSTRING:
                            ret_value->array.type = MRP_FUNCBRIDGE_STRING;
                            item_size = sizeof(char *);
                            break;
                        default:
                            goto error;
                        }
                        items = mrp_allocz(size * item_size);
                        if (!items) {
                            goto error;
                        }
                    }
                    else {
                        /* check that all members of the table are of the same
                         * type */
                        if (lua_type(L, -1) != allowed_type) {
                            goto error;
                        }
                    }

                    if (size == j+1) {
                        /* check size */
                        size *= 2;
                        items = mrp_realloc(items, size * item_size);
                        if (!items) {
                            goto error;
                        }
                    }

                    switch (allowed_type) {
                    case LUA_TNUMBER:
                    {
                        lua_Number *arr = (lua_Number *) items;
                        arr[j] = lua_tonumber(L, -1);
                        break;
                    }
                    case LUA_TBOOLEAN:
                    {
                        int *arr = (int *) items;
                        arr[j] = lua_toboolean(L, -1);
                        break;
                    }
                    case LUA_TSTRING:
                    {
                        char **arr = (char **) items;
                        char *value = mrp_strdup(lua_tostring(L, -1));

                        if (!value) {
                            goto error;
                        }

                        arr[j] = value;
                        break;
                    }
                    default:
                        /* impossible */
                        break;
                    }

                    j++;

                    /* remove the value, keep key */
                    lua_pop(L, 1);
                }

                ret_value->array.nitem = j;
                ret_value->array.items = items;

                break;
            error:
                if (allowed_type == LUA_TSTRING) {
                    /* we need to free possibly allocated strings */
                    int k;
                    char **arr = items;

                    if (items) {
                        for (k = 0; k < j; k++) {
                            mrp_free(arr[k]);
                        }
                    }
                }

                mrp_free(items);

                *ret_type = MRP_FUNCBRIDGE_NO_DATA;
                memset(ret_value, 0, sizeof(*ret_value));
                sts = 1; /* success is later calculated from !sts */
                mrp_log_error("funcbridge: error reading array from Lua");
                break;
            }

            default:
                *ret_type = MRP_FUNCBRIDGE_NO_DATA;
                memset(ret_value, 0, sizeof(*ret_value));
                break;
            }
            success = !sts;
        done:
            lua_settop(L, sp);
            break;

        default:
            success = false;
            break;
        }
    }

    return success;
}


int mrp_funcbridge_push(lua_State *L, mrp_funcbridge_t *fb)
{
    if (!fb)
        lua_pushnil(L);
    else
        lua_rawgeti(L, LUA_REGISTRYINDEX, fb->luatbl);
    return 1;
}


mrp_funcarray_t *mrp_funcarray_create(lua_State *L)
{
    int table;
    mrp_funcarray_t *fa;

    lua_createtable(L, 0, 1);
    table = lua_gettop(L);

    luaL_getmetatable(L, FUNCARRAY_METATABLE);
    lua_setmetatable(L, table);

    lua_pushliteral(L, "userdata");

    fa = (mrp_funcarray_t *)lua_newuserdata(L,sizeof(mrp_funcarray_t));
    memset(fa, 0, sizeof(mrp_funcarray_t));

    luaL_getmetatable(L, FUNCARRAY_USERDATA_METATABLE);
    lua_setmetatable(L, -2);

    lua_rawset(L, table);

    return fa;
}



bool mrp_funcarray_call_from_c(lua_State *L,
                               mrp_funcarray_t *fa,
                               const char *signature,
                               mrp_funcbridge_value_t *args)
{
    size_t i;
    bool success, ok;
    char rtyp;
    mrp_funcbridge_value_t rval;

    if (!fa || (fa->nfunc > 0 && !fa->funcs))
        success = false;
    else {
        success = true;

        for (i = 0;   i < fa->nfunc;   i++) {
            ok = mrp_funcbridge_call_from_c(L, fa->funcs[i], signature, args,
                                            &rtyp, &rval);
            if (!ok || rtyp != MRP_FUNCBRIDGE_BOOLEAN || !rval.boolean)
                success = false;
        }
    }

    return success;
}

mrp_funcarray_t *mrp_funcarray_check(lua_State *L, int t)
{
    mrp_funcarray_t *fa;
    size_t i, len;

    if (t < 0 && t > LUA_REGISTRYINDEX)
        t = lua_gettop(L) + t + 1;

    switch (lua_type(L, t)) {

    case LUA_TFUNCTION:
        fa = mrp_funcarray_create(L);

        fa->funcs = calloc(1, sizeof(mrp_funcbridge_t *));
        if (!fa->funcs)
            return NULL;

        fa->nfunc = 1;
        fa->funcs[0] = mrp_funcbridge_create_luafunc(L, t);

        break;

    case LUA_TTABLE:
        if (!(fa = to_funcarray(L, t))) {
            fa = mrp_funcarray_create(L);

            len = luaL_getn(L, t);

            fa->funcs = calloc(len, sizeof(mrp_funcbridge_t *));
            fa->nfunc = len;

            for (i = 0;  i < len;  i++) {
                lua_pushnumber(L, (int)(i + 1));
                lua_gettable(L, t);

                fa->funcs[i] = mrp_funcbridge_create_luafunc(L, -1);

                lua_pop(L, 1);
            }

            lua_replace(L, t);
        }
        break;

    default:
        luaL_typerror(L, t, "function array");
        fa = NULL;
        break;
    }

    return fa;
}


static mrp_funcbridge_t *create_funcbridge(lua_State *L, int narr, int nrec)
{
    int table;
    mrp_funcbridge_t *fb;

    lua_createtable(L, narr, nrec);
    table = lua_gettop(L);

    luaL_getmetatable(L, FUNCBRIDGE_METATABLE);
    lua_setmetatable(L, table);

    lua_pushliteral(L, "userdata");

    fb = (mrp_funcbridge_t *)lua_newuserdata(L,sizeof(mrp_funcbridge_t));
    memset(fb, 0, sizeof(mrp_funcbridge_t));

    luaL_getmetatable(L, FUNCBRIDGE_USERDATA_METATABLE);
    lua_setmetatable(L, -2);

    lua_rawset(L, table);

    lua_pushvalue(L, -1);
    fb->luatbl = luaL_ref(L, LUA_REGISTRYINDEX);

    fb->refcnt = 1;

    return fb;
}


static mrp_funcbridge_t *check_funcbridge(lua_State *L, int t)
{
    mrp_funcbridge_t *fb;

    luaL_checktype(L, t, LUA_TTABLE);

    lua_pushvalue(L, t);
    lua_pushliteral(L, "userdata");
    lua_rawget(L, -2);

    fb = luaL_checkudata(L, -1, FUNCBRIDGE_USERDATA_METATABLE);
    luaL_argcheck(L, fb != NULL, 1, "'function bridge' expected");

    lua_pop(L, 2);

    return fb;
}

static int call_funcbridge_from_lua(lua_State *L)
{
    mrp_funcbridge_t *fb = check_funcbridge(L, 1);

    return make_lua_call(L, fb, 1);
}

static int get_funcbridge_field(lua_State *L)
{
    lua_pushnil(L);
    return 1;
}

static int set_funcbridge_field(lua_State *L)
{
    return luaL_error(L, "attempt to write a readonly object");
}

static int funcbridge_destructor(lua_State *L)
{
    mrp_funcbridge_t *fb;

    fb = luaL_checkudata(L, -1, FUNCBRIDGE_USERDATA_METATABLE);

    if (!fb->dead) {
        fb->dead = true;
        mrp_funcbridge_unref(L, fb);
    }

    return 0;
}

static int call_funcarray_from_lua(lua_State *L)
{
    mrp_funcarray_t *fa;
    int narg, top;
    size_t i;
    int j;
    int success;

    top  = lua_gettop(L);
    narg = top - 1;

    if (!(fa = to_funcarray(L, 1)))
        luaL_typerror(L, 1, "function array");

    if (fa->nfunc > 0 && !fa->funcs)
        luaL_error(L, "attempt to call a corruptfunction array");

    success = true;

    for (i = 0;   i < fa->nfunc;   i++) {
        mrp_funcbridge_push(L, fa->funcs[i]);

        for (j = 0;  j < narg;  j++)
            lua_pushvalue(L, j+2);

        make_lua_call(L, fa->funcs[i], top+1);

        if (!lua_isboolean(L, -1) || !lua_toboolean(L, -1))
            success = false;

        lua_settop(L, top);
    }

    lua_pushboolean(L, success);
    lua_replace(L, 1);

    lua_settop(L, 1);

    return 1;
}

static int get_funcarray_field(lua_State *L)
{
    lua_pushnil(L);
    return 1;
}

static int set_funcarray_field(lua_State *L)
{
    luaL_error(L, "attempt to change a function array");
    return 0;
}

static mrp_funcarray_t *to_funcarray(lua_State *L, int t)
{
    mrp_funcarray_t *fa = NULL;

    t = (t < 0) ? lua_gettop(L) + t + 1 : t;

    if (t < 0 && t > LUA_REGISTRYINDEX)
        t = lua_gettop(L) + t + 1;

    if (lua_istable(L, t)) {
        lua_pushstring(L, "userdata");
        lua_rawget(L, t);

        if (!lua_isnil(L, -1))
            fa = luaL_checkudata(L, -1, FUNCARRAY_USERDATA_METATABLE);

        lua_pop(L, 1);
    }

    return fa;
}

static int funcarray_destructor(lua_State *L)
{
    mrp_funcarray_t *fa;
    size_t i;

    fa = luaL_checkudata(L, -1, FUNCARRAY_USERDATA_METATABLE);

    if (fa->funcs) {
        for (i = 0;   i < fa->nfunc;   i++)
            mrp_funcbridge_unref(L, fa->funcs[i]);

        free(fa->funcs);
    }

    memset(fa, 0, sizeof(mrp_funcarray_t));

    return 0;
}


static inline int funcbridge_type(mrp_lua_type_t type)
{
#define MAP(_t, _fbt) case MRP_LUA_##_t: return MRP_FUNCBRIDGE_##_fbt;
    switch (type) {
        MAP(STRING , STRING);
        MAP(INTEGER, INTEGER);
        MAP(DOUBLE , FLOATING);
        MAP(BOOLEAN, BOOLEAN);
        MAP(OBJECT , OBJECT);
        MAP(STRING_ARRAY , ARRAY);
        MAP(INTEGER_ARRAY, ARRAY);
        MAP(DOUBLE_ARRAY , ARRAY);
        MAP(BOOLEAN_ARRAY, ARRAY);
    default:
        return MRP_FUNCBRIDGE_UNSUPPORTED;
    }
}


static inline int funcbridge_elemtype(mrp_lua_type_t type)
{
    switch (type) {
        MAP(STRING_ARRAY , STRING);
        MAP(INTEGER_ARRAY, INTEGER);
        MAP(DOUBLE_ARRAY , FLOATING);
        MAP(BOOLEAN_ARRAY, BOOLEAN);
    default:
        return MRP_FUNCBRIDGE_UNSUPPORTED;
    }
}


static int autobridge_patch(lua_State *L, void *object, int npop, int *refs)
{
    int i;

    /* remove nref elements from the bottom, save references to them */
    for (i = 1; i <= npop; i++) {
        lua_pushvalue(L, 1);
        lua_remove(L, 1);
    }

    for (i = -npop; i <= -1; i++) {
        refs[npop+i] = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    if (object && mrp_lua_check_object(L, NULL, 1) != object) {
        mrp_log_error("wrong stack detected before calling autobridge");
#if 0
        /*
         * Hmm... on a second though, let's not count on that only
         * the self/object argument is missing for the autobridge
         * method call. Who knows what else is wrong with the stack...
         */
        if (object != NULL) {
            mrp_lua_push_object(L, object);
            lua_insert(L, 1);
        }
#endif
        return -1;
    }

    return 0;
}


static void autobridge_restore(lua_State *L, void *object, int npop, int *refs)
{
    int i;

    MRP_UNUSED(object);

    lua_settop(L, 0);

    for (i = 1; i <= npop; i++) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, refs[i-1]);
        luaL_unref(L, LUA_REGISTRYINDEX, refs[i-1]);
        lua_insert(L, 1);
    }
}


static int make_lua_call(lua_State *L, mrp_funcbridge_t *fb, int f)
{
#define ARG_MAX   256
#define ARRAY_MAX 256

    int ret;
    int i, n, m, b, e, tidx;
    const char *s;
    char t;
    mrp_funcbridge_value_t args[ARG_MAX];
    mrp_funcbridge_value_t *a, r;
    mrp_lua_type_t          type;
    int                     elem;
    size_t                  tlen;
    int                     status;
    int                     refs[3] = { LUA_NOREF, LUA_NOREF, LUA_NOREF };

    e = lua_gettop(L);
    f = (f < 0) ? e + f + 1 : f;
    b = f + 1 + (fb->autobridge ? 1 : 0);
    n = e - b + 1;

    mrp_debug("fn:%d, beg:%d, end:%d, num:%d", f, b, e, n);

    switch (fb->type) {

    case MRP_C_FUNCTION:
        m = strlen(fb->c.signature);

        if (n >= ARG_MAX - 1 || n > m)
            return luaL_error(L, "too many arguments (%d > %d)", n, m);
        if (n < m)
            return luaL_error(L, "too few arguments (%d < %d)", n, m);

        tidx = 0;
        for (i = b, s = fb->c.signature, a= args;    i <= e;    i++, s++, a++){
            switch (*s) {
            case MRP_FUNCBRIDGE_STRING:
                a->string = luaL_checklstring(L, i, NULL);
                break;
            case MRP_FUNCBRIDGE_INTEGER:
                a->integer = luaL_checkinteger(L, i);
                break;
            case MRP_FUNCBRIDGE_FLOATING:
                a->floating = luaL_checknumber(L, i);
                break;
            case MRP_FUNCBRIDGE_OBJECT:
                a->pointer = mrp_lua_check_object(L, NULL, i);
                break;
            case MRP_FUNCBRIDGE_BOOLEAN:
                a->boolean = lua_toboolean(L, i);
                break;
            case MRP_FUNCBRIDGE_ARRAY:
                if (fb->c.sigtypes == NULL ||
                    (type = fb->c.sigtypes[tidx++]) == MRP_LUA_NONE) {
                invalid_array_type:
                    return luaL_error(L, "type info missing for array or "
                                      "array argument %d", (i - b + 1));
                }

                switch (type) {
                case MRP_LUA_STRING_ARRAY:
                    tlen = sizeof(char  *);
                    elem = MRP_FUNCBRIDGE_STRING;
                    break;
                case MRP_LUA_INTEGER_ARRAY:
                    tlen = sizeof(int32_t);
                    elem = MRP_FUNCBRIDGE_INTEGER;
                    break;
                case MRP_LUA_DOUBLE_ARRAY:
                    tlen = sizeof(double );
                    elem = MRP_FUNCBRIDGE_DOUBLE;
                    break;
                case MRP_LUA_BOOLEAN_ARRAY:
                    tlen = sizeof(bool   );
                    elem = MRP_FUNCBRIDGE_BOOLEAN;
                    break;
                case MRP_LUA_ANY:
                    tlen = sizeof(double);
                    elem = MRP_FUNCBRIDGE_ANY;
                    break;
                default: goto invalid_array_type;
                }

                a->array.items = alloca(tlen * ARRAY_MAX);
                a->array.nitem = ARRAY_MAX;

                if (mrp_lua_object_collect_array(L, i, &a->array.items,
                                                 &a->array.nitem, &type,
                                                 false, NULL, 0) < 0) {
                    return luaL_error(L, "failed to collect array");
                }

                if (elem == MRP_FUNCBRIDGE_ANY) {
                    elem = funcbridge_elemtype(type);
                    if (elem == MRP_FUNCBRIDGE_UNSUPPORTED)
                        goto invalid_array_type;
                }
                a->array.type = elem;
                break;

            default:
                return luaL_error(L, "argument %d has unsupported type '%c'",
                                  (i - b) + 1, i);
            }
        }
        memset(a, 0, sizeof(*a));

        if (fb->autobridge && fb->usestack) {
            mrp_debug("patching stack for autobridge %p", fb->c.func);

            if (autobridge_patch(L, fb->c.data, 1, refs) < 0) {
                autobridge_restore(L, fb->c.data, 1, refs);
                return luaL_error(L, "incorrect stack to call autobridge %p",
                                  fb->c.func);
            }
        }

        status = fb->c.func(L, fb->c.data, fb->c.signature, args, &t, &r);

        if (fb->autobridge && fb->usestack) {
            mrp_debug("restoring stack after autobridge call");

            autobridge_restore(L, fb->c.data, 1, refs);
        }

        if (!status)
            return luaL_error(L, "c function invocation failed");

        switch (t) {
        case MRP_FUNCBRIDGE_NO_DATA:
            ret = 0;
            break;
        case MRP_FUNCBRIDGE_STRING:
            ret = 1;
            lua_pushstring(L, r.string);
            break;
        case MRP_FUNCBRIDGE_INTEGER:
            ret = 1;
            lua_pushinteger(L, r.integer);
            break;
        case MRP_FUNCBRIDGE_FLOATING:
            ret = 1;
            lua_pushnumber(L, r.floating);
            break;
        default:
            ret = 0;
            lua_pushnil(L);
        }
        break;

    case MRP_LUA_FUNCTION:
        lua_rawgeti(L, f, 1);
        luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_replace(L, f);
        lua_pcall(L, n, 1, 0);
        ret = 1;
        break;

    default:
        return luaL_error(L, "internal error");
    }

    return ret;

#undef ARG_MAX
#undef ARRAY_MAX
}


int mrp_call_funcbridge(lua_State *L, mrp_funcbridge_t *fb, int f)
{
    return make_lua_call(L, fb, f);
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
