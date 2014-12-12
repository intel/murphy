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
#include <murphy/common/transport.h>
#include <murphy/common/wsck-transport.h>

#include <murphy/core/lua-utils/error.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>
#include <murphy/core/lua-bindings/lua-json.h>


/*
 * Lua transport object
 */

#define TRANSPORT_LUA_CLASS MRP_LUA_CLASS(transport, lua)

typedef struct {
    lua_State       *L;                  /* Lua execution context */
    mrp_context_t   *ctx;                /* murphy context */
    mrp_transport_t *t;                  /* associated murphy transport */
    char            *address;            /* transport address */
    mrp_sockaddr_t   addr;               /* resolved address */
    const char      *atype;              /* address type */
    socklen_t        alen;               /* resolved length */
    char            *encoding;           /* transport encoding mode */
    bool             closing;            /* whether being closed */
    struct {
        int connect;                     /*     connection event */
        int closed;                      /*     closed event */
        int recv;                        /*     connected recv event */
        int recvfrom;                    /*     unconnected recv event */
    }                callback;           /* event callback references */
    int              data;               /* referece to callback data */
} transport_lua_t;


/* native transport handling */
static int transport_create(transport_lua_t *t, char *err, size_t elen);
static int transport_listen(transport_lua_t *t, char *err, size_t elen);
static int transport_connect(transport_lua_t *t, char *err, size_t elen);
static transport_lua_t *transport_accept(transport_lua_t *lt);
static void transport_disconnect(transport_lua_t *t);

static void event_connect(mrp_transport_t *mt, void *user_data);
static void event_closed(mrp_transport_t *mt, int error, void *user_data);
static void event_recv(mrp_transport_t *mt, void *msg, void *user_data);
static void event_recvfrom(mrp_transport_t *mt, void *msg, mrp_sockaddr_t *addr,
                           socklen_t alen, void *user_data);

/* Lua transport handling */
static int transport_lua_create(lua_State *L);
static int transport_lua_listen(lua_State *L);
static int transport_lua_connect(lua_State *L);
static int transport_lua_accept(lua_State *L);
static void transport_lua_destroy(void *data);
static int transport_lua_disconnect(lua_State *L);
static void transport_lua_changed(void *data, lua_State *L, int member);
static ssize_t transport_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                      size_t size, lua_State *L, void *data);


/*
 * Lua transport class
 */

#define OFFS(m) MRP_OFFSET(transport_lua_t, m)
#define CB(m)   OFFS(callback.m)
#define RO      MRP_LUA_CLASS_READONLY
#define NOTIFY  MRP_LUA_CLASS_NOTIFY
#define NOFLAGS MRP_LUA_CLASS_NOFLAGS

MRP_LUA_METHOD_LIST_TABLE(transport_lua_methods,
                          MRP_LUA_METHOD_CONSTRUCTOR(transport_lua_create)
                          MRP_LUA_METHOD(listen    , transport_lua_listen )
                          MRP_LUA_METHOD(connect   , transport_lua_connect)
                          MRP_LUA_METHOD(accept    , transport_lua_accept)
                          MRP_LUA_METHOD(disconnect, transport_lua_disconnect));

MRP_LUA_METHOD_LIST_TABLE(transport_lua_overrides,
                          MRP_LUA_OVERRIDE_CALL     (transport_lua_create));

