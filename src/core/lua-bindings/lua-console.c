#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/core/console.h>
#include <murphy/core/lua-bindings/murphy.h>

static void eval_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    lua_State *L;
    char       code[1024], *p;
    int        i, n, l, len;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    L = mrp_lua_get_lua_state();

    if (L == NULL) {
        printf("Lua runtime not available or initialized.");
        return;
    }

    p   = code;
    l   = sizeof(code);
    len = 0;
    for (i = 2; i < argc; i++) {
        n = snprintf(p, l, "%s", argv[i]);

        if (n >= l)
            return;

        l   -= n;
        p   += n;
        len += n;
    }

    if (luaL_loadbuffer(L, code, len, "<console>") || lua_pcall(L, 0, 0, 0))
        printf("Lua error: %s\n", lua_tostring(L, -1));

    lua_settop(L, 0);
}


static void source_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    lua_State   *L;
    struct stat  st;
    char        *path, *code;
    size_t       size;
    ssize_t      len;
    int          fd;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc != 3) {
        printf("Invalid arguments, expecting a single path.");
        return;
    }
    else
        path = argv[2];

    L = mrp_lua_get_lua_state();

    if (L == NULL) {
        printf("Lua runtime not available or initialized.");
        return;
    }

    if (path && *path) {
        if (stat(path, &st) == 0) {
            fd = open(path, O_RDONLY);

            if (fd >= 0) {
                size = st.st_size;
                code = alloca(size);
                len  = read(fd, code, size);
                close(fd);

                if (len > 0) {
                    if (luaL_loadbuffer(L, code, len, path) != 0 ||
                        lua_pcall(L, 0, 0, 0) != 0)
                        printf("Lua error: %s\n", lua_tostring(L, -1));

                    lua_settop(L, 0);
                }
            }
            else
                printf("Failed to open %s (%d: %s).", path,
                       errno, strerror(errno));
        }
        else
            printf("Failed to open %s (%d: %s).", path, errno, strerror(errno));
    }
}


#define LUA_GROUP_DESCRIPTION                                    \
    "Lua commands allows one to evaluate Lua code either from\n" \
    "the console command line itself, or from sourced files.\n"

#define EVAL_SYNTAX      "eval <lua-code>"
#define EVAL_SUMMARY     "evaluate the given snippet of Lua code"
#define EVAL_DESCRIPTION                                               \
    "Evaluate the given snippet of Lua code. Currently you have to\n"  \
    "fully quote the Lua code you are trying to evaluate to protect\n" \
    "it from the tokenizer of the console input parser. This is the\n" \
    "easiest to accomplish by surrounding your Lua code snippet in\n"  \
    "single or double quotes unconditionally.\n"

#define SOURCE_SYNTAX      "source <lua-file>"
#define SOURCE_SUMMARY     "evaluate the Lua script from the given <lua-file>"
#define SOURCE_DESCRIPTION "Read and evaluate the contents of <lua-file>.\n"

MRP_CORE_CONSOLE_GROUP(lua_group, "lua", LUA_GROUP_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("eval", eval_cb, TRUE,
                          EVAL_SYNTAX, EVAL_SUMMARY, EVAL_DESCRIPTION),
        MRP_TOKENIZED_CMD("source", source_cb, FALSE,
                          SOURCE_SYNTAX, SOURCE_SUMMARY, SOURCE_DESCRIPTION),
    });
