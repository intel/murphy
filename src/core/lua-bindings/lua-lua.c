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

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>


static int open_lualib(lua_State *L)
{
    struct {
        const char  *name;
        int        (*loader)(lua_State *L);
    } *lib, libs[] = {
        { "math"   , luaopen_math    },
        { "string" , luaopen_string  },
        { "io"     , luaopen_io      },
        { "os"     , luaopen_os      },
        { "table"  , luaopen_table   },
        { "debug"  , luaopen_debug   },
        { "package", luaopen_package },
        { "base"   , luaopen_base    },
        { NULL     , NULL }
    };
    const char *name;
    int         i, n;

    n = lua_gettop(L);

    if (lua_isuserdata(L, 1)) {
        lua_remove(L, 1);                /* remove self if any */
        n--;
    }

    if (n < 1)
        return luaL_error(L, "%s called without any arguments", __FUNCTION__);

    for (i = 1; i <= n; i++) {
        luaL_checktype(L, 1, LUA_TSTRING);

        name = lua_tostring(L, i);

        for (lib = libs; lib->name != NULL; lib++)
            if (!strcmp(lib->name, name))
                break;

        if (lib->loader != NULL) {
            mrp_debug("loading Lua lib '%s' with %p...", name, lib->loader);
            lib->loader(L);
        }
        else
            return luaL_error(L, "failed to load unknown Lua lib '%s'", name);
    }

    lua_settop(L, 0);
    return 0;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, { "open_lualib", open_lualib });
