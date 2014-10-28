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
#include <stdlib.h>
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
    int        len, top;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(grp);
    MRP_UNUSED(cmd);

    L = mrp_lua_get_lua_state();

    if (L == NULL) {
        printf("Lua runtime not available or initialized.");
        return;
    }

    top = lua_gettop(L);

    len = strlen(code);
    if (luaL_loadbuffer(L, code, len, "<console>") || lua_pcall(L, 0, 0, 0))
        printf("Lua error: %s\n", lua_tostring(L, -1));

    lua_settop(L, top);
}


static void source_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    lua_State   *L;
    struct stat  st;
    char        *path, *code;
    size_t       size;
    ssize_t      len;
    int          fd;
    int          top;

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
        top = lua_gettop(L);

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
                }
            }
            else
                printf("Failed to open %s (%d: %s).\n", path,
                       errno, strerror(errno));
        }
        else
            printf("Failed to open %s (%d: %s).\n", path,
                   errno, strerror(errno));

        lua_settop(L, top);
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


static void dump_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    lua_State            *L = mrp_lua_get_lua_state();
    mrp_lua_tostr_mode_t  mode;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    switch (argc) {
    case 2:
        mode = MRP_LUA_TOSTR_CHECKDUMP;
        break;

    case 3:
        if      (!strcmp(argv[2], "default"))   mode = MRP_LUA_TOSTR_DEFAULT;
        else if (!strcmp(argv[2], "stackdump")) mode = MRP_LUA_TOSTR_STACKDUMP;
        else if (!strcmp(argv[2], "errordump")) mode = MRP_LUA_TOSTR_ERRORDUMP;
        else if (!strcmp(argv[2], "checkdump")) mode = MRP_LUA_TOSTR_CHECKDUMP;
        else {
            printf("Unknown dump mode '%s', using default.\n", argv[2]);
            mode = MRP_LUA_TOSTR_DEFAULT;
        }
        break;

    case 4:
#define MAP(var, name, value) if (!strcmp(var, name)) mode |= value
        mode = 0;
        MAP(argv[2], "lua"    , MRP_LUA_TOSTR_LUA    );
        MAP(argv[2], "minimal", MRP_LUA_TOSTR_MINIMAL);
        MAP(argv[2], "compact", MRP_LUA_TOSTR_COMPACT);
        MAP(argv[2], "oneline", MRP_LUA_TOSTR_ONELINE);
        MAP(argv[2], "short"  , MRP_LUA_TOSTR_SHORT  );
        MAP(argv[2], "medium" , MRP_LUA_TOSTR_MEDIUM );
        MAP(argv[2], "full"   , MRP_LUA_TOSTR_FULL   );
        MAP(argv[2], "verbose", MRP_LUA_TOSTR_VERBOSE);
        MAP(argv[2], "meta"   , MRP_LUA_TOSTR_META   );
        MAP(argv[2], "data"   , MRP_LUA_TOSTR_DATA   );
        MAP(argv[2], "both"   , MRP_LUA_TOSTR_BOTH   );

        MAP(argv[3], "lua"    , MRP_LUA_TOSTR_LUA    );
        MAP(argv[3], "minimal", MRP_LUA_TOSTR_MINIMAL);
        MAP(argv[3], "compact", MRP_LUA_TOSTR_COMPACT);
        MAP(argv[3], "oneline", MRP_LUA_TOSTR_ONELINE);
        MAP(argv[3], "short"  , MRP_LUA_TOSTR_SHORT  );
        MAP(argv[3], "medium" , MRP_LUA_TOSTR_MEDIUM );
        MAP(argv[3], "full"   , MRP_LUA_TOSTR_FULL   );
        MAP(argv[3], "verbose", MRP_LUA_TOSTR_VERBOSE);
        MAP(argv[3], "meta"   , MRP_LUA_TOSTR_META   );
        MAP(argv[3], "data"   , MRP_LUA_TOSTR_DATA   );
        MAP(argv[3], "both"   , MRP_LUA_TOSTR_BOTH   );
#undef MAP
        break;

    default:
        printf("Invalid dump command.\n");
        return;
    }

    if (L != NULL)
        mrp_lua_dump_objects(MRP_LUA_TOSTR_CHECKDUMP, L, stdout);
}


