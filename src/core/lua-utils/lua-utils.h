/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#ifndef __MURPHY_LUA_UTILS_H__
#define __MURPHY_LUA_UTILS_H__

#include <stdbool.h>

#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM >= 502
/* this backward-compatibility macro is no longer pre-defined in Lua 5.2 */
#    define  luaL_reg luaL_Reg

/* the undocumented luaL_getn is no longer available in 5.2 */
#    define luaL_getn luaL_len

/* this has been removed from Lua 5.2 */
static inline int luaL_typerror (lua_State *L, int arg, const char *type) {
    return luaL_argerror(L, arg, lua_pushfstring(L, "%s expected, got %s",
                                                 type, luaL_typename(L, arg)));
}

#    ifndef lua_objlen
#        define lua_objlen(L, i) lua_rawlen(L, (i))
#    endif
#endif

/** Convert the given stack index to an absolute one. */
static inline int mrp_lua_absidx(lua_State *L, int idx) {
    return (idx >= 0 ? idx :  (1 + lua_gettop(L) + idx));
}

/** Convert the given stack index to a relative one. */
static inline int mrp_lua_relidx(lua_State *L, int idx) {
    return (idx <= 0 ? idx : -(1 + lua_gettop(L) - idx));
}

/** Set @name to the value at the top, pops the stack. */
void mrp_lua_setglobal(lua_State *L, const char *name);

/** Set the value at the top to the name at @idx. */
void mrp_lua_setglobal_idx(lua_State *L, int idx);

/** Get the value of the global variable @name. Push nil if not found. */
void mrp_lua_getglobal(lua_State *L, const char *name);

/** Get the value of the name at @idx. Push nil if not found. */
void mrp_lua_getglobal_idx(lua_State *L, int idx);

/** Traverse table @t to find/create member @field. */
#define MRP_LUA_GLOBALTABLE 0            /* use as t for globals */
const char *mrp_lua_findtable(lua_State *L, int t, const char *field, int size);

/** Make sure there's space for at least extra values in the stack. */
void mrp_lua_checkstack(lua_State *L, int extra);

/** Produce a Lua call stack trace of the given depth. */
const char *mrp_lua_callstack(lua_State *L, char *buf, size_t size, int depth);

/** Print a Lua call stack trace of the given depth. */
void mrp_lua_calltrace(lua_State *L, int depth, bool debug);

#endif /* __MURPHY_LUA_UTILS_H__ */
