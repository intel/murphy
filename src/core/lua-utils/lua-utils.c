/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#include <string.h>

#include "murphy/core/lua-utils/lua-utils.h"


void mrp_lua_setglobal(lua_State *L, const char *name)
{
#if LUA_VERSION_NUM >= 502
    lua_setglobal(L, name);
#else
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_insert(L, -2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
#endif
}


void mrp_lua_setglobal_idx(lua_State *L, int idx)
{
    if (lua_isstring(L, idx)) {
        lua_pushvalue(L, idx);
        mrp_lua_setglobal(L, lua_tostring(L, -1));
    }
}


void mrp_lua_getglobal(lua_State *L, const char *name)
{
#if LUA_VERSION_NUM >= 502
    lua_getglobal(L, name);
#else
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
#endif
}


void mrp_lua_getglobal_idx(lua_State *L, int idx)
{
    if (lua_isstring(L, idx)) {
        lua_pushvalue(L, idx);
        mrp_lua_getglobal(L, lua_tostring(L, -1));
        lua_remove(L, -2);
    }
}


const char *mrp_lua_findtable(lua_State *L, int t, const char *field, int size)
{
    const char *p, *n;
    size_t      l;

    if (t != MRP_LUA_GLOBALTABLE) {      /* t == 0 indicates a global */
        if (!lua_istable(L, t))
            return field;
        else
            lua_pushvalue(L, t);
    }

    for (p = field; p != NULL; p = n && *n ? n + 1: NULL) {
        if ((n = strchr(p, '.')) != NULL)
            l = n - p;
        else
            l = strlen(p);

        lua_pushlstring(L, p, l);

        if (!(p == field && t == MRP_LUA_GLOBALTABLE))
            lua_rawget(L, -2);
        else
            mrp_lua_getglobal_idx(L, -1);

        switch (lua_type(L, -1)) {
        case LUA_TTABLE:
            lua_remove(L, -2);
            break;

        case LUA_TNIL:
            lua_pop(L, 1);
            lua_createtable(L, 0, n && *n ? 1 : size);
            lua_pushlstring(L, p, l);
            lua_pushvalue(L, -2);
            lua_settable(L, -4);
            lua_remove(L, -2);
            break;

        default:
            lua_pop(L, 2);
            return p;
        }
    }

    lua_remove(L, -2);
    return NULL;
}
