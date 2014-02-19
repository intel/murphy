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

#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>


static int stringify_table(lua_State *L, int index,
                           char **bufp, int *sizep, int *offsp);

static int plugin_exists(lua_State *L)
{
    mrp_context_t *ctx;
    const char    *name;

    ctx = mrp_lua_check_murphy_context(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    name = lua_tostring(L, 2);

    mrp_debug("lua: check if plugin '%s' exists", name);

    lua_pushboolean(L, mrp_plugin_exists(ctx, name));

    return 1;
}


static int plugin_loaded(lua_State *L)
{
    mrp_context_t *ctx;
    const char    *name;

    ctx = mrp_lua_check_murphy_context(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    name = lua_tostring(L, 2);

    mrp_debug("lua: check if plugin '%s' is loaded", name);

    lua_pushboolean(L, mrp_plugin_running(ctx, name));

    return 1;
}


static int ensure_buffer(char **bufp, int *sizep, int *offsp, int len)
{
    int spc, size, diff;

    spc = *sizep - *offsp;
    if (spc < len) {
        diff = len - spc;

        if (diff > *sizep)
            size = *sizep + diff;
        else
            size = *sizep * 2;

        if (!mrp_realloc(*bufp, size))
            return -1;
        else
            *sizep = size;
    }

    return 0;
}


static int push_buffer(char **bufp, int *sizep, int *offsp,
                       const char *str, int len)
{
    if (len <= 0)
        len = strlen(str);

    if (ensure_buffer(bufp, sizep, offsp, len + 1) == 0) {
        strcpy(*bufp + *offsp, str);
        *offsp += len;

        return 0;
    }
    else
        return -1;
}


static int stringify_string(lua_State *L, int index,
                            char **bufp, int *sizep, int *offsp)
{
    const char *str;
    size_t      size;
    int         len;

    str = lua_tolstring(L, index, &size);
    len = (int)size;

    if (ensure_buffer(bufp, sizep, offsp, len + 3) < 0)
        return -1;

    snprintf(*bufp + *offsp, len + 3, "'%s'", str);
    *offsp += len + 2;

    return 0;
}


static int stringify_number(lua_State *L, int index,
                                char **bufp, int *sizep, int *offsp)
{
    char   num[64];
    double d;
    int    i, len;

    if ((d = lua_tonumber(L, index)) == 1.0 * (i = lua_tointeger(L, index)))
        len = snprintf(num, sizeof(num), "%d", i);
    else
        len = snprintf(num, sizeof(num), "%f", d);

    return push_buffer(bufp, sizep, offsp, num, len);
}


static int stringify_boolean(lua_State *L, int index,
                                 char **bufp, int *sizep, int *offsp)
{
    const char *bln;
    int         len;

    if (lua_toboolean(L, index)) {
        bln = "true";
        len = 4;
    }
    else {
        bln = "false";
        len = 5;
    }

    return push_buffer(bufp, sizep, offsp, bln, len);
}


static int stringify_object(lua_State *L, int index,
                            char **bufp, int *sizep, int *offsp)
{
    switch (lua_type(L, index)) {
    case LUA_TSTRING:  return stringify_string(L, index, bufp, sizep, offsp);
    case LUA_TNUMBER:  return stringify_number(L, index, bufp, sizep, offsp);
    case LUA_TBOOLEAN: return stringify_boolean(L, index, bufp, sizep, offsp);
    case LUA_TTABLE:   return stringify_table(L, index, bufp, sizep, offsp);
    default:
        errno = EINVAL;
    }

    return -1;
}


static int stringify_table(lua_State *L, int index,
                           char **bufp, int *sizep, int *offsp)
{
    const char *sep, *p;
    char        key[256];
    int         arr;
    int         i, d, idx, len;

    arr = TRUE;

    lua_pushnil(L);

    /*
     * check what the table should be converted to (array or dictionary)
     */
    idx = -1;
    while (arr && lua_next(L, index - 1)) {
        switch (lua_type(L, -2)) {
        case LUA_TBOOLEAN:
        case LUA_TTABLE:
        default:
        invalid:
            lua_pop(L, 3);
            return -1;

        case LUA_TNUMBER:
            d = lua_tonumber(L, -2);
            i = lua_tointeger(L, -2);

            if (d != 1.0 * i || (idx >= 0 && i != idx + 1))
                goto invalid;
            else
                idx = i;
            break;

        case LUA_TSTRING:
            lua_pop(L, 1);
            arr = FALSE;
            break;
        }

        lua_pop(L, 1);
    }


    /*
     * convert either to an array or a dictionary
     */

    if (push_buffer(bufp, sizep, offsp, arr ? "[" : "{", 1))
        return -1;

    sep = "";
    lua_pushnil(L);
    while (lua_next(L, index - 1)) {
        if (!arr) {
            len = snprintf(key, sizeof(key), "%s'%s':", sep,
                           lua_tostring(L, -2));
            p = key;
        }
        else {
            p   = (char *)sep;
            len = strlen(sep);
        }

        if (push_buffer(bufp, sizep, offsp, p, len) < 0)
            return -1;

        if (stringify_object(L, -1, bufp, sizep, offsp) < 0) {
            lua_pop(L, 3);
            return -1;
        }

        lua_pop(L, 1);
        sep = ",";
    }

    if (push_buffer(bufp, sizep, offsp, arr ? "]" : "}", 1) < 0)
        return -1;
    else
        return 0;
}


static int load(lua_State *L, int may_fail)
{
    mrp_context_t    *ctx;
    mrp_plugin_t     *plugin;
    char              name[256], instbuf[256];
    const char       *instance, *argerr;
    mrp_plugin_arg_t  args[256];
    int               narg, n, type, t, success;
    char             *json;
    int               size, offs;

    ctx = mrp_lua_check_murphy_context(L, 1);
    n   = lua_gettop(L);

    if (n < 2 || n > 4)
        return luaL_error(L, "%s called with incorrect arguments",
                          __FUNCTION__);

    luaL_checktype(L, 2, LUA_TSTRING);
    snprintf(name, sizeof(name), "%s", lua_tostring(L, 2));
    instance = NULL;
    narg     = 0;

    mrp_debug("lua: %sload-plugin '%s'", may_fail ? "try-" : "", name);

    switch (n) {
    case 2:
        break;

    case 3:
        type = lua_type(L, 3);

        if (type == LUA_TTABLE) {
            t = 3;
            goto parse_arguments;
        }
        else if (type == LUA_TSTRING) {
            snprintf(instbuf, sizeof(instbuf), "%s", lua_tostring(L, 3));
            instance = instbuf;
        }
        else
            return luaL_error(L, "%s expects string or table as 2nd argument",
                              __FUNCTION__);
        break;

    case 4:
    default:
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TTABLE);
        snprintf(instbuf, sizeof(instbuf), "%s", lua_tostring(L, 3));
        instance = instbuf;
        t        = 4;
    parse_arguments:
        mrp_clear(&args);
        lua_pushnil(L);
        while (lua_next(L, t) != 0) {
            if (narg >= (int)MRP_ARRAY_SIZE(args)) {
                argerr = "too many plugin arguments";
                goto arg_error;
            }

            if (lua_type(L, -2) != LUA_TSTRING) {
                argerr = "non-string argument table key";
                goto arg_error;
            }

            args[narg].type = MRP_PLUGIN_ARG_TYPE_STRING;
            args[narg].key  = mrp_strdup(lua_tostring(L, -2));

            switch (lua_type(L, -1)) {
            case LUA_TSTRING:
            case LUA_TNUMBER:
                args[narg].str = mrp_strdup(lua_tostring(L, -1));
                break;
            case LUA_TBOOLEAN:
                args[narg].str = mrp_strdup(lua_toboolean(L, -1) ?
                                            "true" : "false");
                break;
            case LUA_TTABLE:
                json = NULL;
                size = 0;
                offs = 0;
                if (stringify_table(L, -1, &json, &size, &offs) == 0)
                    args[narg].str = json;
                else {
                    argerr = "failed to json-stringify Lua table";
                    goto arg_error;
                }
                break;
            default:
                argerr = "invalid argument table value";
                goto arg_error;
            }
            mrp_debug("lua: argument #%d: '%s' = '%s'", narg,
                      args[narg].key, args[narg].str);
            narg++;

            lua_pop(L, 1);
        }
        break;
    }

    plugin = mrp_load_plugin(ctx, name, instance, narg ? args : NULL, narg);

    if (plugin != NULL) {
        plugin->may_fail = may_fail;

        success = TRUE;
    }
    else {
        success = FALSE;

        if (!may_fail)
            return luaL_error(L, "failed to load plugin %s (as instance %s)",
                              name, instance ? instance : name);
    }

    while (narg > 0) {
        mrp_free(args[narg - 1].key);
        mrp_free(args[narg - 1].str);
        narg--;
    }

    lua_pushboolean(L, success);
    return 1;

 arg_error:
    while (narg > 0) {
        mrp_free(args[narg - 1].key);
        mrp_free(args[narg - 1].str);
        narg--;
    }

    return luaL_error(L, "plugin argument table error: %s", argerr);
}


static int load_plugin(lua_State *L)
{
    return load(L, FALSE);
}


static int try_load_plugin(lua_State *L)
{
    return load(L, TRUE);
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, NULL,
                             { "plugin_exists"  , plugin_exists   },
                             { "plugin_loaded"  , plugin_loaded   },
                             { "load_plugin"    , load_plugin     },
                             { "try_load_plugin", try_load_plugin });