static void gc_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    lua_State *L = mrp_lua_get_lua_state();
    int        pause, step;
    char      *e;

    MRP_UNUSED(c);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    MRP_UNUSED(user_data);

    if (L == NULL)
        return;

    switch (argc) {
    case 2:
        goto full;

    case 3:
        if (!strcmp(argv[2], "full")) {
        full:
            printf("Performing a full Lua garbage collection cycle...\n");
            lua_gc(L, LUA_GCCOLLECT, 0);
        }
        else if (!strcmp(argv[2], "stop")) {
            lua_gc(L, LUA_GCSTOP, 0);
            printf("Lua garbage collector stopped...\n");
        }
        else if (!strcmp(argv[2], "start")) {
            lua_gc(L, LUA_GCRESTART, 0);
            printf("Lua garbage collector restarted...\n");
        }
        else
        invalid:
            printf("Invalid Lua garbage collector command.\n");
        break;

    case 5:
        if (strcmp(argv[2], "set"))
            goto invalid;

        pause = (int)strtoul(argv[3], &e, 10);
        if (*e) {
            printf("Invalid Lua garbage collector pause '%s'.\n", argv[3]);
            return;
        }

        step = strtoul(argv[4], &e, 10);
        if (*e) {
            printf("Invalid Lua garbage collector step '%s'.\n", argv[4]);
            return;
        }

        printf("Setting Lua garbage collector pause=%d, step=%d...\n",
               pause, step);

        lua_gc(L, LUA_GCSETPAUSE, pause);
        lua_gc(L, LUA_GCSETSTEPMUL, step);
        break;

    default:
        goto invalid;
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
#define DEBUG_SUMMARY      "configure Murphy Lua debugging"
#define DEBUG_DESCRIPTION  "Configure Murphy Lua debugging."

#define DUMP_SYNTAX        "dump [dump-flags]"
#define DUMP_SUMMARY       "dump active Murphy Lua objects"
#define DUMP_DESCRIPTION                                                     \
    "Dump unfreed Murphy Lua objects per object class. You need to enable\n" \
    "object tracking for this to work. The easiest way to do this is to\n"   \
    "set the environment variable__MURPHY_MM_CONFIG=\"lua:true\" before\n"   \
    "starting the daemon. dump-flags control how much information gets\n"    \
    "printed about a single object. If you use a single dump-flag, it can\n" \
    "be one of default, stackdump, errordump, or checkdump. If omitted,\n"   \
    "default is used. You can also give a pair of dump flags, the first\n"   \
    "of lua, minimal, compact, oneline, short, medium, full, or verbose\n"   \
    "and the second one of meta, data, or both. These correspond directly\n" \
    "to the object to string conversion mode flags of the Murphy Lua\n"      \
    "object infrastructure. At the moment these flags have very little\n"    \
    "practical effect on the actual dump as most of the dump modes have\n"   \
    "not been implemented yet so now they are just aliased to the default.\n"

#define GC_SYNTAX        "gc [full|stop|start|set <pause> <step>"
#define GC_SUMMARY       "trigger or configure the Lua garbage collector"
#define GC_DESCRIPTION   "Trigger or configure the Lua garbage collector."

MRP_CORE_CONSOLE_GROUP(lua_group, "lua", LUA_GROUP_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("source", source_cb, FALSE,
                          SOURCE_SYNTAX, SOURCE_SUMMARY, SOURCE_DESCRIPTION),
        MRP_RAWINPUT_CMD("eval", eval_cb,
                         MRP_CONSOLE_CATCHALL | MRP_CONSOLE_SELECTABLE,
                         EVAL_SYNTAX, EVAL_SUMMARY, EVAL_DESCRIPTION),
        MRP_TOKENIZED_CMD("debug", debug_cb, FALSE,
                          DEBUG_SYNTAX, DEBUG_SUMMARY, DEBUG_DESCRIPTION),
        MRP_TOKENIZED_CMD("dump", dump_cb, FALSE,
                          DUMP_SYNTAX, DUMP_SUMMARY, DUMP_DESCRIPTION),
        MRP_TOKENIZED_CMD("gc", gc_cb, FALSE,
                          GC_SYNTAX, GC_SUMMARY, GC_DESCRIPTION),
    });
