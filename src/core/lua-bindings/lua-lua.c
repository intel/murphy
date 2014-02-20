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

#include <unistd.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>

#include <murphy/core/lua-utils/include.h>
#include <murphy/core/lua-bindings/murphy.h>

static MRP_LIST_HOOK(included);
static int include_disabled;

static int include_lua(lua_State *L, const char *file, int try, int once)
{
    mrp_list_hook_t *files = once ? &included : NULL;
    const char      *dirs[2];

    dirs[0] = mrp_lua_get_murphy_lua_config_dir();
    dirs[1] = NULL;

    if (mrp_lua_include_file(L, file, &dirs[0], files) == 0)
        return 0;

    if (try) {
        if (errno == EINVAL) {
            if (lua_type(L, -1) == LUA_TSTRING) {
                mrp_log_warning("inclusion of '%s' failed with error '%s'",
                                file, lua_tostring(L, -1));
            }
        }

        return 0;
    }

    return -1;

}


static int include_lua_file(lua_State *L, int try, int once)
{
    const char *file;
    int         narg, status;

    if (include_disabled)
        return luaL_error(L, "Lua inclusion is disabled.");

    narg = lua_gettop(L);

    switch (narg) {
    case 1:
        if (lua_type(L, -1) != LUA_TSTRING)
            return luaL_error(L, "expecting <string> for inclusion");
        break;
    case 2:
        if (lua_type(L, -2) != LUA_TUSERDATA ||
            lua_type(L, -1) != LUA_TSTRING)
            return luaL_error(L, "expecting <murphy>, <string> for inclusion");
        break;
    default:
        return luaL_error(L, "expecting <string> for inclusion");
    }

    file = lua_tostring(L, -1);

    status = include_lua(L, file, try, once);

    if (status == 0 || try) {
        lua_settop(L, 0);
        return 0;
    }
    else {
        mrp_log_error("failed to include%s Lua file '%s'.",
                      once ? "_once" : "", file);

        return luaL_error(L, "failed to include file '%s' (%s)", file,
                          lua_type(L, -1) == LUA_TSTRING ?
                          lua_tostring(L, -1) : "<unknown error>");
    }
}


static int try_luafile(lua_State *L)
{
    return include_lua_file(L, TRUE, FALSE);
}


static int try_once_luafile(lua_State *L)
{
    return include_lua_file(L, TRUE, TRUE);
}


static int include_luafile(lua_State *L)
{
    return include_lua_file(L, FALSE, FALSE);
}


static int include_once_luafile(lua_State *L)
{
    return include_lua_file(L, FALSE, TRUE);
}


static int disable_include(lua_State *L)
{
    MRP_UNUSED(L);

    include_disabled = TRUE;

    return 0;
}


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
        else {
            if (include_disabled)
                return luaL_error(L, "Lua inclusion is disabled.");

            if (include_lua(L, name, FALSE, TRUE) < 0)
                return luaL_error(L, "failed to load unknown "
                                  "Lua library '%s'", name);
        }
    }

    lua_settop(L, 0);
    return 0;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, NULL,
                             { "open_lualib"     , open_lualib          },
                             { "include"         , include_luafile      },
                             { "include_once"    , include_once_luafile },
                             { "try_include"     , try_luafile          },
                             { "try_include_once", try_once_luafile     },
                             { "disable_include" , disable_include      });
