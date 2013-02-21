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

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-decision/element.h>
#include <murphy/core/lua-bindings/murphy.h>

static mrp_context_t *context;
static MRP_LIST_HOOK(bindings);
static int debug_level;

static int create_murphy_object(lua_State *L)
{
    mrp_lua_murphy_t *m;

    m = (mrp_lua_murphy_t *)lua_newuserdata(L, sizeof(*m));

    m->ctxp = &context;

    luaL_getmetatable(L, "murphy");
    lua_setmetatable(L, -2);

    return 1;
}


static int register_murphy(mrp_context_t *ctx)
{
    static luaL_reg functions[] = {
        { "get", create_murphy_object },
        { NULL , NULL                 }
    };
    lua_State *L = ctx->lua_state;

    luaL_newmetatable(L, "murphy");
    lua_pushliteral(L, "__index");       /* murphy.__index = murphy */
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    luaL_openlib(L, "murphy", functions, 0);

    return TRUE;
}


static int register_bindings(mrp_lua_bindings_t *b)
{
    lua_State *L = context->lua_state;
    luaL_reg  *m;

    luaL_getmetatable(L, b->meta);

    for (m = b->methods; m->name != NULL; m++) {
        lua_pushstring(L, m->name);
        lua_pushcfunction(L, m->func);
        lua_rawset(L, -3);
    }

    return TRUE;
}


int mrp_lua_register_murphy_bindings(mrp_lua_bindings_t *b)
{
    mrp_context_t   *ctx;
    lua_State       *L;

    mrp_list_append(&bindings, &b->hook);

    if ((ctx = context) != NULL && (L = ctx->lua_state) != NULL)
        return register_bindings(b);
    else
        return TRUE;
}


static void init_lua_utils(lua_State *L)
{
    mrp_create_funcbridge_class(L);
    mrp_create_funcarray_class(L);
}


static void init_lua_decision(lua_State *L)
{
    mrp_lua_create_mdb_class(L);
    mrp_lua_create_element_class(L);
}


static lua_State *init_lua(void)
{
    lua_State *L = luaL_newstate();

    if (L != NULL) {
        luaopen_base(L);
        init_lua_utils(L);
        init_lua_decision(L);
    }

    return L;
}


lua_State *mrp_lua_set_murphy_context(mrp_context_t *ctx)
{
    lua_State          *L;
    mrp_list_hook_t    *p, *n;
    mrp_lua_bindings_t *b;
    int                 success;

    if (context == NULL) {
        L = init_lua();

        if (L != NULL) {
            ctx->lua_state = L;
            context        = ctx;

            if (register_murphy(ctx)) {
                success = TRUE;

                init_lua_utils(L);
                init_lua_decision(L);

                mrp_list_foreach(&bindings, p, n) {
                    b = mrp_list_entry(p, typeof(*b), hook);
                    success &= register_bindings(b);
                }

                return L;
            }
        }
    }

    return NULL;
}


mrp_context_t *mrp_lua_check_murphy_context(lua_State *L, int index)
{
    mrp_lua_murphy_t *m;

    m = (mrp_lua_murphy_t *)luaL_checkudata(L, index, "murphy");
    luaL_argcheck(L, m, index, "murphy object expected");

    if (*m->ctxp == NULL)
        return (void *)(ptrdiff_t)luaL_error(L, "murphy context is not set");
    else
        return *m->ctxp;
}


mrp_context_t *mrp_lua_get_murphy_context(void)
{
    return context;
}


lua_State *mrp_lua_get_lua_state(void)
{
    if (context != NULL)
        return context->lua_state;
    else
        return NULL;
}


static void lua_debug(lua_State *L, lua_Debug *ar)
{
#define RUNNING(_ar, _what) ((_ar)->what != NULL && !strcmp((_ar)->what, _what))
#define ALIGNFMT "%*.*s"
#define ALIGNARG 4 * depth, 4 * depth, ""

    static int depth = 0;

    lua_Debug   f;
    const char *type, *name;
    char        loc[1024];

    switch (ar->event) {
    case LUA_HOOKRET:
        depth--;
        mrp_debug(ALIGNFMT"<= return", ALIGNARG);
        break;

    case LUA_HOOKTAILRET:
        depth--;
        mrp_debug(ALIGNFMT"<= tail return", ALIGNARG);
        break;

    case LUA_HOOKCALL:
        mrp_clear(&f);
        if (lua_getstack(L, 1, &f) && lua_getinfo(L, "Snl", &f)) {
            if      (RUNNING(&f, "C"))    type = "Lua-C";
            else if (RUNNING(&f, "Lua"))  type = "Lua";
            else if (RUNNING(&f, "main")) type = "Lua-main";
            else if (RUNNING(&f, "tail")) {
                mrp_debug(ALIGNFMT"=> %*.*stail-call", ALIGNARG);
                depth++;
                return;
            }

            name = f.name ? f.name : NULL;

            if (f.currentline != -1 && f.short_src != NULL)
                snprintf(loc, sizeof(loc), "@ %s:%d", f.short_src,
                         f.currentline);
            else
                loc[0] = '\0';

            if (name)
                mrp_debug(ALIGNFMT"=> %s %s %s", ALIGNARG, type, name, loc);
            else
                mrp_debug(ALIGNFMT"=> %s %s", ALIGNARG, type, loc);
        }
        else
            mrp_debug(ALIGNFMT"=> Lua", ALIGNARG);

        depth++;
        break;

    case LUA_HOOKLINE:
        mrp_clear(&f);

        if (lua_getstack(L, 1, &f) && lua_getinfo(L, "Snl", &f))
            mrp_debug(ALIGNFMT" @ %s:%d", ALIGNARG, f.short_src, f.currentline);
        else
            mrp_debug(ALIGNFMT" @ line %d", ALIGNARG, ar->currentline);
        break;

    default:
        break;
    }

#undef RUNNING
#undef ALIGNFMT
#undef ALIGNARG
}


static int setup_debug_hook(int mask)
{
    mrp_context_t *ctx     = mrp_lua_get_murphy_context();
    lua_State     *L       = ctx ? ctx->lua_state : NULL;

    return (L != NULL && lua_sethook(L, lua_debug, mask, 0));
}


static void clear_debug_hook(void)
{
    mrp_context_t *ctx = mrp_lua_get_murphy_context();
    lua_State     *L   = ctx ? ctx->lua_state : NULL;

    if (L != NULL)
        lua_sethook(L, lua_debug, 0, 0);
}




int mrp_lua_set_debug(mrp_lua_debug_t level)
{
    int success;

    if (debug_level)
        clear_debug_hook();

    switch (level) {
    case MRP_LUA_DEBUG_DISABLED:
        success = TRUE;
        break;

    case MRP_LUA_DEBUG_ENABLED:
        success = setup_debug_hook(LUA_MASKCALL | LUA_MASKRET);
        break;

    case MRP_LUA_DEBUG_DETAILED:
        success = setup_debug_hook(LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE);
        break;

    default:
        success = FALSE;
    }

    if (success)
        debug_level = level;

    return success;
}
