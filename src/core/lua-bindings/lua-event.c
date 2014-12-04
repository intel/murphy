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
#include <murphy/core/event.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-bindings/murphy.h>

/* This is a placeholder for proper Murphy event system. The facilities for
 * sending/receiving messages along with the events have not been implemented
 * yet, and neither is sending/receiving multiple events at once (as part of
 * the event mask). Only the basic functionality for sending/receiving
 * argumentless messages exists. */

/*
 * Lua EventListener object
 */

#define EVENT_LUA_CLASS MRP_LUA_CLASS(event, lua)

typedef struct {
    lua_State          *L;         /* Lua execution context */
    mrp_event_watch_t *ev;         /* associated murphy event */
    bool initialized;

    /* Lua members */
    int                callback;   /* reference to callback */
    const char        *name;       /* event name */
} event_lua_t;


static int event_lua_create(lua_State *L);
static void event_lua_destroy(void *data);
static void event_lua_changed(void *data, lua_State *L, int member);
static ssize_t event_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                  size_t size, lua_State *L, void *data);


/*
 * Lua EventListener class
 */

#define OFFS(m) MRP_OFFSET(event_lua_t, m)
#define RDONLY  MRP_LUA_CLASS_READONLY
#define NOTIFY  MRP_LUA_CLASS_NOTIFY
#define NOFLAGS MRP_LUA_CLASS_NOFLAGS

MRP_LUA_METHOD_LIST_TABLE(event_lua_methods,
        MRP_LUA_METHOD_CONSTRUCTOR(event_lua_create));

MRP_LUA_METHOD_LIST_TABLE(event_lua_overrides,
        MRP_LUA_OVERRIDE_CALL (event_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(event_lua_members,
        MRP_LUA_CLASS_LFUNC  ("callback", OFFS(callback), NULL, NULL, NOFLAGS)
        MRP_LUA_CLASS_STRING ("name", OFFS(name), NULL, NULL, NOTIFY));


typedef enum {
    EVENT_MEMBER_CALLBACK,
    EVENT_MEMBER_NAME,
} event_member_t;

MRP_LUA_DEFINE_CLASS(event, lua, event_lua_t, event_lua_destroy,
        event_lua_methods, event_lua_overrides,
        event_lua_members, NULL, event_lua_changed,
        event_lua_tostring, NULL,
        MRP_LUA_CLASS_EXTENSIBLE | MRP_LUA_CLASS_DYNAMIC);


static void event_lua_cb(mrp_event_watch_t *event, int id, mrp_msg_t *msg,
        void *user_data)
{
    event_lua_t *ev = (event_lua_t *) user_data;
    int top;

    MRP_UNUSED(event);
    MRP_UNUSED(msg);

    mrp_debug("callback for event %d", id);

    top = lua_gettop(ev->L);

    if (ev->callback == LUA_NOREF || ev->callback == LUA_REFNIL)
        goto end;

    if (mrp_lua_object_deref_value(ev, ev->L, ev->callback, false)) {
        mrp_lua_push_object(ev->L, ev);

        if (lua_pcall(ev->L, 1, 0, 0) != 0) {
            mrp_log_error("failed to invoke Lua event callback, stopping");
            mrp_del_event_watch(ev->ev);
        }
    }

end:
    lua_settop(ev->L, top);
}


static void event_lua_changed(void *data, lua_State *L, int member)
{
    event_lua_t *ev = (event_lua_t *) data;

    MRP_UNUSED(L);

    if (!ev->initialized)
        return;

    mrp_debug("%s (%d)", ev->name, member);

    switch (member) {
        case EVENT_MEMBER_NAME: {
            uint32_t id;
            mrp_event_mask_t mask = 0;

            if (ev->ev) {
                mrp_del_event_watch(ev->ev);
            }
            id = mrp_get_event_id(ev->name, TRUE);
            mrp_add_event(&mask, id);

            ev->ev = mrp_add_event_watch(&mask, event_lua_cb, ev);
            break;
        }
    default:
        break;
    }
}


static int event_lua_create(lua_State *L)
{
    event_lua_t *ev;
    uint32_t id;
    mrp_event_mask_t mask = 0;
    int narg;
    char e[128];

    narg = lua_gettop(L);

    if (narg < 1 || narg > 2) {
        return luaL_error(L, "expected 0-1 constructor arguments, got %d",
                narg-1);
    }

    ev = (event_lua_t *) mrp_lua_create_object(L, EVENT_LUA_CLASS, NULL, 0);
    ev->initialized = FALSE;

    if (mrp_lua_init_members(ev, L, -2, e, sizeof(e)) != 1)
        return luaL_error(L, "failed to initialize deferred (%s)", e);

    ev->L = L;

    id = mrp_get_event_id(ev->name, TRUE);
    mrp_add_event(&mask, id);

    ev->ev = mrp_add_event_watch(&mask, event_lua_cb, ev);
    if (ev->ev == NULL)
        return luaL_error(L, "failed to create Murphy event listener");

    ev->initialized = TRUE;

    mrp_debug("created listener %p for event %s (%d)", ev, ev->name, id);

    return 1;
}


static void event_lua_destroy(void *data)
{
    event_lua_t *ev = (event_lua_t *) data;

    mrp_debug("destroying Lua event watch %p", ev);

    if (ev->ev)
        mrp_del_event_watch(ev->ev);
    ev->ev = NULL;

    mrp_lua_object_unref_value(ev, ev->L, ev->callback);

    ev->callback = LUA_NOREF;
}


static ssize_t event_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                  size_t size, lua_State *L, void *data)
{
    event_lua_t *ev = (event_lua_t *) data;

    MRP_UNUSED(L);

    mrp_debug("%p", ev);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "event '%s'", ev->name);
    }
}


static int event_send_event(lua_State *L)
{
    const char *event_name;
    uint32_t id;
    bool ret = FALSE;

    luaL_checktype(L, 2, LUA_TSTRING);
    event_name = lua_tostring(L, 2);

    id = mrp_get_event_id(event_name, FALSE);

    if (id != MRP_EVENT_UNKNOWN) {
        mrp_debug("sending event %s", event_name);
        ret = mrp_emit_event(id, NULL);
    }

    lua_pushboolean(L, ret);

    return 1;
}

MURPHY_REGISTER_LUA_BINDINGS(murphy, EVENT_LUA_CLASS,
                             { "EventListener", event_lua_create },
                             { "send_event", event_send_event });
