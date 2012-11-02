#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>


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


static int load(lua_State *L, int may_fail)
{
    mrp_context_t    *ctx;
    mrp_plugin_t     *plugin;
    char              name[256], instbuf[256];
    const char       *instance, *argerr;
    mrp_plugin_arg_t  args[256];
    int               narg, n, type, t, success;

    ctx = mrp_lua_check_murphy_context(L, 1);
    n   = lua_gettop(L);

    if (n < 2 || n > 4)
        luaL_error(L, "%s called with incorrect arguments", __FUNCTION__);

    luaL_checktype(L, 2, LUA_TSTRING);
    snprintf(name, sizeof(name), "%s", lua_tostring(L, 2));
    narg = 0;

    mrp_debug("lua: %sload-plugin '%s'", may_fail ? "try-" : "", name);

    switch (n) {
    case 2:
        instance = NULL;
        break;

    case 3:
        type = lua_type(L, 3);

        if (type == LUA_TTABLE) {
            instance = NULL;
            t        = 3;
            goto parse_arguments;
        }
        else if (type == LUA_TSTRING) {
            snprintf(instbuf, sizeof(instbuf), "%s", lua_tostring(L, 3));
            instance = instbuf;
        }
        else
            luaL_error(L, "%s expects string or table as 2nd argument",
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
            if (narg > (int)MRP_ARRAY_SIZE(args)) {
                argerr = "too many plugin arguments";
                goto arg_error;
            }

            if (lua_type(L, -2) != LUA_TSTRING) {
                argerr = "non-string argument table key";
                goto arg_error;
            }

            args[narg].type = MRP_PLUGIN_ARG_TYPE_STRING;
            args[narg].key  = mrp_strdup(lua_tostring(L, -2));
            args[narg].str  = mrp_strdup(lua_tostring(L, -1));
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
            luaL_error(L, "failed to load plugin %s (as instance %s)",
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

    luaL_error(L, "plugin argument table error: %s", argerr);
    return 0;
}


static int load_plugin(lua_State *L)
{
    return load(L, FALSE);
}


static int try_load_plugin(lua_State *L)
{
    return load(L, TRUE);
}


MURPHY_REGISTER_LUA_BINDINGS(murphy,
                             { "plugin_exists"  , plugin_exists   },
                             { "load_plugin"    , load_plugin     },
                             { "try_load_plugin", try_load_plugin });
