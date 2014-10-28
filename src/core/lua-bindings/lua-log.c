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

#include <murphy/common/log.h>
#include <murphy/core/lua-bindings/murphy.h>


static int call_function(lua_State *L, const char *table, const char *method)
{
    int n, type, status;

    if (table != NULL)
        lua_getglobal(L, table);

    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, method);
        lua_remove(L, -2);

        if (lua_isfunction(L, -1)) {
            n = lua_gettop(L);
            lua_insert(L, 1);

            status = lua_pcall(L, n - 1, 1, 0);
        }
        else {
            type = lua_type(L, -1);
            lua_pop(L, 1);

            if (type == LUA_TNIL)
                lua_pushfstring(L, "non-existent member %s", method);
            else
                lua_pushfstring(L, "member %s is not a function", method);

            status = -1;
        }
    }
    else {
        if (table != NULL)
            lua_pushfstring(L, "%s is not a table", table);
        else
            lua_pushfstring(L, "requested field %s of a non-table", method);

        status = -1;
    }


    return status;
}


static int log_msg(lua_State *L, int level)
{
    static int  loaded = FALSE;
    int         n      = lua_gettop(L);
    lua_Debug   caller;
    const char *file, *func;
    int         line;
    int         top;

    top = lua_gettop(L);

    if (!loaded) {
        luaopen_string(L);
        loaded = TRUE;
    }

    if (lua_isuserdata(L, 1)) {
        lua_remove(L, 1);                /* remove self if any */
        n--;
    }

    if (n > 1)
        if (call_function(L, "string", "format") != 0)
            goto out;

    lua_getstack(L, 1, &caller);
    if (lua_getinfo(L, "Snl", &caller)) {
        func = caller.name   ? caller.name   : "<lua-function>";
        file = caller.source ? caller.source : "<lua-source>";
        line = caller.currentline;
    }
    else {
        func = "<lua-function>";
        line = 0;
        file = "<lua-source>";
    }

    mrp_log_msg(level, file, line, func, "%s", lua_tostring(L, 1));

 out:
    lua_settop(L, top);

    return 0;
}


static int log_info(lua_State *L)
{
    return log_msg(L, MRP_LOG_INFO);
}


static int log_warning(lua_State *L)
{
    return log_msg(L, MRP_LOG_WARNING);

}


static int log_error(lua_State *L)
{
    return log_msg(L, MRP_LOG_ERROR);
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, NULL,
                             { "info"   , log_info    },
                             { "warning", log_warning },
                             { "error"  , log_error   });
