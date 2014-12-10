/*
 * Copyright (c) 2014 Intel Corporation
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

#include <murphy/common.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-bindings/murphy.h>

/* This is a placeholder for proper Murphy event system. The facilities for
 * sending/receiving messages along with the events have not been implemented
 * yet, and neither is sending/receiving multiple events at once (as part of
 * the event mask). Only the basic functionality for sending/receiving
 * argumentless messages exists. */

/*
 * Lua EventWatch object
 */

#define EVTWATCH_LUA_CLASS MRP_LUA_CLASS(evtwatch, lua)

typedef struct {
    lua_State         *L;          /* Lua execution context */
    mrp_context_t     *ctx;        /* murphy context */
    mrp_event_bus_t   *bus;        /* event bus to watch on */
    mrp_event_mask_t   mask;       /* mask of events to watch */
    mrp_event_watch_t *w;          /* associated murphy event watch */
    bool               init;       /* being initialized */

    /* Lua members */
    char              *bus_name;   /* name of the event bus to use */
    char             **events;     /* name of the events to watch */
    int                nevent;     /* number of event names */
    int                callback;   /* reference to callback */
    bool               oneshot;    /* disable after first matching event */
} evtwatch_lua_t;


static int evtwatch_lua_create(lua_State *L);
static void evtwatch_lua_destroy(void *data);
static void evtwatch_lua_changed(void *data, lua_State *L, int member);
static ssize_t evtwatch_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                     size_t size, lua_State *L, void *data);

static int evtwatch_lua_start(lua_State *L);
static int evtwatch_lua_stop(lua_State *L);

static void evtwatch_lua_cb(mrp_event_watch_t *ew, uint32_t id,
                            int format, void *data, void *user_data);



/*
 * Lua EventWatch class
 */

#define OFFS(m) MRP_OFFSET(evtwatch_lua_t, m)
#define RDONLY  MRP_LUA_CLASS_READONLY
#define NOTIFY  MRP_LUA_CLASS_NOTIFY
#define NOFLAGS MRP_LUA_CLASS_NOFLAGS

MRP_LUA_METHOD_LIST_TABLE(evtwatch_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(evtwatch_lua_create)
                          MRP_LUA_METHOD(stop , evtwatch_lua_stop)
                          MRP_LUA_METHOD(start, evtwatch_lua_start));

