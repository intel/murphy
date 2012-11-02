#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>


enum {
    ARG_CONFIG,
};


static int load_config(lua_State *L, const char *path)
{
    struct stat  st;
    char        *code;
    size_t       size;
    ssize_t      len;
    int          fd;

    if (path && *path) {
        if (stat(path, &st) == 0) {
            fd = open(path, O_RDONLY);

            if (fd >= 0) {
                size = st.st_size;
                code = alloca(size);
                len  = read(fd, code, size);
                close(fd);

                if (len >= 0) {
                    if (!luaL_loadbuffer(L, code, len, path) &&
                        !lua_pcall(L, 0, 0, 0))
                        return TRUE;
                    else {
                        mrp_log_error("%s", lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                }
            }
        }

        return FALSE;
    }
    else
        return TRUE;
}


static int plugin_init(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *args = plugin->args;
    lua_State        *L;

    L = mrp_lua_set_murphy_context(plugin->ctx);

    if (L != NULL)
        return load_config(L, args[ARG_CONFIG].str);
    else
        return FALSE;
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);
}


#define PLUGIN_DESCRIPTION "Lua bindings for Murphy."
#define PLUGIN_HELP        "Enable Lua bindings for Murphy."
#define PLUGIN_AUTHORS     "Krisztian Litkey <kli@iki.fi>"
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)

#define DEFAULT_CONFIG  "/etc/murphy/murphy.lua"

static mrp_plugin_arg_t plugin_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_CONFIG, STRING, "config", DEFAULT_CONFIG),
};

MURPHY_REGISTER_PLUGIN("lua",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION, PLUGIN_AUTHORS,
                       PLUGIN_HELP, MRP_SINGLETON, plugin_init, plugin_exit,
                       plugin_args, MRP_ARRAY_SIZE(plugin_args),
                       NULL, 0,
                       NULL, 0,
                       NULL);