MRP_LUA_MEMBER_LIST_TABLE(transport_lua_members,
    MRP_LUA_CLASS_LFUNC  ("connect" , CB(connect)   , NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_LFUNC  ("closed"  , CB(closed)    , NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_LFUNC  ("recv"    , CB(recv)      , NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_LFUNC  ("recvfrom", CB(recvfrom)  , NULL, NULL, NOTIFY)
    MRP_LUA_CLASS_ANY    ("data"    , OFFS(data)    , NULL, NULL, NOTIFY   )
    MRP_LUA_CLASS_STRING ("address" , OFFS(address) , NULL, NULL, NOTIFY|RO)
    MRP_LUA_CLASS_STRING ("encoding", OFFS(encoding), NULL, NULL, NOTIFY|RO)
);

typedef enum {
    TRANSPORT_MEMBER_CONNECT,
    TRANSPORT_MEMBER_CLOSED,
    TRANSPORT_MEMBER_RECV,
    TRANSPORT_MEMBER_RECVFROM,
    TRANSPORT_MEMBER_DATA,
    TRANSPORT_MEMBER_ADDRESS,
    TRANSPORT_MEMBER_ENCODING,
} transport_member_t;


MRP_LUA_DEFINE_CLASS(transport, lua, transport_lua_t, transport_lua_destroy,
                     transport_lua_methods, transport_lua_overrides,
                     transport_lua_members, NULL, transport_lua_changed,
                     transport_lua_tostring, NULL, MRP_LUA_CLASS_EXTENSIBLE);

MRP_LUA_CLASS_CHECKER(transport_lua_t, transport_lua, TRANSPORT_LUA_CLASS);


static int set_address(transport_lua_t *t, const char *address,
                       char *err, size_t elen, int overwrite)
{
    MRP_LUA_ERRUSE(err, elen);

    if (t->address != NULL) {
        if (t->address == address) {
            if (t->alen > 0 && t->atype != NULL)
                return 1;
        }
        else {
            if (!overwrite)
                return mrp_lua_error(-1, t->L,
                                     "address already set ('%s')", t->address);
        }
    }

    if (t->address != address) {
        mrp_free(t->address);
        t->address = NULL;
    }

    t->atype = NULL;
    t->alen  = 0;

    if (address == NULL)
        return 1;

    if ((t->address = mrp_strdup(address)) == NULL)
        return mrp_lua_error(-1, t->L,
                             "failed to store address '%s'", address);

    t->alen = mrp_transport_resolve(NULL, t->address, &t->addr,
                                    sizeof(t->addr), &t->atype);

    if (t->alen <= 0) {
        if (address != t->address)
            mrp_free(t->address);

        t->atype = NULL;
        t->alen  = 0;

        return mrp_lua_error(-1, t->L, "failed to resolve '%s'", address);
    }

    return 0;
}


static int transport_create(transport_lua_t *t, char *err, size_t elen)
{
    MRP_LUA_ERRUSE(err, elen);

    static mrp_transport_evt_t events = {
        { .recvcustom     = event_recv     },
        { .recvcustomfrom = event_recvfrom },
          .connection     = event_connect,
          .closed         = event_closed,
    };
    const char *opt, *val;
    int         flags;

    if (t->alen <= 0) {
        errno = EADDRNOTAVAIL;
        return mrp_lua_error(-1, t->L, "no address specified");
    }

    if (t->t != NULL)
        return 0;

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;
    t->t  = mrp_transport_create(t->ctx->ml, t->atype, &events, t, flags);

    if (t->t == NULL)
        return mrp_lua_error(-1, t->L, "failed to create transport");


    opt = MRP_WSCK_OPT_SENDMODE;
    val = MRP_WSCK_SENDMODE_TEXT;
    mrp_transport_setopt(t->t, opt, val);

    return 0;
}


static int transport_listen(transport_lua_t *t, char *err, size_t elen)
{
    MRP_LUA_ERRUSE(err, elen);

    if (t->alen <= 0) {
        errno = EADDRNOTAVAIL;
        return mrp_lua_error(-1, t->L, "no address specified");
    }

    if (transport_create(t, MRP_LUA_ERRPASS) < 0)
        return -1;

    if (!mrp_transport_bind(t->t, &t->addr, t->alen) ||
        !mrp_transport_listen(t->t, 0))
        return mrp_lua_error(-1, t->L, "failed to bind transport");

    return 0;
}


static int transport_connect(transport_lua_t *t, char *err, size_t elen)
{
    MRP_LUA_ERRUSE(err, elen);

    const char *opt, *val;

    if (t->alen <= 0) {
        errno = EADDRNOTAVAIL;
        return mrp_lua_error(-1, t->L, "no address specified");
    }

    if (t->t != NULL) {
        errno = EISCONN;
        return mrp_lua_error(-1, t->L, "transport already active");
    }

    if (transport_create(t, MRP_LUA_ERRPASS) < 0)
        return mrp_lua_error(-1, t->L, "failed to connect transport to %s",
                             t->address);

    opt = MRP_WSCK_OPT_SENDMODE;
    val = MRP_WSCK_SENDMODE_TEXT;
    mrp_transport_setopt(t->t, opt, val);

    if (!mrp_transport_connect(t->t, &t->addr, t->alen)) {
        mrp_transport_destroy(t->t);
        t->t = NULL;

        return mrp_lua_error(-1, t->L, "failed to connect transport");
    }

    return 0;
}


static transport_lua_t *transport_accept(transport_lua_t *lt)
{
    transport_lua_t *t;

    t = (transport_lua_t *)mrp_lua_create_object(lt->L, TRANSPORT_LUA_CLASS,
                                                 NULL, 0);

    t->L   = lt->L;
    t->ctx = lt->ctx;
    t->callback.connect  = LUA_NOREF;
    t->callback.closed   = LUA_NOREF;
    t->callback.recv     = LUA_NOREF;
    t->callback.recvfrom = LUA_NOREF;
    t->data              = LUA_NOREF;

    t->t = mrp_transport_accept(lt->t, t, MRP_TRANSPORT_REUSEADDR);

    if (t->t != NULL) {
        t->callback.recv = mrp_lua_object_getref(lt, t, t->L,lt->callback.recv);
        t->data          = mrp_lua_object_getref(lt, t, t->L,lt->data);

        return t;
    }

    /* XXX TODO
     * Hmm, is it enough to just wait for the next gc cycle, or
     * should we actively do something to destroy the object ?
     */

    return NULL;
}


static void transport_disconnect(transport_lua_t *t)
{
    mrp_transport_disconnect(t->t);
    mrp_transport_destroy(t->t);
    t->t = NULL;
}




static void transport_lua_changed(void *data, lua_State *L, int member)
{
    MRP_LUA_ERRBUF();

    transport_lua_t *t = (transport_lua_t *)data;

    MRP_UNUSED(L);

    mrp_debug("member <transport <%s> %p(%p)>.%s changed",
              t->address ? t->address : "no address", t, t->t,
              transport_lua_members[member].name);

    switch (member) {
    case TRANSPORT_MEMBER_CONNECT:
    case TRANSPORT_MEMBER_CLOSED:
    case TRANSPORT_MEMBER_RECV:
    case TRANSPORT_MEMBER_RECVFROM:
    case TRANSPORT_MEMBER_DATA:
        break;

    case TRANSPORT_MEMBER_ADDRESS:
        if (set_address(t, t->address, MRP_LUA_ERRPASS, t->t == NULL) < 0)
            mrp_lua_error(-1, L, "%s", MRP_LUA_ERR);
        return;

    case TRANSPORT_MEMBER_ENCODING:
        break;

    default:
        break;
    }
}


static int transport_lua_create(lua_State *L)
{
    MRP_LUA_ERRBUF();

    mrp_context_t   *ctx  = mrp_lua_get_murphy_context();
    int              narg = lua_gettop(L);
    transport_lua_t *t;

    if (ctx == NULL)
        return mrp_lua_error(-1, L, "failed to get murphy context");

    t = (transport_lua_t *)mrp_lua_create_object(L, TRANSPORT_LUA_CLASS,
                                                 NULL, 0);
    t->L   = L;
    t->ctx = ctx;

    t->callback.connect  = LUA_NOREF;
    t->callback.closed   = LUA_NOREF;
    t->callback.recv     = LUA_NOREF;
    t->callback.recvfrom = LUA_NOREF;
    t->data              = LUA_NOREF;

    switch (narg) {
    case 1:
        break;
    case 2:
        if (mrp_lua_init_members(t, L, -2, MRP_LUA_ERRPASS) != 1)
            return mrp_lua_error(-1, L, "failed to initialize transport (%s)",
                                 MRP_LUA_ERR);
        break;
    default:
        return mrp_lua_error(-1, L, "expected 0 or 1 arguments, got %d", narg);
    }

    mrp_lua_push_object(L, t);

    return 1;
}


static int transport_lua_listen(lua_State *L)
{
    MRP_LUA_ERRUSE(NULL, 0);

    transport_lua_t *t = transport_lua_check(L, 1);
    int              narg;

    if ((narg = lua_gettop(L)) != 1)
        return mrp_lua_error(-1, L, "listen takes no arguments, got %d",
                             narg - 1);

    return transport_listen(t, MRP_LUA_ERRPASS);
}


static int transport_lua_connect(lua_State *L)
{
    MRP_LUA_ERRBUF();

    transport_lua_t *t    = transport_lua_check(L, 1);
    int              narg = lua_gettop(L);

    if (t->alen <= 0 || t->atype == NULL)
        return mrp_lua_error(-1, L, "can't connect, no address set");

    if (narg != 1)
        return mrp_lua_error(-1, L, "connect takes no arguments, %d given",
                             narg - 1);

    if (transport_connect(t, MRP_LUA_ERRPASS) < 0)
        return mrp_lua_error(-1, L, "connection failed");

    return 0;
}


static int transport_lua_accept(lua_State *L)
{
    MRP_LUA_ERRBUF();

    transport_lua_t *lt   = transport_lua_check(L, 1);
    int              narg = lua_gettop(L);
    transport_lua_t *t;

    if (narg != 1)
        return mrp_lua_error(-1, L, "disconnect takes no arguments, got %d",
                             narg - 1);

    t = transport_accept(lt);

    if (t != NULL) {
        mrp_lua_push_object(L, t);
        return 1;
    }

    /* XXX TODO
     * Hmm, is it enough to just wait for the next gc cycle, or
     * should we actively do something to destroy the object ?
     */

    return mrp_lua_error(-1, L, "failed to accept connection");
}


static int transport_lua_disconnect(lua_State *L)
{
    MRP_LUA_ERRBUF();

    transport_lua_t *t    = transport_lua_check(L, 1);
    int              narg = lua_gettop(L);

    if (narg != 1)
        return mrp_lua_error(-1, L, "disconnect takes no arguments, got %d",
                             narg - 1);

    transport_disconnect(t);

    return 0;
}


static void transport_lua_destroy(void *data)
{
    transport_lua_t *t = (transport_lua_t *)data;

    mrp_transport_disconnect(t->t);
    t->t = NULL;
    mrp_free(t->address);
    t->address = NULL;

    mrp_lua_object_unref_value(t, t->L, t->callback.connect);
    mrp_lua_object_unref_value(t, t->L, t->callback.closed);
    mrp_lua_object_unref_value(t, t->L, t->callback.recv);
    mrp_lua_object_unref_value(t, t->L, t->callback.recvfrom);
    mrp_lua_object_unref_value(t, t->L, t->data);
    t->callback.connect  = LUA_NOREF;
    t->callback.closed   = LUA_NOREF;
    t->callback.recv     = LUA_NOREF;
    t->callback.recvfrom = LUA_NOREF;
    t->data              = LUA_NOREF;
}


static ssize_t transport_lua_tostring(mrp_lua_tostr_mode_t mode, char *buf,
                                      size_t size, lua_State *L, void *data)
{
    transport_lua_t *t = (transport_lua_t *)data;

    MRP_UNUSED(L);

    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_LUA:
    default:
        return snprintf(buf, size, "{%stransport <%s> %p}",
                        t->t && t->t->connected ? "connected" : "",
                        t->address ? t->address : "no address", t->t);
    }
}


static void event_connect(mrp_transport_t *mt, void *user_data)
{
    transport_lua_t *t = (transport_lua_t *)user_data;
    int              top;

    MRP_UNUSED(mt);

    mrp_debug("incoming connection on <transport <%s> %p(%p)>",
              t->address ? t->address : "no address", t, t->t);

    top = lua_gettop(t->L);

    if (mrp_lua_object_deref_value(t, t->L, t->callback.connect, false)) {
        mrp_lua_push_object(t->L, t);
        lua_pushliteral(t->L, "<remote address should be here>");
        mrp_lua_object_deref_value(t, t->L, t->data, true);

        if (lua_pcall(t->L, 3, 0, 0) != 0)
            mrp_log_error("failed to invoke transport connect callback");
    }

    lua_settop(t->L, top);
}


static void event_closed(mrp_transport_t *mt, int error, void *user_data)
{
    transport_lua_t *t = (transport_lua_t *)user_data;
    int              top;

    MRP_UNUSED(mt);

    mrp_debug("<transport <%s> %p(%p)> has been closed",
              t->address ? t->address : "no address", t, t->t);

    top = lua_gettop(t->L);

    if (mrp_lua_object_deref_value(t, t->L, t->callback.closed, false)) {
        mrp_lua_push_object(t->L, t);
        lua_pushinteger(t->L, error);
        mrp_lua_object_deref_value(t, t->L, t->data, true);

        if (lua_pcall(t->L, 3, 0, 0) != 0)
            mrp_log_error("failed to invoke transport closed callback");

        mrp_transport_destroy(t->t);
        t->t = NULL;
    }

    lua_settop(t->L, top);
}


static void event_recv(mrp_transport_t *mt, void *msg, void *user_data)
{
    transport_lua_t *t = (transport_lua_t *)user_data;
    int              top;

    MRP_UNUSED(mt);

    mrp_debug("received message on <transport <%s> %p(%p)>",
              t->address ? t->address : "no address", t, t->t);

    top = lua_gettop(t->L);

    if (mrp_lua_object_deref_value(t, t->L, t->callback.recv, false)) {
        mrp_lua_push_object(t->L, t);
        mrp_json_lua_push(t->L, msg);
        mrp_lua_object_deref_value(t, t->L, t->data, true);

        if (lua_pcall(t->L, 3, 0, 0) != 0)
            mrp_log_error("failed to invoke transport recv callback");
    }

    lua_settop(t->L, top);
}


static void event_recvfrom(mrp_transport_t *mt, void *msg, mrp_sockaddr_t *addr,
                           socklen_t alen, void *user_data)
{
    transport_lua_t *t = (transport_lua_t *)user_data;
    int              top;

    MRP_UNUSED(mt);
    MRP_UNUSED(addr);
    MRP_UNUSED(alen);

    mrp_debug("received message on <transport <%s> %p(%p)>",
              t->address ? t->address : "no address", t, t->t);

    top = lua_gettop(t->L);

    if (mrp_lua_object_deref_value(t, t->L, t->callback.recvfrom, false)) {
        mrp_lua_push_object(t->L, t);
        mrp_json_lua_push(t->L, msg);
        lua_pushliteral(t->L, "<remote address should be here>");
        mrp_lua_object_deref_value(t, t->L, t->data, true);

        if (lua_pcall(t->L, 4, 0, 0) != 0)
            mrp_log_error("failed to invoke transport recvfrom callback");
    }

    lua_settop(t->L, top);
}


MURPHY_REGISTER_LUA_BINDINGS(murphy, TRANSPORT_LUA_CLASS,
                             { "Transport", transport_lua_create });
