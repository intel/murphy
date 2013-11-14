/*
 * Copyright (c) 2012, 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef __MURPHY_LUA_JSON_H__
#define __MURPHY_LUA_JSON_H__

#include <murphy/common/json.h>
#include <murphy/core/lua-bindings/murphy.h>

/** Create a Lua JSON object with an empty JSON object. */
int  mrp_json_lua_create(lua_State *L);

/** Create a Lua JSON object, wrap the given JSON object, increase refcount. */
void *mrp_json_lua_wrap(lua_State *L, mrp_json_t *json);

/** Get and add a reference to a wrapped JSON object. */
mrp_json_t *mrp_json_lua_unwrap(void *lson);

/** Wrap the given JSON object, increase refcount and push it on the stack. */
int mrp_json_lua_push(lua_State *L, mrp_json_t *json);

/** Get the JSON object at the given stack position, increase refcount. */
mrp_json_t *mrp_json_lua_get(lua_State *L, int idx);

#endif /* __MURPHY_LUA_JSON_H__ */
