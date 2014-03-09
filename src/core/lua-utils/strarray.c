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


#include <lualib.h>
#include <lauxlib.h>

#include <murphy/core/lua-utils/strarray.h>
#include <murphy/core/lua-utils/lua-utils.h>


mrp_lua_strarray_t *mrp_lua_check_strarray(lua_State *L, int t)
{
    size_t len;
    size_t size;
    mrp_lua_strarray_t *arr;
    size_t i;

    luaL_checktype(L, t, LUA_TTABLE);
    len  = luaL_getn(L, t);
    size = sizeof(mrp_lua_strarray_t) + sizeof(const char *) * (len + 1);

    if (!(arr = malloc(size)))
        luaL_error(L, "can't allocate %d byte long memory", size);

    arr->nstring = len;

    lua_pushvalue(L, t);

    for (i = 0;  i < len;  i++) {
        lua_pushnumber(L, (int)(i+1));
        lua_gettable(L, -2);

        arr->strings[i] = strdup(luaL_checklstring(L, -1, NULL));

        lua_pop(L, 1);
    }

    arr->strings[i] = NULL;

    lua_pop(L, 1);

    return arr;
}

int mrp_lua_push_strarray(lua_State *L, mrp_lua_strarray_t *arr)
{
    size_t i;

    if (!arr)
        lua_pushnil(L);
    else {
        lua_createtable(L, arr->nstring, 0);

        for (i = 0;  i < arr->nstring; i++) {
            lua_pushinteger(L, (int)(i+1));
            lua_pushstring(L, arr->strings[i]);
            lua_settable(L, -3);
        }
    }

    return 1;
}

void mrp_lua_free_strarray(mrp_lua_strarray_t *arr)
{
    size_t i;

    if (arr) {
        for (i = 0;  i < arr->nstring;  i++)
            free((void *)arr->strings[i]);
        free(arr);
    }
}


char *mrp_lua_print_strarray(mrp_lua_strarray_t *arr, char *buf, int len)
{
    char *p, *e, *s;
    size_t i;

    e = (p = buf) + len;

    if (len > 0) {
        if (!arr->nstring)
            p += snprintf(p, e-p, "<empty>");
        else {
            for (i = 0, s = "";    i < arr->nstring && p < e;    i++, s = ", ")
                p += snprintf(p, e-p, "%s%s", s, arr->strings[i]);
        }
    }

    return buf;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
