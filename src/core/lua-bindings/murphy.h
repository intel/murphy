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

#ifndef __MURPHY_LUA_BINDINGS_H__
#define __MURPHY_LUA_BINDINGS_H__

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/list.h>
#include <murphy/core/context.h>
#include <murphy/core/lua-utils/object.h>


typedef struct {
    const char         *meta;            /* add method to this metatable */
    luaL_reg           *methods;         /* Lua method table to register */
    mrp_lua_classdef_t *classdef;        /* class definition or NULL */
    mrp_list_hook_t     hook;            /* to list of registered bindings */
} mrp_lua_bindings_t;


typedef struct {
    mrp_context_t **ctxp;                /* murphy context */
} mrp_lua_murphy_t;


/** Macro to automatically register murphy Lua bindings on startup. */
#define MURPHY_REGISTER_LUA_BINDINGS(_metatbl, _classdef, ...) \
    static void register_##_metatbl##_bindings(void) MRP_INIT; \
                                                               \
    static void register_##_metatbl##_bindings(void) {         \
        static struct luaL_reg methods[] = {                   \
            __VA_ARGS__,                                       \
            { NULL, NULL }                                     \
        };                                                     \
        static mrp_lua_bindings_t b = {                        \
            .meta     = #_metatbl,                             \
            .methods  = methods,                               \
            .classdef = _classdef,                             \
        };                                                     \
                                                               \
        mrp_list_init(&b.hook);                                \
        mrp_lua_register_murphy_bindings(&b);                  \
    }


/** Set murphy context for the bindings. */
lua_State *mrp_lua_set_murphy_context(mrp_context_t *ctx);

/** Set the path to the main Lua configuration file. */
void mrp_lua_set_murphy_lua_config_file(const char *path);

/** Get murphy context for the bindings. */
mrp_context_t *mrp_lua_get_murphy_context(void);

/** Get the common Lua state for the bindings. */
lua_State *mrp_lua_get_lua_state(void);

/** Get the main Lua configuration directory. */
const char *mrp_lua_get_murphy_lua_config_dir(void);

/** Register the given lua murphy bindings. */
int mrp_lua_register_murphy_bindings(mrp_lua_bindings_t *b);

/** Check and get murphy context for the bindings. */
mrp_context_t *mrp_lua_check_murphy_context(lua_State *L, int index);

/** Produce a debugging dump of the Lua stack (using mrp_debug). */
void mrp_lua_dump_stack(lua_State *L, const char *prefix);

/*
 * level of debugging detail
 */

typedef enum {
    MRP_LUA_DEBUG_DISABLED = 0,          /* debugging disabled */
    MRP_LUA_DEBUG_ENABLED,               /* debugging enabled  */
    MRP_LUA_DEBUG_DETAILED,              /* detailed debugging enabled */
} mrp_lua_debug_t;

/** Configure murphy lua debugging. */
int mrp_lua_set_debug(mrp_lua_debug_t level);

#endif /* __MURPHY_LUA_BINDINGS_H__ */