MRP_LUA_METHOD_LIST_TABLE(evtwatch_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL(evtwatch_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(evtwatch_lua_members,
    MRP_LUA_CLASS_STRING ("bus"     , OFFS(bus_name), NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_ARRAY  ("events"  , STRING, evtwatch_lua_t, events, nevent,
                                                      NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_LFUNC  ("callback", OFFS(callback), NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_BOOLEAN("oneshot" , OFFS(oneshot) , NULL, NULL, NOTIFY));


typedef enum {
    EVENT_MEMBER_BUS,
    EVENT_MEMBER_EVENTS,
    EVENT_MEMBER_CALLBACK,
    EVENT_MEMBER_ONESHOT
} event_member_t;

MRP_LUA_DEFINE_CLASS(evtwatch, lua, evtwatch_lua_t, evtwatch_lua_destroy,
        evtwatch_lua_methods, evtwatch_lua_overrides,
        evtwatch_lua_members, NULL, evtwatch_lua_changed,
        evtwatch_lua_tostring, NULL,
        MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);


static void evtwatch_stop(evtwatch_lua_t *w)
{
    mrp_event_del_watch(w->w);
    w->w = NULL;
}


static bool evtwatch_start(evtwatch_lua_t *w)
{
    if (w->w == NULL) {
        w->w = mrp_event_add_watch_mask(w->bus, &w->mask, evtwatch_lua_cb, w);
        mrp_debug("started event watch %p (%p)", w, w->w);
    }

    return w->w != NULL;
}


static void evtwatch_lua_cb(mrp_event_watch_t *watch, uint32_t id,
                            int format, void *data, void *user_data)
{
    evtwatch_lua_t *w   = (evtwatch_lua_t *)user_data;
    int             one = w->oneshot;
    int             top;

    MRP_UNUSED(watch);
    MRP_UNUSED(format);
    MRP_UNUSED(data);

    mrp_debug("got event 0x%x (%s)", id, mrp_event_name(id));

    top = lua_gettop(w->L);

    if (mrp_lua_object_deref_value(w, w->L, w->callback, false)) {
        mrp_lua_push_object(w->L, w);
        lua_pushinteger(w->L, id);

        if (lua_pcall(w->L, 2, 0, 0) != 0) {
            mrp_log_error("failed to invoke Lua event watch callback (%s), "
                          "stopping", lua_tostring(w->L, -1));
            evtwatch_stop(w);
        }

        if (one)
            evtwatch_stop(w);
    }

    lua_settop(w->L, top);
}


static void evtwatch_lua_changed(void *data, lua_State *L, int member)
{
    evtwatch_lua_t *w = (evtwatch_lua_t *)data;
    int             i;

    MRP_UNUSED(L);

    mrp_debug("event watch member #%d (%s) changed", member,
              evtwatch_lua_members[member].name);

    switch (member) {
    case EVENT_MEMBER_BUS:
        if (!w->init)
            evtwatch_stop(w);
        if (!w->bus_name || !*w->bus_name || !strcmp(w->bus_name, "global"))
            w->bus = NULL;
        else
            w->bus = mrp_event_bus_get(w->ctx->ml, w->bus_name);
        if (!w->init)
            evtwatch_start(w);
        break;

    case EVENT_MEMBER_EVENTS:
        if (!w->init)
            evtwatch_stop(w);
        mrp_mask_reset(&w->mask);
        for (i = 0; i < w->nevent; i++) {
            mrp_debug("setting event %s in mask", w->events[i]);
            mrp_mask_set(&w->mask, mrp_event_id(w->events[i]));
        }
        if (!w->init)
            evtwatch_start(w);
        break;

    case EVENT_MEMBER_CALLBACK:
        mrp_debug("callback set to (ref) %u", w->callback);
        if (w->callback == LUA_NOREF || w->callback == LUA_REFNIL) {
            if (!w->init)
                evtwatch_stop(w);
        }
        else
            if (!w->init)
                evtwatch_start(w);
        break;

    case EVENT_MEMBER_ONESHOT:
        break;

    default:
        break;
    }



}


static int evtwatch_lua_create(lua_State *L)
{
    evtwatch_lua_t *w;
    int             narg;
    char           e[128], events[512];

    narg = lua_gettop(L);

    if (narg < 1 || narg > 2)
        return luaL_error(L, "expected 0, or 1 arguments, got %d", narg - 1);

    w = (evtwatch_lua_t *)mrp_lua_create_object(L, EVTWATCH_LUA_CLASS, NULL, 0);
    w->L        = L;
    w->ctx      = mrp_lua_get_murphy_context();
    w->init     = true;
    w->callback = LUA_NOREF;

    if (mrp_lua_init_members(w, L, -2, e, sizeof(e)) != 1)
        return luaL_error(L, "failed to initialize event watch (%s)", e);

    w->init = false;

    evtwatch_start(w);

    mrp_debug("created event watch %p (%p) for events %s", w, w->w,
              mrp_event_dump_mask(&w->mask, events, sizeof(events)));

    return 1;
}


static void evtwatch_lua_destroy(void *data)
{
    evtwatch_lua_t *w = (evtwatch_lua_t *) data;

    mrp_debug("destroying Lua event watch %p", w);

    evtwatch_stop(w);

    mrp_lua_object_unref_value(w, w->L, w->callback);

    w->callback = LUA_NOREF;
}


static evtwatch_lua_t *evtwatch_lua_check(lua_State *L, int idx)
{
    return (evtwatch_lua_t *)mrp_lua_check_object(L, EVTWATCH_LUA_CLASS, idx);
}


static ssize_t evtwatch_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                     size_t size, lua_State *L, void *data)
{
    evtwatch_lua_t *w = (evtwatch_lua_t *) data;
    char            events[512];

    MRP_UNUSED(L);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "event watch <%s>",
                        mrp_event_dump_mask(&w->mask, events, sizeof(events)));
    }
}


static int evtwatch_lua_start(lua_State *L)
{
    evtwatch_lua_t *w = evtwatch_lua_check(L, -1);

    lua_pushboolean(L, evtwatch_start(w));

    return 1;
}


static int evtwatch_lua_stop(lua_State *L)
{
    evtwatch_lua_t *w = evtwatch_lua_check(L, -1);

    evtwatch_stop(w);

    return 0;
}


static int evtwatch_emit_event(lua_State *L)
{
    mrp_context_t   *ctx  = mrp_lua_get_murphy_context();
    int              narg = lua_gettop(L);
    mrp_event_bus_t *bus;
    const char      *bus_name, *event;
    uint32_t         id;
    int              flags, r;

    if (narg != 3 && narg != 4)
        return luaL_error(L, "expected 2 or 3 arguments, got %d", narg - 1);

    if (lua_type(L, 2) == LUA_TSTRING)
        bus_name = lua_tostring(L, 2);
    else if (lua_type(L, 2) == LUA_TNIL)
        bus_name = NULL;
    else
        return luaL_error(L, "expected nil or bus name as 1st argument");

    if (lua_type(L, 3) == LUA_TSTRING)
        event = lua_tostring(L, 3);
    else
        return luaL_error(L, "expected event name string as 2nd argument");

    flags = MRP_EVENT_SYNCHRONOUS;

    if (narg == 4) {
        if (lua_type(L, 4) != LUA_TBOOLEAN)
            return luaL_error(L, "expected asynchronous bool as 3rd argument");
        if (lua_toboolean(L, 4))
            flags = MRP_EVENT_ASYNCHRONOUS;
    }

    bus = mrp_event_bus_get(ctx->ml, bus_name);
    id  = mrp_event_id(event);

    mrp_debug("emitting event 0x%x (<%s>) on bus <%s>", id, event,
              bus_name ? bus_name : "global");

    r = mrp_event_emit_msg(bus, id, MRP_EVENT_SYNCHRONOUS, flags, MRP_MSG_END);

    lua_pushboolean(L, r == 0);

    return 1;
}


static int evtwatch_event_id(lua_State *L)
{
    int narg = lua_gettop(L);

    if (narg != 2)
        return luaL_error(L, "expected 1 event name argument, got %d", narg);

    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "expected event name string argument");

    lua_pushinteger(L, mrp_event_id(lua_tostring(L, 2)));

    return 1;
}


static int evtwatch_event_name(lua_State *L)
{
    int narg = lua_gettop(L);

    if (narg != 2)
        return luaL_error(L, "expected 1 event id argument, got %d", narg);

    if (lua_type(L, 2) != LUA_TNUMBER)
        return luaL_error(L, "expected event id integer argument");

    lua_pushstring(L, mrp_event_name(lua_tointeger(L, 2)));

    return 1;
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, EVTWATCH_LUA_CLASS,
                             { "EventWatch"   , evtwatch_lua_create },
                             { "emit_event"   , evtwatch_emit_event },
                             { "EventListener", evtwatch_lua_create },
                             { "send_event"   , evtwatch_emit_event },
                             { "event_id"     , evtwatch_event_id   },
                             { "event_name"   , evtwatch_event_name });
