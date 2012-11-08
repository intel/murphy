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
            return 0;

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


MURPHY_REGISTER_LUA_BINDINGS(murphy,
                             { "info"   , log_info    },
                             { "warning", log_warning },
                             { "error"  , log_error   });
