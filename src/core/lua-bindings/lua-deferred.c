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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>
#include <murphy/core/lua-utils/error.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-bindings/murphy.h>

#define DEFERRED_LUA_CLASS MRP_LUA_CLASS(deferred, lua)

/*
 * Lua deferred object
 */

typedef struct {
    lua_State      *L;                   /* Lua execution context */
    mrp_mainloop_t *ml;                  /* murphy mainloop */
    mrp_deferred_t *d;                   /* associated murphy deferred */
    int             callback;            /* reference to callback */
    bool            disabled;            /* true if disabled */
    bool            oneshot;             /* true for one-shot deferreds */
} deferred_lua_t;


static int deferred_lua_create(lua_State *L);
static void deferred_lua_destroy(void *data);
static void deferred_lua_changed(void *data, lua_State *L, int member);
static ssize_t deferred_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                     size_t size, lua_State *L, void *data);
static int deferred_lua_enable(lua_State *L);
static int deferred_lua_disable(lua_State *L);

/*
 * Lua deferred class
 */

#define OFFS(m)  MRP_OFFSET(deferred_lua_t, m)
#define RDONLY   MRP_LUA_CLASS_READONLY
#define NOTIFY   MRP_LUA_CLASS_NOTIFY
#define NOFLAGS  MRP_LUA_CLASS_NOFLAGS
#define USESTACK MRP_LUA_CLASS_USESTACK

MRP_LUA_METHOD_LIST_TABLE(deferred_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(deferred_lua_create)
                          MRP_LUA_METHOD(disable, deferred_lua_disable)
                          MRP_LUA_METHOD(enable , deferred_lua_enable));

MRP_LUA_METHOD_LIST_TABLE(deferred_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (deferred_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(deferred_lua_members,
    MRP_LUA_CLASS_LFUNC  ("callback" , OFFS(callback) , NULL, NULL, NOTIFY )
    MRP_LUA_CLASS_BOOLEAN("disabled" , OFFS(disabled) , NULL, NULL, NOTIFY )
    MRP_LUA_CLASS_BOOLEAN("oneshot"  , OFFS(oneshot)  , NULL, NULL, NOTIFY ));

typedef enum {
    DEFERRED_MEMBER_CALLBACK,
    DEFERRED_MEMBER_DISABLED,
    DEFERRED_MEMBER_ONESHOT,
} deferred_member_t;

MRP_LUA_DEFINE_CLASS(deferred, lua, deferred_lua_t, deferred_lua_destroy,
                     deferred_lua_methods, deferred_lua_overrides,
                     deferred_lua_members, NULL, deferred_lua_changed,
                     deferred_lua_tostring, NULL,
                     MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);


static void deferred_lua_cb(mrp_deferred_t *deferred, void *user_data)
{
    deferred_lua_t *d   = (deferred_lua_t *)user_data;
    int             one = d->oneshot;
    int             top;

    MRP_UNUSED(deferred);

    top = lua_gettop(d->L);

    if (mrp_lua_object_deref_value(d, d->L, d->callback, false)) {
        mrp_lua_push_object(d->L, d);

        if (lua_pcall(d->L, 1, 0, 0) != 0) {
            mrp_log_error("failed to invoke Lua deferred callback, disabling");
            mrp_disable_deferred(d->d);
            d->disabled = true;
        }
    }

    if (one) {
        mrp_disable_deferred(d->d);
        d->disabled = true;
    }

    lua_settop(d->L, top);
}


static void deferred_lua_changed(void *data, lua_State *L, int member)
{
    deferred_lua_t *d = (deferred_lua_t *)data;

    MRP_UNUSED(L);

    mrp_debug("deferred member #%d (%s) changed", member,
              deferred_lua_members[member].name);

    switch (member) {
    case DEFERRED_MEMBER_DISABLED:
        if (d->disabled)
            mrp_disable_deferred(d->d);
        else
            mrp_enable_deferred(d->d);
        mrp_debug("deferred %p(%p) is now %sabled", d, d->d,
                  d->disabled ? "dis" : "en");
        break;

    case DEFERRED_MEMBER_CALLBACK:
        if (!d->disabled) {
            if (d->callback == LUA_NOREF)
                mrp_disable_deferred(d->d);
            else
                mrp_enable_deferred(d->d);
            mrp_debug("deferred %p(%p) is now %sabled", d, d->d,
                      d->disabled ? "dis" : "en");
        }
        break;

    default:
        break;
    }
}


static int deferred_lua_create(lua_State *L)
{

    mrp_context_t  *ctx    = mrp_lua_get_murphy_context();
    char            e[128] = "";
    deferred_lua_t *d;
    int             narg;

    if (ctx == NULL)
        luaL_error(L, "failed to get murphy context");

    narg = lua_gettop(L);

    d = (deferred_lua_t *)mrp_lua_create_object(L, DEFERRED_LUA_CLASS, NULL, 0);

    d->L        = L;
    d->ml       = ctx->ml;
    d->d        = mrp_add_deferred(d->ml, deferred_lua_cb, d);
    d->callback = LUA_NOREF;

    if (d->d == NULL)
        return luaL_error(L, "failed to create Lua Murphy deferred");

    switch (narg) {
    case 1:
        break;
    case 2:
        if (mrp_lua_init_members(d, L, -2, e, sizeof(e)) != 1)
            return luaL_error(L, "failed to initialize deferred (%s)", e);
        break;
    default:
        return luaL_error(L, "expecting 0 or 1 arguments, got %d", narg);
    }

    if (d->disabled || d->callback == LUA_NOREF || d->callback == LUA_REFNIL)
        mrp_disable_deferred(d->d);

    return 1;
}


static void deferred_lua_destroy(void *data)
{
    deferred_lua_t *d = (deferred_lua_t *)data;

    mrp_debug("destroying Lua deferred %p", data);

    mrp_del_deferred(d->d);
    d->d = NULL;

    mrp_lua_object_unref_value(d, d->L, d->callback);

    d->callback = LUA_NOREF;
}


static deferred_lua_t *deferred_lua_check(lua_State *L, int idx)
{
    return (deferred_lua_t *)mrp_lua_check_object(L, DEFERRED_LUA_CLASS, idx);
}


static ssize_t deferred_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                     size_t size, lua_State *L, void *data)
{
    deferred_lua_t *d = (deferred_lua_t *)data;

    MRP_UNUSED(L);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "{%s %s deferred %p}",
                        d->disabled ? "disabled" : "enabled",
                        d->oneshot  ? "oneshot"  : "recurring",
                        d->d);
    }
}


static int deferred_lua_enable(lua_State *L)
{
    deferred_lua_t *d = deferred_lua_check(L, -1);

    if (d == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (d->d != NULL && d->callback != LUA_NOREF && d->callback != LUA_REFNIL) {
        mrp_enable_deferred(d->d);
        d->disabled = false;
    }

    lua_pushboolean(L, !d->disabled);

    return 1;
}


static int deferred_lua_disable(lua_State *L)
{
    deferred_lua_t *d = deferred_lua_check(L, -1);

    if (d == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (d->d != NULL) {
        mrp_disable_deferred(d->d);
        d->disabled = true;
    }

    lua_pushboolean(L, true);

    return 1;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, DEFERRED_LUA_CLASS,
                             { "Deferred", deferred_lua_create });
