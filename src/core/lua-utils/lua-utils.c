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

#include "murphy/common/debug.h"
#include "murphy/common/log.h"
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


void mrp_lua_checkstack(lua_State *L, int extra)
{
    /*
     * Notes:
     *
     *   We have a systematic bug throughout our codebase. We never ever
     *   grow the Lua stack according to our needs. We simply rely on the
     *   available space to be enough. When we occasionally do run out of
     *   stack space, this causes severe memory corruption.
     *
     *   This is relatively easy to trigger with Lua 5.1.x but much harder
     *   with Lua 5.2.x (I could not reproduce this with 5.2.x at all).
     *
     *   This function is merely a desperate kludgish attemp to try and
     *   hide the damage caused by the bug. In a couple of commonly used
     *   functions we call this to make sure there's plenty of space in the
     *   stack and hope that it will be enough also for those who do not
     *   ensure this themselves.
     *
     * XXX TODO: Eventually we'll need to fix this properly.
     */

    lua_checkstack(L, extra > 0 ? extra : 40);
}


const char *mrp_lua_callstack(lua_State *L, char *buf, size_t size, int depth)
{
    int         top;
    int         i, b, e;
    const char *w;
    char       *p;
    int         n, l;

    if (depth <= 0)
        depth = 16;

    top = lua_gettop(L);

    p = buf;
    l = (int)size;

    *p = '\0';

    b = e = -1;
    for (i = 0; i < depth; i++) {
        luaL_where(L, i);

        if (lua_isnil(L, -1))
            break;

        w = lua_tostring(L, -1);

        if (!(w && *w)) {
            if (b < 0)
                b = e = i;
            else
                e = i;

            continue;
        }

        if (b >= 0  && e == i - 1) {
            if (b != e)
                n = snprintf(p, l, "\n    [#%d-%d] ?", b, e);
            else
                n = snprintf(p, l, "\n    [#%d] ?", b);
            b = e = -1;

            p += n;
            l -= n;

            if (l <= 0)
                goto out;
        }

        n = snprintf(p, l, "\n    [#%d] @%s", i, w);

        p += n;
        l -= n;

        if (l <= 0)
            goto out;
    }

 out:
    lua_settop(L, top);

    return buf;
}


void mrp_lua_calltrace(lua_State *L, int depth, bool debug)
{
    char buf[1024];

    if (debug)
        mrp_debug("\n%s", mrp_lua_callstack(L, buf, sizeof(buf), depth));
    else
        mrp_log_info("%s", mrp_lua_callstack(L, buf, sizeof(buf), depth));
}
