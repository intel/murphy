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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>


/*
 * Lua bitwise operations
 */

static int bitwise_lua_and(lua_State *L);
static int bitwise_lua_or(lua_State *L);
static int bitwise_lua_xor(lua_State *L);
static int bitwise_lua_neg(lua_State *L);

static int bitwise_lua_and(lua_State *L)
{
    int narg = lua_gettop(L);
    int offs, i, v;

    switch (lua_type(L, 1)) {
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        offs = 2;
        break;
    default:
        offs = 1;
        break;
    }

    v = lua_tointeger(L, offs);
    for (i = offs + 1; i <= narg; i++)
        v &= lua_tointeger(L, i);

    lua_pushinteger(L, v);
    return 1;
}


static int bitwise_lua_or(lua_State *L)
{
    int narg = lua_gettop(L);
    int offs, i, v;

    switch (lua_type(L, 1)) {
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        offs = 2;
        break;
    default:
        offs = 1;
        break;
    }

    v = lua_tointeger(L, offs);
    for (i = offs + 1; i <= narg; i++)
        v |= lua_tointeger(L, i);

    lua_pushinteger(L, v);
    return 1;
}


static int bitwise_lua_xor(lua_State *L)
{
    int narg = lua_gettop(L);
    int offs, i, v;

    switch (lua_type(L, 1)) {
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        offs = 2;
        break;
    default:
        offs = 1;
        break;
    }

    v = lua_tointeger(L, offs);
    for (i = offs + 1; i <= narg; i++)
        v ^= lua_tointeger(L, i);

    lua_pushinteger(L, v);
    return 1;
}


static int bitwise_lua_neg(lua_State *L)
{
    int narg = lua_gettop(L);
    int arg, offs;

    switch (lua_type(L, 1)) {
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        offs = 2;
        break;
    default:
        offs = 1;
        break;
    }

    if (narg != offs)
        return luaL_error(L, "bitwise NEG takes a single argument");

    arg = lua_tointeger(L, offs);
    lua_pushinteger(L, ~arg);

    return 1;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, NULL,
                             { "AND"   , bitwise_lua_and },
                             { "OR"    , bitwise_lua_or  },
                             { "XOR"   , bitwise_lua_xor },
                             { "NEG"   , bitwise_lua_neg },
                             { "NEGATE", bitwise_lua_neg });
