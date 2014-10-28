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
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>


/*
 * Lua timer object
 */

#define TIMER_LUA_CLASS MRP_LUA_CLASS(timer, lua)

typedef struct {
    lua_State     *L;                    /* Lua execution context */
    mrp_context_t *ctx;                  /* murphy context */
    mrp_timer_t   *t;                    /* associated murphy timer */
    unsigned int   msecs;                /* timer interval in milliseconds */
    int            callback;             /* reference to callback */
    bool           oneshot;              /* true for one-shot timers */
} timer_lua_t;


static int timer_lua_create(lua_State *L);
static void timer_lua_destroy(void *data);
static void timer_lua_changed(void *data, lua_State *L, int member);
static ssize_t timer_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                  size_t size, lua_State *L, void *data);
static int timer_lua_start(lua_State *L);
static int timer_lua_stop(lua_State *L);


/*
 * Lua timer class
 */

#define OFFS(m) MRP_OFFSET(timer_lua_t, m)
#define RDONLY  MRP_LUA_CLASS_READONLY
#define NOTIFY  MRP_LUA_CLASS_NOTIFY
#define NOFLAGS MRP_LUA_CLASS_NOFLAGS

MRP_LUA_METHOD_LIST_TABLE(timer_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(timer_lua_create)
                          MRP_LUA_METHOD(stop , timer_lua_stop)
                          MRP_LUA_METHOD(start, timer_lua_start));

MRP_LUA_METHOD_LIST_TABLE(timer_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (timer_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(timer_lua_members,
    MRP_LUA_CLASS_INTEGER("interval", OFFS(msecs)   , NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_LFUNC  ("callback", OFFS(callback), NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_BOOLEAN("oneshot" , OFFS(oneshot) , NULL, NULL, NOTIFY));


typedef enum {
    TIMER_MEMBER_INTERVAL,
    TIMER_MEMBER_CALLBACK,
    TIMER_MEMBER_ONESHOT
} timer_member_t;

MRP_LUA_DEFINE_CLASS(timer, lua, timer_lua_t, timer_lua_destroy,
                     timer_lua_methods, timer_lua_overrides,
                     timer_lua_members, NULL, timer_lua_changed,
                     timer_lua_tostring, NULL,
                     MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);


static void timer_lua_cb(mrp_timer_t *timer, void *user_data)
{
    timer_lua_t *t   = (timer_lua_t *)user_data;
    int          one = t->oneshot;
    int          top;

    MRP_UNUSED(timer);

    top = lua_gettop(t->L);

    if (mrp_lua_object_deref_value(t, t->L, t->callback, false)) {
        mrp_lua_push_object(t->L, t);

        if (lua_pcall(t->L, 1, 0, 0) != 0) {
            mrp_log_error("failed to invoke Lua timer callback, stopping");
            mrp_del_timer(t->t);
            t->t = NULL;
        }
    }

    if (one) {
        mrp_del_timer(t->t);
        t->t = NULL;
    }

    lua_settop(t->L, top);
}


static void timer_lua_changed(void *data, lua_State *L, int member)
{
    timer_lua_t *t = (timer_lua_t *)data;

    MRP_UNUSED(L);

    mrp_debug("timer member #%d (%s) changed", member,
              timer_lua_members[member].name);

    switch (member) {
    case TIMER_MEMBER_INTERVAL:
        if (t->t != NULL)
            mrp_mod_timer(t->t, t->msecs);
        else {
        enable:
            t->t = mrp_add_timer(t->ctx->ml, t->msecs, timer_lua_cb, t);
            if (t->t == NULL)
                luaL_error(L, "failed to create Murphy timer");
        }
        break;

    case TIMER_MEMBER_CALLBACK:
        if (t->callback == LUA_NOREF || t->callback == LUA_REFNIL) {
            mrp_del_timer(t->t);
            t->t = NULL;
        }
        else {
            if (t->t == NULL)
                goto enable;
        }
        break;

    default:
        break;
    }
}


static int timer_lua_create(lua_State *L)
{

    mrp_context_t *ctx    = mrp_lua_get_murphy_context();
    char           e[128] = "";
    timer_lua_t   *t;
    int            narg;

    if (ctx == NULL)
        luaL_error(L, "failed to get murphy context");

    narg = lua_gettop(L);

    t = (timer_lua_t *)mrp_lua_create_object(L, TIMER_LUA_CLASS, NULL, 0);

    t->L        = L;
    t->ctx      = ctx;
    t->callback = LUA_NOREF;
    t->msecs    = 5000;

    switch (narg) {
    case 1:
        break;
    case 2:
        if (mrp_lua_init_members(t, L, -2, e, sizeof(e)) != 1)
            return luaL_error(L, "failed to initialize timer members (%s)", e);
        break;
    default:
        return luaL_error(L, "expecting 0 or 1 constructor arguments, "
                          "got %d", narg);
    }

    if (t->callback != LUA_NOREF && t->callback != LUA_REFNIL && t->t == NULL) {
        t->t = mrp_add_timer(t->ctx->ml, t->msecs, timer_lua_cb, t);

        if (t->t == NULL)
            return luaL_error(L, "failed to create Murphy timer");
    }

    return 1;
}


static void timer_lua_destroy(void *data)
{
    timer_lua_t *t = (timer_lua_t *)data;

    mrp_debug("destroying Lua timer %p", data);

    mrp_del_timer(t->t);
    t->t = NULL;

    mrp_lua_object_unref_value(t, t->L, t->callback);

    t->callback = LUA_NOREF;
}


static timer_lua_t *timer_lua_check(lua_State *L, int idx)
{
    return (timer_lua_t *)mrp_lua_check_object(L, TIMER_LUA_CLASS, idx);
}


static ssize_t timer_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                  size_t size, lua_State *L, void *data)
{
    timer_lua_t *t = (timer_lua_t *)data;

    MRP_UNUSED(L);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "{%s%stimer %p @ %d msecs}",
                        t->t        ? ""         : "disabled ",
                        t->oneshot  ? "oneshot " : "",
                        t->t, t->msecs);
    }
}


static int timer_lua_start(lua_State *L)
{
    timer_lua_t *t = timer_lua_check(L, -1);

    if (t == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (t->t == NULL && t->callback != LUA_NOREF)
        t->t = mrp_add_timer(t->ctx->ml, t->msecs, timer_lua_cb, t);

    lua_pushboolean(L, t->t != NULL);

    return 1;
}


static int timer_lua_stop(lua_State *L)
{
    timer_lua_t *t = timer_lua_check(L, -1);

    if (t == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    mrp_del_timer(t->t);
    t->t = NULL;

    lua_pushboolean(L, true);

    return 1;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, TIMER_LUA_CLASS,
                             { "Timer", timer_lua_create });
