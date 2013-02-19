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
#include <string.h>
#include <alloca.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/core/console.h>
#include <murphy/core/lua-bindings/murphy.h>

static void eval_cb(mrp_console_t *c, void *user_data, const char *grp,
                    const char *cmd, char *code)
{
    lua_State *L;
    int        len;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(grp);
    MRP_UNUSED(cmd);

    L = mrp_lua_get_lua_state();

    if (L == NULL) {
        printf("Lua runtime not available or initialized.");
        return;
    }

    len = strlen(code);
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
                printf("Failed to open %s (%d: %s).\n", path,
                       errno, strerror(errno));
        }
        else
            printf("Failed to open %s (%d: %s).\n", path,
                   errno, strerror(errno));
    }
}


static void debug_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    mrp_lua_debug_t level;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    if (argc == 3) {
        if      (!strcmp(argv[2], "disable"))  level = MRP_LUA_DEBUG_DISABLED;
        else if (!strcmp(argv[2], "enable"))   level = MRP_LUA_DEBUG_ENABLED;
        else if (!strcmp(argv[2], "detailed")) level = MRP_LUA_DEBUG_DETAILED;
        else {
            printf("Invalid Lua debug level '%s'.\n", argv[2]);
            printf("The valid levels are: disable, enable, detailed.\n");
            return;
        }

        if (mrp_lua_set_debug(level))
            printf("Lua debugging level set to '%s'.\n", argv[2]);
        else
            printf("Failed to set Lua debugging level to '%s'.\n", argv[2]);
    }
    else {
        printf("Invalid usage.\n");
        printf("Argument must be disable, enable, or detailed.\n");
    }
}


#define LUA_GROUP_DESCRIPTION                                    \
    "Lua commands allows one to evaluate Lua code either from\n" \
    "the console command line itself, or from sourced files.\n"

#define EVAL_SYNTAX      "<lua-code>"
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

#define DEBUG_SYNTAX       "debug {disable, enable, detailed}"
#define DEBUG_SUMMARY      "configure Murphy Lua debugging."
#define DEBUG_DESCRIPTION  "Configure Murphy Lua debugging."

MRP_CORE_CONSOLE_GROUP(lua_group, "lua", LUA_GROUP_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("source", source_cb, FALSE,
                          SOURCE_SYNTAX, SOURCE_SUMMARY, SOURCE_DESCRIPTION),
        MRP_RAWINPUT_CMD("eval", eval_cb,
                         MRP_CONSOLE_CATCHALL | MRP_CONSOLE_SELECTABLE,
                         EVAL_SYNTAX, EVAL_SUMMARY, EVAL_DESCRIPTION),
        MRP_TOKENIZED_CMD("debug", debug_cb, FALSE,
                          DEBUG_SYNTAX, DEBUG_SUMMARY, DEBUG_DESCRIPTION),
    });
