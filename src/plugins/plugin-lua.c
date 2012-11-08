#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>

#define LUAR_INTERPRETER_NAME "lua"


enum {
    ARG_CONFIG,                          /* configuration file */
    ARG_RESOLVER,                        /* enable resolver lua support */
};


static int load_config(lua_State *L, const char *path)
{
    if (!luaL_loadfile(L, path) && !lua_pcall(L, 0, 0, 0))
        return TRUE;
    else {
        mrp_log_error("plugin-lua: failed to load config file %s.", path);
        mrp_log_error("%s", lua_tostring(L, -1));
        lua_settop(L, 0);

        return FALSE;
    }
}


static int luaR_compile(mrp_scriptlet_t *script)
{
    mrp_interpreter_t *i    = script->interpreter;
    lua_State         *L    = i->data;
    const char        *code = script->source;
    int                len  = strlen(code);
    int                status;

    if (!luaL_loadbuffer(L, code, len, "<resolver Lua scriptlet>")) {
        script->data = (void *)(ptrdiff_t)luaL_ref(L, LUA_REGISTRYINDEX);
        status = 0;
    }
    else {
        mrp_log_error("plugin-lua: failed to compile scriptlet.");
        mrp_log_error("%s", lua_tostring(L, -1));
        status = -EINVAL;
    }

    lua_settop(L, 0);

    return status;
}


static int luaR_prepare(mrp_scriptlet_t *script)
{
    MRP_UNUSED(script);

    return 0;
}


static int luaR_execute(mrp_scriptlet_t *script, mrp_context_tbl_t *ctbl)
{
    mrp_interpreter_t *i   = script->interpreter;
    lua_State         *L   = i->data;
    int                ref = (ptrdiff_t)script->data;

    MRP_UNUSED(ctbl);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

    if (lua_isfunction(L, -1)) {
        if (!lua_pcall(L, 0, 0, 0)) {
            lua_settop(L, 0);

            return TRUE;
        }
    }

    mrp_log_error("plugin-lua: failed to execute scriptlet.");
    mrp_log_error("error: %s", lua_tostring(L, -1));
    lua_settop(L, 0);

    return FALSE;
}


static void luaR_cleanup(mrp_scriptlet_t *script)
{
    mrp_interpreter_t *i   = script->interpreter;
    lua_State         *L   = i->data;
    int                ref = (ptrdiff_t)script->data;

    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

static int plugin_init(mrp_plugin_t *plugin)
{
    static mrp_interpreter_t interpreter = {
        .name    = LUAR_INTERPRETER_NAME,
        .compile = luaR_compile,
        .prepare = luaR_prepare,
        .execute = luaR_execute,
        .cleanup = luaR_cleanup,
        .data    = NULL
    };
    mrp_plugin_arg_t *args = plugin->args;
    const char       *cfg  = args[ARG_CONFIG].str;
    int               res  = args[ARG_RESOLVER].bln;
    lua_State        *L;

    L = mrp_lua_set_murphy_context(plugin->ctx);

    if (L != NULL) {
        if (res) {
            interpreter.data = L;

            if (!mrp_register_interpreter(&interpreter)) {
                mrp_log_error("plugin-lua: failed to register interpreter.");

                return FALSE;
            }
        }
        else
            mrp_log_info("plugin-lua: resolver Lua support disabled.");

        if (load_config(L, cfg))
            return TRUE;
        else
            if (res)
                mrp_unregister_interpreter(LUAR_INTERPRETER_NAME);
    }

    return FALSE;
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;

    if (args[ARG_RESOLVER].bln)
        mrp_unregister_interpreter(LUAR_INTERPRETER_NAME);
}


#define PLUGIN_DESCRIPTION "Lua bindings for Murphy."
#define PLUGIN_HELP        "Enable Lua bindings for Murphy."
#define PLUGIN_AUTHORS     "Krisztian Litkey <kli@iki.fi>"
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)

#define DEFAULT_CONFIG  "/etc/murphy/murphy.lua"

static mrp_plugin_arg_t plugin_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_CONFIG  , STRING,  "config",  DEFAULT_CONFIG),
    MRP_PLUGIN_ARGIDX(ARG_RESOLVER, BOOL  , "resolver",TRUE),
};

MURPHY_REGISTER_PLUGIN("lua",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION, PLUGIN_AUTHORS,
                       PLUGIN_HELP, MRP_SINGLETON, plugin_init, plugin_exit,
                       plugin_args, MRP_ARRAY_SIZE(plugin_args),
                       NULL, 0,
                       NULL, 0,
                       NULL);
