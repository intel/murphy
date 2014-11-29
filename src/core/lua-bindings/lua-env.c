/*
 * Copyright (c) 2014, Intel Corporation
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

#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>


static int env_lua_getenv(lua_State *L)
{
    int         narg = lua_gettop(L);
    int         offs, i;
    const char *v;

    switch (lua_type(L, 1)) {
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        offs = 2;
        break;
    default:
        offs = 1;
        break;
    }

    for (i = offs; i <= narg; i++) {
        if (lua_type(L, offs) == LUA_TSTRING)
            v = getenv(lua_tostring(L, offs));
        else
            v = NULL;

        lua_remove(L, offs);

        if (v)
            lua_pushstring(L, v);
        else
            lua_pushnil(L);
    }

    return narg + 1 - offs;
}


static int env_lua_getpid(lua_State *L)
{
    lua_pushinteger(L, getpid());

    return 1;
}


static int env_lua_getuid(lua_State *L)
{
    lua_pushinteger(L, getuid());

    return 1;
}


static int env_lua_geteuid(lua_State *L)
{
    lua_pushinteger(L, geteuid());

    return 1;
}


static int env_lua_getgid(lua_State *L)
{
    lua_pushinteger(L, getuid());

    return 1;
}


static int env_lua_getuser(lua_State *L)
{
    struct passwd pwd, *r;
    char          buf[1024];

    getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &r);

    if (r != NULL)
        lua_pushstring(L, pwd.pw_name);
    else
        lua_pushnil(L);

    return 1;
}



MURPHY_REGISTER_LUA_BINDINGS(murphy, NULL,
                             { "getenv" , env_lua_getenv  },
                             { "getpid" , env_lua_getpid  },
                             { "getuid" , env_lua_getuid  },
                             { "geteuid", env_lua_geteuid },
                             { "getgid" , env_lua_getgid  },
                             { "getuser", env_lua_getuser });
