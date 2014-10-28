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

#define SIGHANDLER_LUA_CLASS MRP_LUA_CLASS(sighandler, lua)

/*
 * Lua sighandler object
 */

typedef struct {
    lua_State        *L;                 /* Lua execution context */
    mrp_mainloop_t   *ml;                /* Murphy mainloop */
    mrp_sighandler_t *h;                 /* associated murphy sighandler */
    int               signum;            /* signal number */
    int               callback;          /* reference to callback */
    bool              oneshot;           /* true for one-shot sighandlers */
} sighandler_lua_t;


static int sighandler_lua_create(lua_State *L);
static void sighandler_lua_destroy(void *data);
static void sighandler_lua_changed(void *data, lua_State *L, int member);
static ssize_t sighandler_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                       size_t size, lua_State *L, void *data);
static int sighandler_lua_enable(lua_State *L);
static int sighandler_lua_disable(lua_State *L);

/*
 * Lua sighandler class
 */

#define OFFS(m) MRP_OFFSET(sighandler_lua_t, m)
#define RDONLY  MRP_LUA_CLASS_READONLY
#define NOTIFY  MRP_LUA_CLASS_NOTIFY
#define NOFLAGS MRP_LUA_CLASS_NOFLAGS

MRP_LUA_METHOD_LIST_TABLE(sighandler_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(sighandler_lua_create)
                          MRP_LUA_METHOD(disable, sighandler_lua_disable)
                          MRP_LUA_METHOD(enable , sighandler_lua_enable));

MRP_LUA_METHOD_LIST_TABLE(sighandler_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (sighandler_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(sighandler_lua_members,
    MRP_LUA_CLASS_INTEGER("signal"   , OFFS(signum)   , NULL, NULL, RDONLY )
    MRP_LUA_CLASS_LFUNC  ("callback" , OFFS(callback) , NULL, NULL, NOFLAGS)
    MRP_LUA_CLASS_BOOLEAN("oneshot"  , OFFS(oneshot)  , NULL, NULL, NOFLAGS));

typedef enum {
    SIGHANDLER_MEMBER_SIGNAL,
    SIGHANDLER_MEMBER_CALLBACK,
    SIGHANDLER_MEMBER_ONESHOT,
} sighandler_member_t;

MRP_LUA_DEFINE_CLASS(sighandler, lua, sighandler_lua_t, sighandler_lua_destroy,
                     sighandler_lua_methods, sighandler_lua_overrides,
                     sighandler_lua_members, NULL, sighandler_lua_changed,
                     sighandler_lua_tostring, NULL,
                     MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);


static void sighandler_lua_cb(mrp_sighandler_t *hlr, int sig, void *user_data)
{
    sighandler_lua_t *h   = (sighandler_lua_t *)user_data;
    int               one = h->oneshot;
    const char       *s   = strsignal(sig);
    int               top;

    MRP_UNUSED(hlr);

    top = lua_gettop(h->L);

    if (mrp_lua_object_deref_value(h, h->L, h->callback, false)) {
        mrp_lua_push_object(h->L, h);
        if (s != NULL)
            lua_pushstring(h->L, s);
        else
            lua_pushinteger(h->L, sig);

        if (lua_pcall(h->L, 2, 0, 0) != 0)
            mrp_log_error("failed to invoke Lua sighandler callback");
    }

    if (one) {
        mrp_del_sighandler(h->h);
        h->h = NULL;
    }

    lua_settop(h->L, top);
}


static void sighandler_lua_changed(void *data, lua_State *L, int member)
{
    sighandler_lua_t *h = (sighandler_lua_t *)data;

    MRP_UNUSED(L);
    MRP_UNUSED(h);

    mrp_debug("sighandler member #%d (%s) changed", member,
              sighandler_lua_members[member].name);
}


static int sighandler_lua_create(lua_State *L)
{

    mrp_context_t    *ctx    = mrp_lua_get_murphy_context();
    char              e[128] = "";
    sighandler_lua_t *h;
    int               narg;

    if (ctx == NULL)
        luaL_error(L, "failed to get murphy context");

    narg = lua_gettop(L);

    h = (sighandler_lua_t *)mrp_lua_create_object(L, SIGHANDLER_LUA_CLASS,
                                                  NULL, 0);
    h->L        = L;
    h->ml       = ctx->ml;
    h->callback = LUA_NOREF;

    switch (narg) {
    case 1:
        break;
    case 2:
        if (mrp_lua_init_members(h, L, -2, e, sizeof(e)) != 1)
            return luaL_error(L, "failed to initialize sighandler (%s)", e);
        break;
    default:
        return luaL_error(L, "expecting 0 or 1 arguments, got %d", narg);
    }

    if (h->signum)
        h->h = mrp_add_sighandler(h->ml, h->signum, sighandler_lua_cb, h);
    else
        return luaL_error(L, "signal number must be set in constructor");

    if (h->h == NULL)
        return luaL_error(L, "failed to create Murphy sighandler");

    return 1;
}


static void sighandler_lua_destroy(void *data)
{
    sighandler_lua_t *h = (sighandler_lua_t *)data;

    mrp_debug("destroying Lua sighandler %p", data);

    mrp_del_sighandler(h->h);
    h->h = NULL;

    mrp_lua_object_unref_value(h, h->L, h->callback);

    h->callback = LUA_NOREF;
}


static sighandler_lua_t *sighandler_lua_check(lua_State *L, int idx)
{
    return (sighandler_lua_t *)mrp_lua_check_object(L,
                                                    SIGHANDLER_LUA_CLASS, idx);
}


static ssize_t sighandler_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                       size_t size, lua_State *L, void *data)
{
    sighandler_lua_t *h = (sighandler_lua_t *)data;
    const char       *s = strsignal(h->signum);

    MRP_UNUSED(L);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "{%ssighandler %p of '%s'}",
                        h->oneshot  ? "oneshot " : "", h->h,
                        s ? s : "unknow signal");
    }
}


static int sighandler_lua_enable(lua_State *L)
{
    sighandler_lua_t *h = sighandler_lua_check(L, -1);

    if (h == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (h->h == NULL && h->signum != 0)
        if (h->callback != LUA_NOREF && h->callback != LUA_REFNIL)
            mrp_add_sighandler(h->ml, h->signum, sighandler_lua_cb, h);

    lua_pushboolean(L, h->h != NULL);

    return 1;
}


static int sighandler_lua_disable(lua_State *L)
{
    sighandler_lua_t *h = sighandler_lua_check(L, -1);

    if (h == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }

    mrp_del_sighandler(h->h);
    h->h = NULL;

    lua_pushboolean(L, true);

    return 1;
}



MURPHY_REGISTER_LUA_BINDINGS(murphy, SIGHANDLER_LUA_CLASS,
                             { "SigHandler", sighandler_lua_create });
