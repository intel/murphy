/*
 * Copyright (c) 2012, Intel Corporation
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
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>
#include <murphy/common/dbus-sdbus.h>
#include <murphy/common/dbus-transport.h>

#define DBUS  "dbus"
#define DBUSL 4

#define TRANSPORT_PATH       "/murphy/transport"
#define TRANSPORT_INTERFACE  "Murphy.Transport"
#define TRANSPORT_MESSAGE    "DeliverMessage"
#define TRANSPORT_DATA       "DeliverData"
#define TRANSPORT_RAW        "DeliverRaw"
#define TRANSPORT_METHOD     "DeliverMessage"

#define ANY_ADDRESS          "any"

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    mrp_dbus_t     *dbus;                /* D-BUS connection */
    int             bound : 1;           /* whether bound to an address */
    int             peer_resolved : 1;   /* connected and peer name known */
    mrp_dbusaddr_t  local;               /* address we're bound to */
    mrp_dbusaddr_t  remote;              /* address we're connected to */
} dbus_t;


static uint32_t nauto;                   /* for autobinding */


static int dbus_msg_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data);
static int dbus_data_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data);
static int dbus_raw_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data);

static void peer_state_cb(mrp_dbus_t *dbus, const char *name, int up,
                          const char *owner, void *user_data);

static mrp_dbus_msg_t *msg_encode(mrp_dbus_t *dbus, const char *destination,
                                  const char *path, const char *interface,
                                  const char *member, const char *sender_id,
                                  mrp_msg_t *msg);
static mrp_msg_t *msg_decode(mrp_dbus_msg_t *msg, const char **sender_id);

static mrp_dbus_msg_t *data_encode(mrp_dbus_t *dbus, const char *destination,
                                   const char *path, const char *interface,
                                   const char *member, const char *sender_id,
                                   void *data, uint16_t tag);
static void *data_decode(mrp_dbus_msg_t *msg, uint16_t *tag,
                         const char **sender_id);

static mrp_dbus_msg_t *raw_encode(mrp_dbus_t *dbus, const char *destination,
                                  const char *path, const char *interface,
                                  const char *member, const char *sender_id,
                                  void *data, size_t size);
static void *raw_decode(mrp_dbus_msg_t *msg, size_t *sizep,
                        const char **sender_id);


static socklen_t parse_address(const char *str, mrp_dbusaddr_t *addr,
                               socklen_t size)
{
    const char *p, *e;
    char       *q;
    size_t      l, n;

    if (size < sizeof(*addr)) {
        errno = EINVAL;
        return FALSE;
    }

    if (strncmp(str, DBUS":", DBUSL + 1))
        return 0;
    else
        str += DBUSL + 1;

    /*
     * The format of the address is
     *     dbus:[bus-address]@address/path
     * eg.
     *     dbus:[session]@:1.33/client1, or
     *     dbus:[unix:abstract=/tmp/dbus-Xx2Kpi...a572]@:1.33/client1
     */

    p = str;
    q = addr->db_fqa;
    l = sizeof(addr->db_fqa);

    /* get bus address */
    if (*p != '[') {
        errno = EINVAL;
        return 0;
    }
    else
        p++;

    e = strchr(p, ']');

    if (e == NULL) {
        errno = EINVAL;
        return 0;
    }

    n = e - p;
    if (n >= l) {
        errno = ENAMETOOLONG;
        return 0;
    }

    /* save bus address */
    strncpy(q, p, n);
    q[n] = '\0';
    addr->db_bus = q;

    q += n + 1;
    l -= n + 1;
    p  = e + 1;

    /* get (local or remote) address on bus */
    if (*p != '@')
        addr->db_addr = ANY_ADDRESS;
    else {
        p++;
        e = strchr(p, '/');

        if (e == NULL) {
            errno = EINVAL;
            return 0;
        }

        n = e - p;
        if (n >= l) {
            errno = ENAMETOOLONG;
            return 0;
        }

        /* save address on bus */
        strncpy(q, p, n);
        q[n] = '\0';
        addr->db_addr = q;

        q += n + 1;
        l -= n + 1;
        p  = e;
    }

    /* get object path */
    if (*p != '/') {
        errno = EINVAL;
        return 0;
    }

    n = snprintf(q, l, "%s%s", TRANSPORT_PATH, p);
    if (n >= l) {
        errno = ENAMETOOLONG;
        return 0;
    }

    addr->db_path   = q;
    addr->db_family = MRP_AF_DBUS;

    return sizeof(*addr);
}


static mrp_dbusaddr_t *copy_address(mrp_dbusaddr_t *dst, mrp_dbusaddr_t *src)
{
    char   *p, *q;
    size_t  l, n;

    dst->db_family = src->db_family;

    /* copy bus address */
    p = src->db_bus;
    q = dst->db_fqa;
    l = sizeof(dst->db_fqa);

    n = strlen(p);
    if (l < n + 1) {
        errno = EOVERFLOW;
        return NULL;
    }

    dst->db_bus = q;
    memcpy(q, p, n + 1);
    q += n + 1;
    l -= n + 1;

    /* copy address */
    p = src->db_addr;
    n = strlen(p);
    if (l < n + 1) {
        errno = EOVERFLOW;
        return NULL;
    }

    dst->db_addr = q;
    memcpy(q, p, n + 1);
    q += n + 1;
    l -= n + 1;

    /* copy path */
    p = src->db_path;
    n = strlen(p);
    if (l < n + 1) {
        errno = EOVERFLOW;
        return NULL;
    }

    dst->db_path = q;
    memcpy(q, p, n + 1);
    q += n + 1;
    l -= n + 1;

    return dst;
}


static inline int check_address(mrp_sockaddr_t *addr, socklen_t addrlen)
{
    mrp_dbusaddr_t *a = (mrp_dbusaddr_t *)addr;

    return (a && a->db_family == MRP_AF_DBUS && addrlen == sizeof(*a));
}


static size_t peer_address(mrp_sockaddr_t *addrp, const char *sender,
                           const char *path)
{
    mrp_dbusaddr_t *addr = (mrp_dbusaddr_t *)addrp;
    const char     *p;
    char           *q;
    int             l, n;

    q = addr->db_fqa;
    l = sizeof(addr->db_fqa);
    p = ANY_ADDRESS;
    n = 3;

    addr->db_bus = q;
    memcpy(q, p, n + 1);
    q += n + 1;
    l -= n + 1;

    addr->db_addr = q;
    p = sender;
    n = strlen(sender);

    if (l < n + 1)
        return 0;

    memcpy(q, p, n + 1);
    q += n + 1;
    l -= n + 1;

    addr->db_path = q;
    p = path;
    n = strlen(p);

    if (l < n + 1)
        return 0;

    memcpy(q, p, n + 1);

    return sizeof(addrp);
}


static socklen_t dbus_resolve(const char *str, mrp_sockaddr_t *addr,
                              socklen_t size, const char **typep)
{
    socklen_t len;

    len = parse_address(str, (mrp_dbusaddr_t *)addr, size);

    if (len > 0) {
        if (typep != NULL)
            *typep = DBUS;
    }

    return len;
}


static int dbus_open(mrp_transport_t *mt)
{
    MRP_UNUSED(mt);

    return TRUE;
}


static int dbus_createfrom(mrp_transport_t *mt, void *conn)
{
    dbus_t     *t    = (dbus_t *)mt;
    mrp_dbus_t *dbus = (mrp_dbus_t *)conn;

    t->dbus = mrp_dbus_ref(dbus);

    if (t->dbus != NULL)
        return TRUE;
    else
        return FALSE;
}


static int dbus_bind(mrp_transport_t *mt, mrp_sockaddr_t *addrp,
                     socklen_t addrlen)
{
    dbus_t          *t    = (dbus_t *)mt;
    mrp_dbus_t      *dbus = NULL;
    mrp_dbusaddr_t  *addr = (mrp_dbusaddr_t *)addrp;
    int            (*cb)(mrp_dbus_t *, mrp_dbus_msg_t *, void *);
    const char      *method;

    if (t->bound) {
        errno = EINVAL;
        goto fail;
    }

    if (!check_address(addrp, addrlen)) {
        errno = EINVAL;
        goto fail;
    }

    if (t->dbus == NULL) {
        dbus = mrp_dbus_connect(t->ml, addr->db_bus, NULL);

        if (dbus == NULL) {
            errno = ECONNRESET;
            goto fail;
        }
        else {
            t->dbus = dbus;

            if (addr->db_addr != NULL && strcmp(addr->db_addr, ANY_ADDRESS)) {
                if (!mrp_dbus_acquire_name(t->dbus, addr->db_addr, NULL)) {
                    errno = EADDRINUSE; /* XXX TODO, should check error... */
                    goto fail;
                }
            }
        }
    }
    else {
        /* XXX TODO: should check given address against address of the bus */
    }

    copy_address(&t->local, addr);

    switch (t->mode) {
    case MRP_TRANSPORT_MODE_DATA:
        method = TRANSPORT_DATA;
        cb     = dbus_data_cb;
        break;
    case MRP_TRANSPORT_MODE_RAW:
        method = TRANSPORT_RAW;
        cb     = dbus_raw_cb;
        break;
    case MRP_TRANSPORT_MODE_MSG:
        method = TRANSPORT_MESSAGE;
        cb     = dbus_msg_cb;
        break;
    default:
        errno = EPROTOTYPE;
        goto fail;
    }

    if (!mrp_dbus_export_method(t->dbus, addr->db_path, TRANSPORT_INTERFACE,
                                method, cb, t)) {
        errno = EIO;
        goto fail;
    }
    else {
        t->bound = TRUE;
        return TRUE;
    }

 fail:
    if (dbus != NULL) {
        mrp_dbus_unref(dbus);
        t->dbus = NULL;
    }

    return FALSE;
}


static int dbus_autobind(mrp_transport_t *mt, mrp_sockaddr_t *addrp)
{
    mrp_dbusaddr_t *a = (mrp_dbusaddr_t *)addrp;
    char            astr[MRP_SOCKADDR_SIZE];
    mrp_sockaddr_t  addr;
    socklen_t       alen;

    snprintf(astr, sizeof(astr), "dbus:[%s]/auto/%u", a->db_bus, nauto++);

    alen = dbus_resolve(astr, &addr, sizeof(addr), NULL);

    if (alen > 0)
        return dbus_bind(mt, &addr, alen);
    else
        return FALSE;
}


static void dbus_close(mrp_transport_t *mt)
{
    dbus_t         *t = (dbus_t *)mt;
    mrp_dbusaddr_t *addr;
    const char     *method;
    int           (*cb)(mrp_dbus_t *, mrp_dbus_msg_t *, void *);

    if (t->bound) {
        switch (t->mode) {
        case MRP_TRANSPORT_MODE_DATA:
            method = TRANSPORT_DATA;
            cb     = dbus_data_cb;
            break;
        case MRP_TRANSPORT_MODE_RAW:
            method = TRANSPORT_RAW;
            cb     = dbus_raw_cb;
            break;
        default:
        case MRP_TRANSPORT_MODE_MSG:
            method = TRANSPORT_MESSAGE;
            cb     = dbus_msg_cb;
        }

        addr = (mrp_dbusaddr_t *)&t->local;
        mrp_dbus_remove_method(t->dbus, addr->db_path, TRANSPORT_INTERFACE,
                               method, cb, t);
    }

    if (t->connected && t->remote.db_addr != NULL)
        mrp_dbus_forget_name(t->dbus, t->remote.db_addr, peer_state_cb, t);

    mrp_dbus_unref(t->dbus);
    t->dbus = NULL;
}


static int dbus_msg_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *dmsg, void *user_data)
{
    mrp_transport_t *mt = (mrp_transport_t *)user_data;
    dbus_t          *t  = (dbus_t *)mt;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    const char      *sender, *sender_path;
    mrp_msg_t       *msg;

    MRP_UNUSED(dbus);

    msg = msg_decode(dmsg, &sender_path);

    if (msg != NULL) {
        sender = mrp_dbus_msg_sender(dmsg);

        if (mt->connected) {
            if (!t->peer_resolved || !strcmp(t->remote.db_addr, sender))
                MRP_TRANSPORT_BUSY(mt, {
                        mt->evt.recvmsg(mt, msg, mt->user_data);
                    });
        }
        else {
            peer_address(&addr, sender, sender_path);
            alen = sizeof(addr);

            MRP_TRANSPORT_BUSY(mt, {
                    mt->evt.recvmsgfrom(mt, msg, &addr, alen, mt->user_data);
                });
        }

        mrp_msg_unref(msg);

        mt->check_destroy(mt);
    }
    else {
        mrp_log_error("Failed to decode message.");
    }

    return TRUE;
}


static int dbus_data_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *dmsg, void *user_data)
{
    mrp_transport_t *mt = (mrp_transport_t *)user_data;
    dbus_t          *t  = (dbus_t *)mt;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    const char      *sender, *sender_path;
    uint16_t         tag;
    void            *decoded;

    MRP_UNUSED(dbus);

    decoded = data_decode(dmsg, &tag, &sender_path);

    if (decoded != NULL) {
        sender = mrp_dbus_msg_sender(dmsg);

        if (mt->connected) {
            if (!t->peer_resolved || !strcmp(t->remote.db_addr, sender))
                MRP_TRANSPORT_BUSY(mt, {
                        mt->evt.recvdata(mt, decoded, tag, mt->user_data);
                    });
        }
        else {
            peer_address(&addr, sender, sender_path);
            alen = sizeof(addr);

            MRP_TRANSPORT_BUSY(mt, {
                    mt->evt.recvdatafrom(mt, decoded, tag, &addr, alen,
                                         mt->user_data);
                });
        }

        mt->check_destroy(mt);
    }
    else {
        mrp_log_error("Failed to decode custom data message.");
    }

    return TRUE;
}


static int dbus_raw_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *dmsg, void *user_data)
{
    mrp_transport_t *mt = (mrp_transport_t *)user_data;
    dbus_t          *t  = (dbus_t *)mt;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    const char      *sender, *sender_path;
    void            *data;
    size_t           size;

    MRP_UNUSED(dbus);

    data = raw_decode(dmsg, &size, &sender_path);

    if (data != NULL) {
        sender = mrp_dbus_msg_sender(dmsg);

        if (mt->connected) {
            if (!t->peer_resolved || !strcmp(t->remote.db_addr, sender))
                MRP_TRANSPORT_BUSY(mt, {
                        mt->evt.recvraw(mt, data, size, mt->user_data);
                    });
        }
        else {
            peer_address(&addr, sender, sender_path);
            alen = sizeof(addr);

            MRP_TRANSPORT_BUSY(mt, {
                    mt->evt.recvrawfrom(mt, data, size, &addr, alen,
                                        mt->user_data);
                });
        }

        mt->check_destroy(mt);
    }
    else {
        mrp_log_error("Failed to decode raw message.");
    }

    return TRUE;
}


static void peer_state_cb(mrp_dbus_t *dbus, const char *name, int up,
                          const char *owner, void *user_data)
{
    dbus_t         *t = (dbus_t *)user_data;
    mrp_sockaddr_t  addr;

    MRP_UNUSED(dbus);
    MRP_UNUSED(name);

    if (up) {
        peer_address(&addr, owner, t->remote.db_path);
        copy_address(&t->remote, (mrp_dbusaddr_t *)&addr);
        t->peer_resolved = TRUE;
    }
    else {
        /*
         * XXX TODO:
         *     It would be really tempting here to call
         *         mt->evt.closed(mt, ECONNRESET, mt->user_data)
         *     to notify the user about the fact our peer went down.
         *     However, that would not be in line with the other
         *     transports which call the closed event handler only
         *     upon foricble transport closes upon errors.
         *
         *     The transport interface abstraction (especially the
         *     available set of events) anyway needs some eyeballing,
         *     so the right thing to do might be to define a new event
         *     for disconnection and call the handler for that here...
         */
    }

}


static int dbus_connect(mrp_transport_t *mt, mrp_sockaddr_t *addrp,
                        socklen_t addrlen)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_dbusaddr_t *addr = (mrp_dbusaddr_t *)addrp;

    if (!check_address(addrp, addrlen)) {
        errno = EINVAL;
        return FALSE;
    }

    if (t->dbus == NULL) {
        t->dbus = mrp_dbus_connect(t->ml, addr->db_bus, NULL);

        if (t->dbus == NULL) {
            errno = ECONNRESET;
            return FALSE;
        }
    }
    else {
        /* XXX TODO: check given address against address of the bus */
    }

    if (!t->bound)
        if (!dbus_autobind(mt, addrp))
            return FALSE;

    if (mrp_dbus_follow_name(t->dbus, addr->db_addr, peer_state_cb, t)) {
        copy_address(&t->remote, addr);

        return TRUE;
    }
    else
        return FALSE;
}


static int dbus_disconnect(mrp_transport_t *mt)
{
    dbus_t *t = (dbus_t *)mt;

    if (t->connected && t->remote.db_addr != NULL) {
        mrp_dbus_forget_name(t->dbus, t->remote.db_addr, peer_state_cb, t);
        mrp_clear(&t->remote);
        t->peer_resolved = FALSE;
    }

    return TRUE;
}


static int dbus_sendmsgto(mrp_transport_t *mt, mrp_msg_t *msg,
                          mrp_sockaddr_t *addrp, socklen_t addrlen)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_dbusaddr_t *addr = (mrp_dbusaddr_t *)addrp;
    mrp_dbus_msg_t *m;
    int             success;

    if (check_address(addrp, addrlen)) {
        if (t->dbus == NULL && !dbus_autobind(mt, addrp))
                return FALSE;

        m = msg_encode(t->dbus, addr->db_addr, addr->db_path,
                       TRANSPORT_INTERFACE, TRANSPORT_MESSAGE,
                       t->local.db_path, msg);

        if (m != NULL) {
            if (mrp_dbus_send_msg(t->dbus, m))
                success = TRUE;
            else {
                errno   = ECOMM;
                success = FALSE;
            }

            mrp_dbus_msg_unref(m);
        }
        else
            success = FALSE;
    }
    else {
        errno   = EINVAL;
        success = FALSE;
    }

    return success;
}


static int dbus_sendmsg(mrp_transport_t *mt, mrp_msg_t *msg)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_sockaddr_t *addr = (mrp_sockaddr_t *)&t->remote;
    socklen_t       alen = sizeof(t->remote);

    return dbus_sendmsgto(mt, msg, addr, alen);
}


static int dbus_sendrawto(mrp_transport_t *mt, void *data, size_t size,
                          mrp_sockaddr_t *addrp, socklen_t addrlen)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_dbusaddr_t *addr = (mrp_dbusaddr_t *)addrp;
    mrp_dbus_msg_t *m;
    int             success;


    MRP_UNUSED(mt);
    MRP_UNUSED(data);
    MRP_UNUSED(size);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    if (check_address(addrp, addrlen)) {
        if (t->dbus == NULL && !dbus_autobind(mt, addrp))
            return FALSE;

        m = raw_encode(t->dbus, addr->db_addr, addr->db_path,
                       TRANSPORT_INTERFACE, TRANSPORT_RAW,
                       t->local.db_path, data, size);

        if (m != NULL) {
            if (mrp_dbus_send_msg(t->dbus, m))
                success = TRUE;
            else {
                errno   = ECOMM;
                success = FALSE;
            }

            mrp_dbus_msg_unref(m);
        }
        else
            success = FALSE;
    }
    else {
        errno   = EINVAL;
        success = FALSE;
    }

    return success;
}


static int dbus_sendraw(mrp_transport_t *mt, void *data, size_t size)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_sockaddr_t *addr = (mrp_sockaddr_t *)&t->remote;
    socklen_t       alen = sizeof(t->remote);

    return dbus_sendrawto(mt, data, size, addr, alen);
}


static int dbus_senddatato(mrp_transport_t *mt, void *data, uint16_t tag,
                           mrp_sockaddr_t *addrp, socklen_t addrlen)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_dbusaddr_t *addr = (mrp_dbusaddr_t *)addrp;
    mrp_dbus_msg_t *m;
    int             success;

    if (check_address(addrp, addrlen)) {
        if (t->dbus == NULL && !dbus_autobind(mt, addrp))
            return FALSE;

        m = data_encode(t->dbus, addr->db_addr, addr->db_path,
                        TRANSPORT_INTERFACE, TRANSPORT_DATA,
                        t->local.db_path, data, tag);

        if (m != NULL) {
            if (mrp_dbus_send_msg(t->dbus, m))
                success = TRUE;
            else {
                errno   = ECOMM;
                success = FALSE;
            }

            mrp_dbus_msg_unref(m);
        }
        else
            success = FALSE;
    }
    else {
        errno   = EINVAL;
        success = FALSE;
    }

    return success;
}


static int dbus_senddata(mrp_transport_t *mt, void *data, uint16_t tag)
{
    dbus_t         *t    = (dbus_t *)mt;
    mrp_sockaddr_t *addr = (mrp_sockaddr_t *)&t->remote;
    socklen_t       alen = sizeof(t->remote);

    return dbus_senddatato(mt, data, tag, addr, alen);
}


static const char *get_array_signature(uint16_t type)
{
#define MAP(from, to)                                 \
    case MRP_MSG_FIELD_##from:                        \
        return MRP_DBUS_TYPE_##to##_AS_STRING;

    switch (type) {
        MAP(STRING, STRING);
        MAP(BOOL  , BOOLEAN);
        MAP(UINT8 , UINT16);
        MAP(SINT8 , INT16);
        MAP(UINT16, UINT16);
        MAP(SINT16, INT16);
        MAP(UINT32, UINT32);
        MAP(SINT32, INT32);
        MAP(UINT64, UINT64);
        MAP(SINT64, INT64);
        MAP(DOUBLE, DOUBLE);
        MAP(BLOB  , BYTE);
    default:
        return NULL;
    }
}


static mrp_dbus_msg_t *msg_encode(mrp_dbus_t *dbus, const char *destination,
                                  const char *path, const char *interface,
                                  const char *member, const char *sender_id,
                                  mrp_msg_t *msg)
{
    /*
     * Notes: There is a type mismatch between our and DBUS types for
     *        8-bit integers (D-BUS does not have a signed 8-bit type)
     *        and boolean types (D-BUS has uint32_t booleans, C99 fails
     *        to specify the type and gcc uses a signed char). The
     *        QUIRKY versions of the macros take care of these mismatches.
     */

#define BASIC_SIMPLE(_i, _mtype, _dtype, _val)                            \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = &(_val);                                               \
                                                                          \
            if (!mrp_dbus_msg_append_basic(_i, type, vptr))               \
                goto fail;                                                \
            break

#define BASIC_STRING(_i, _mtype, _dtype, _val)                            \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = (_val);                                                \
                                                                          \
            if (!mrp_dbus_msg_append_basic(_i, type, vptr))               \
                goto fail;                                                \
            break

#define BASIC_QUIRKY(_i, _mtype, _dtype, _mval, _dval)                    \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type  = MRP_DBUS_TYPE_##_dtype;                               \
            _dval = _mval;                                                \
            vptr  = &_dval;                                               \
                                                                          \
            if (!mrp_dbus_msg_append_basic(_i, type, vptr))               \
                goto fail;                                                \
            break

#define ARRAY_SIMPLE(_i, _mtype, _dtype, _val)                            \
                case MRP_MSG_FIELD_##_mtype:                              \
                    type = MRP_DBUS_TYPE_##_dtype;                        \
                    vptr = &_val;                                         \
                                                                          \
                    if (!mrp_dbus_msg_append_basic(_i, type, vptr))       \
                        goto fail;                                        \
                    break

#define ARRAY_STRING(_i, _mtype, _dtype, _val)                            \
                case MRP_MSG_FIELD_##_mtype:                              \
                    type = MRP_DBUS_TYPE_##_dtype;                        \
                    vptr = _val;                                          \
                                                                          \
                    if (!mrp_dbus_msg_append_basic(_i, type, vptr))       \
                        goto fail;                                        \
                    break

#define ARRAY_QUIRKY(_i, _mtype, _dtype, _mvar, _dvar)                    \
                case MRP_MSG_FIELD_##_mtype:                              \
                    type  = MRP_DBUS_TYPE_##_dtype;                       \
                    _dvar = _mvar;                                        \
                    vptr  = &_dvar;                                       \
                                                                          \
                    if (!mrp_dbus_msg_append_basic(_i, type, vptr))       \
                        goto fail;                                        \
                    break

    mrp_dbus_msg_t  *m;
    mrp_list_hook_t *p, *n;
    mrp_msg_field_t *f;
    uint16_t         base;
    uint32_t         asize, i;
    const char      *sig;
    int              type, len;
    void            *vptr;
    uint32_t         bln;
    uint16_t         u16, blb;
    int16_t          s16;

    m = mrp_dbus_msg_method_call(dbus, destination, path, interface, member);

    if (m == NULL)
        return NULL;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_OBJECT_PATH,
                                   (void *)sender_id))
        goto fail;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &msg->nfield))
        goto fail;

    mrp_list_foreach(&msg->fields, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);

        if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &f->tag) ||
            !mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &f->type))
            goto fail;

        switch (f->type) {
            BASIC_STRING(m, STRING, STRING , f->str);
            BASIC_QUIRKY(m, BOOL  , BOOLEAN, f->bln, bln);
            BASIC_QUIRKY(m, UINT8 , UINT16 , f->u8 , u16);
            BASIC_QUIRKY(m, SINT8 ,  INT16 , f->s8 , s16);
            BASIC_SIMPLE(m, UINT16, UINT16 , f->u16);
            BASIC_SIMPLE(m, SINT16,  INT16 , f->s16);
            BASIC_SIMPLE(m, UINT32, UINT32 , f->u32);
            BASIC_SIMPLE(m, SINT32,  INT32 , f->s32);
            BASIC_SIMPLE(m, UINT64, UINT64 , f->u64);
            BASIC_SIMPLE(m, SINT64,  INT64 , f->s64);
            BASIC_SIMPLE(m, DOUBLE, DOUBLE , f->dbl);

        case MRP_MSG_FIELD_BLOB:
            vptr  = f->blb;
            len   = (int)f->size[0];
            sig   = get_array_signature(f->type);
            asize = len;
            if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT32, &asize))
                goto fail;

            if (!mrp_dbus_msg_open_container(m, MRP_DBUS_TYPE_ARRAY, NULL))
                goto fail;

            for (i = 0; i < asize; i++) {
                blb = ((uint8_t *)f->blb)[i];
                if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &blb))
                    goto fail;
            }

            if (!mrp_dbus_msg_close_container(m))
                goto fail;
            break;

        default:
            if (f->type & MRP_MSG_FIELD_ARRAY) {
                base  = f->type & ~(MRP_MSG_FIELD_ARRAY);
                asize = f->size[0];
                sig   = get_array_signature(base);

                if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT32, &asize))
                    goto fail;

                if (!mrp_dbus_msg_open_container(m, MRP_DBUS_TYPE_ARRAY, sig))
                    goto fail;

                for (i = 0; i < asize; i++) {
                    switch (base) {
                        ARRAY_STRING(m, STRING, STRING , f->astr[i]);
                        ARRAY_QUIRKY(m, BOOL  , BOOLEAN, f->abln[i], bln);
                        ARRAY_QUIRKY(m, UINT8 , UINT16 , f->au8[i] , u16);
                        ARRAY_QUIRKY(m, SINT8 ,  INT16 , f->as8[i] , s16);
                        ARRAY_SIMPLE(m, UINT16, UINT16 , f->au16[i]);
                        ARRAY_SIMPLE(m, SINT16,  INT16 , f->as16[i]);
                        ARRAY_SIMPLE(m, UINT32, UINT32 , f->au32[i]);
                        ARRAY_SIMPLE(m, SINT32,  INT32 , f->as32[i]);
                        ARRAY_SIMPLE(m, UINT64, UINT64 , f->au64[i]);
                        ARRAY_SIMPLE(m, DOUBLE, DOUBLE , f->adbl[i]);

                    case MRP_MSG_FIELD_BLOB:
                        goto fail;

                    default:
                        goto fail;
                    }
                }

                if (!mrp_dbus_msg_close_container(m))
                    goto fail;
            }
            else
                goto fail;
        }
    }

    return m;

 fail:
    if (m != NULL)
        mrp_dbus_msg_unref(m);

    errno = ECOMM;

    return FALSE;

#undef BASIC_SIMPLE
#undef BASIC_STRING
#undef BASIC_QUIRKY
#undef ARRAY_SIMPLE
#undef ARRAY_STRING
#undef ARRAY_QUIRKY
}


static mrp_msg_t *msg_decode(mrp_dbus_msg_t *m, const char **sender_id)
{
#define BASIC_SIMPLE(_i, _mtype, _dtype, _var)                            \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(_i, MRP_DBUS_TYPE_##_dtype,      \
                                         &(_var)))                        \
                goto fail;                                                \
                                                                          \
            if (!mrp_msg_append(msg, tag, type, (_var)))                  \
                goto fail;                                                \
            break

#define BASIC_QUIRKY(_i, _mtype, _dtype, _mvar, _dvar)                    \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(_i, MRP_DBUS_TYPE_##_dtype,      \
                                         &(_dvar)))                       \
                goto fail;                                                \
                                                                          \
            _mvar = _dvar;                                                \
            if (!mrp_msg_append(msg, tag, type, (_mvar)))                 \
                goto fail;                                                \
            break

#define ARRAY_SIMPLE(_i, _mtype, _dtype, _var)                            \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(_i, MRP_DBUS_TYPE_##_dtype,      \
                                         &(_var)))                        \
                goto fail;                                                \
            break

#define ARRAY_QUIRKY(_i, _mtype, _dtype, _mvar, _dvar)                    \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(_i, MRP_DBUS_TYPE_##_dtype,      \
                                         &(_dvar)))                       \
                goto fail;                                                \
                                                                          \
            _mvar = _dvar;                                                \
            break

#define APPEND_ARRAY(_type, _var)                                         \
                case MRP_MSG_FIELD_##_type:                               \
                    if (!mrp_msg_append(msg, tag,                         \
                                        MRP_MSG_FIELD_ARRAY |             \
                                        MRP_MSG_FIELD_##_type,            \
                                        n, _var))                         \
                        goto fail;                                        \
                    break

    mrp_msg_t       *msg;
    mrp_msg_value_t  v;
    uint16_t         u16;
    int16_t          s16;
    uint32_t         u32;
    uint16_t         nfield, tag, type, base, i;
    uint32_t         n, j;
    int              asize;
    const char      *sender, *sig;

    msg = NULL;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_OBJECT_PATH, &sender))
        goto fail;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &nfield))
        goto fail;

    msg = mrp_msg_create_empty();

    if (msg == NULL)
        goto fail;

    for (i = 0; i < nfield; i++) {
        if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &tag))
            goto fail;

        if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &type))
            goto fail;

        switch (type) {
            BASIC_SIMPLE(m, STRING, STRING , v.str);
            BASIC_QUIRKY(m, BOOL  , BOOLEAN, v.bln, u32);
            BASIC_QUIRKY(m, UINT8 , UINT16 , v.u8 , u16);
            BASIC_QUIRKY(m, SINT8 ,  INT16 , v.s8 , s16);
            BASIC_SIMPLE(m, UINT16, UINT16 , v.u16);
            BASIC_SIMPLE(m, SINT16,  INT16 , v.s16);
            BASIC_SIMPLE(m, UINT32, UINT32 , v.u32);
            BASIC_SIMPLE(m, SINT32,  INT32 , v.s32);
            BASIC_SIMPLE(m, UINT64, UINT64 , v.u64);
            BASIC_SIMPLE(m, SINT64,  INT64 , v.s64);
            BASIC_SIMPLE(m, DOUBLE, DOUBLE , v.dbl);

        case MRP_MSG_FIELD_BLOB:
            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT32, &n))
                goto fail;

            {
                uint8_t blb[n];

                if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, NULL))
                    goto fail;

                for (j = 0; j < n; j++) {
                    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_BYTE,
                                                 blb + j))
                        goto fail;
                }

                if (!mrp_dbus_msg_exit_container(m))
                    goto fail;

                asize = n;
                if (!mrp_msg_append(msg, tag, type, asize, blb))
                    goto fail;
            }
            break;

        default:
            if (!(type & MRP_MSG_FIELD_ARRAY))
                goto fail;

            base = type & ~(MRP_MSG_FIELD_ARRAY);

            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT32, &n))
                goto fail;

            sig = get_array_signature(base);
            if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, sig))
                goto fail;

            {
                char     *astr[n];
                uint32_t  dbln[n];
                bool      abln[n];
                uint8_t   au8 [n];
                int8_t    as8 [n];
                uint16_t  au16[n];
                int16_t   as16[n];
                uint32_t  au32[n];
                int32_t   as32[n];
                uint64_t  au64[n];
                int64_t   as64[n];
                double    adbl[n];

                for (j = 0; j < n; j++) {
                    switch (base) {
                        ARRAY_SIMPLE(m, STRING, STRING , astr[j]);
                        ARRAY_QUIRKY(m, BOOL  , BOOLEAN, abln[j], dbln[j]);
                        ARRAY_QUIRKY(m, UINT8 , UINT16 , au8[j] , au16[j]);
                        ARRAY_QUIRKY(m, SINT8 ,  INT16 , as8[j] , as16[j]);
                        ARRAY_SIMPLE(m, UINT16, UINT16 , au16[j]);
                        ARRAY_SIMPLE(m, SINT16,  INT16 , as16[j]);
                        ARRAY_SIMPLE(m, UINT32, UINT32 , au32[j]);
                        ARRAY_SIMPLE(m, SINT32,  INT32 , as32[j]);
                        ARRAY_SIMPLE(m, UINT64, UINT64 , au64[j]);
                        ARRAY_SIMPLE(m, SINT64,  INT64 , as64[j]);
                        ARRAY_SIMPLE(m, DOUBLE, DOUBLE , adbl[j]);
                    default:
                        goto fail;
                    }
                }

                switch (base) {
                    APPEND_ARRAY(STRING, astr);
                    APPEND_ARRAY(BOOL  , abln);
                    APPEND_ARRAY(UINT8 , au8 );
                    APPEND_ARRAY(SINT8 , as8 );
                    APPEND_ARRAY(UINT16, au16);
                    APPEND_ARRAY(SINT16, as16);
                    APPEND_ARRAY(UINT32, au32);
                    APPEND_ARRAY(SINT32, as32);
                    APPEND_ARRAY(UINT64, au64);
                    APPEND_ARRAY(SINT64, as64);
                    APPEND_ARRAY(DOUBLE, adbl);
                default:
                    goto fail;
                }
            }

            if (!mrp_dbus_msg_exit_container(m))
                goto fail;
        }
    }

    if (sender_id != NULL)
        *sender_id = sender;

    return msg;

 fail:
    mrp_msg_unref(msg);
    errno = EBADMSG;

    return NULL;

#undef BASIC_SIMPLE
#undef BASIC_QUIRKY
#undef ARRAY_SIMPLE
#undef ARRAY_QUIRKY
#undef APPEND_ARRAY
}


static mrp_dbus_msg_t *data_encode(mrp_dbus_t *dbus, const char *destination,
                                   const char *path, const char *interface,
                                   const char *member, const char *sender_id,
                                   void *data, uint16_t tag)
{
#define BASIC_SIMPLE(_mtype, _dtype, _val)                                \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = &(_val);                                               \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

#define BASIC_STRING(_mtype, _dtype, _val)                                \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = _val;                                                  \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

#define BASIC_QUIRKY(_mtype, _dtype, _mval, _dval)                        \
        case MRP_MSG_FIELD_##_mtype:                                      \
            type  = MRP_DBUS_TYPE_##_dtype;                               \
            _dval = _mval;                                                \
            vptr  = &_dval;                                               \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

#define ARRAY_SIMPLE(_mtype, _dtype, _val)                                \
         case MRP_MSG_FIELD_##_mtype:                                     \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = &_val;                                                 \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

#define ARRAY_STRING(_mtype, _dtype, _val)                                \
         case MRP_MSG_FIELD_##_mtype:                                     \
            type = MRP_DBUS_TYPE_##_dtype;                                \
            vptr = _val;                                                  \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

#define ARRAY_QUIRKY(_mtype, _dtype, _mvar, _dvar)                        \
         case MRP_MSG_FIELD_##_mtype:                                     \
            type  = MRP_DBUS_TYPE_##_dtype;                               \
            _dvar = _mvar;                                                \
            vptr = &_dvar;                                                \
                                                                          \
            if (!mrp_dbus_msg_append_basic(m, type, vptr))                \
                goto fail;                                                \
            break

    mrp_dbus_msg_t    *m;
    mrp_data_descr_t  *descr;
    mrp_data_member_t *fields, *f;
    int                nfield;
    uint16_t           type, base;
    mrp_msg_value_t   *v;
    void              *vptr;
    uint32_t           n, j;
    int                i, blblen;
    const char        *sig;
    uint16_t           u16;
    int16_t            s16;
    uint32_t           bln, asize;

    m = mrp_dbus_msg_method_call(dbus, destination, path, interface, member);

    if (m == NULL)
        return NULL;

    descr = mrp_msg_find_type(tag);

    if (descr == NULL)
        goto fail;

    fields = descr->fields;
    nfield = descr->nfield;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_OBJECT_PATH,
                                   (void *)sender_id))
        goto fail;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &tag))
        goto fail;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &nfield))
        goto fail;

    for (i = 0, f = fields; i < nfield; i++, f++) {
        if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &f->tag) ||
            !mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT16, &f->type))
            goto fail;

        v = (mrp_msg_value_t *)(data + f->offs);

        switch (f->type) {
            BASIC_STRING(STRING, STRING , v->str);
            BASIC_QUIRKY(BOOL  , BOOLEAN, v->bln, bln);
            BASIC_QUIRKY(UINT8 , UINT16 , v->u8 , u16);
            BASIC_QUIRKY(SINT8 ,  INT16 , v->s8 , s16);
            BASIC_SIMPLE(UINT16, UINT16 , v->u16);
            BASIC_SIMPLE(SINT16,  INT16 , v->s16);
            BASIC_SIMPLE(UINT32, UINT32 , v->u32);
            BASIC_SIMPLE(SINT32,  INT32 , v->s32);
            BASIC_SIMPLE(UINT64, UINT64 , v->u64);
            BASIC_SIMPLE(SINT64,  INT64 , v->s64);
            BASIC_SIMPLE(DOUBLE, DOUBLE , v->dbl);

        case MRP_MSG_FIELD_BLOB:
            sig    = get_array_signature(f->type);
            blblen = mrp_data_get_blob_size(data, descr, i);
            asize  = blblen;

            if (blblen == -1)
                goto fail;

            if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT32, &asize))
                goto fail;

            if (!mrp_dbus_msg_open_container(m, MRP_DBUS_TYPE_ARRAY, sig))
                goto fail;

            for (i = 0; i < blblen; i++)
                if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_BYTE,
                                               f->blb + i))
                    goto fail;

            if (!mrp_dbus_msg_close_container(m))
                goto fail;
            break;

        default:
            if (!(f->type & MRP_MSG_FIELD_ARRAY))
                goto fail;

            base = f->type & ~(MRP_MSG_FIELD_ARRAY);
            n    = mrp_data_get_array_size(data, descr, i);
            sig  = get_array_signature(base);

            if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT32, &n))
                goto fail;

            if (!mrp_dbus_msg_open_container(m, MRP_DBUS_TYPE_ARRAY, sig))
                goto fail;

            for (j = 0; j < n; j++) {
                switch (base) {
                    ARRAY_STRING(STRING, STRING , v->astr[j]);
                    ARRAY_QUIRKY(BOOL  , BOOLEAN, v->abln[j], bln);
                    ARRAY_QUIRKY(UINT8 , UINT16 , v->au8[j] , u16);
                    ARRAY_QUIRKY(SINT8 ,  INT16 , v->as8[j] , s16);
                    ARRAY_SIMPLE(UINT16, UINT16 , v->au16[j]);
                    ARRAY_SIMPLE(SINT16,  INT16 , v->as16[j]);
                    ARRAY_SIMPLE(UINT32, UINT32 , v->au32[j]);
                    ARRAY_SIMPLE(SINT32,  INT32 , v->as32[j]);
                    ARRAY_SIMPLE(UINT64, UINT64 , v->au64[j]);
                    ARRAY_SIMPLE(DOUBLE, DOUBLE , v->adbl[j]);

                case MRP_MSG_FIELD_BLOB:
                    goto fail;

                default:
                    goto fail;
                }
            }

            if (!mrp_dbus_msg_close_container(m))
                goto fail;
        }
    }

    return m;

 fail:
    if (m != NULL)
        mrp_dbus_msg_unref(m);

    errno = ECOMM;

    return NULL;

#undef BASIC_SIMPLE
#undef BASIC_QUIRKY
#undef ARRAY_SIMPLE
#undef ARRAY_QUIRKY
}


static mrp_data_member_t *member_type(mrp_data_member_t *fields, int nfield,
                                      uint16_t tag)
{
    mrp_data_member_t *f;
    int                i;

    for (i = 0, f = fields; i < nfield; i++, f++)
        if (f->tag == tag)
            return f;

    return NULL;
}


static void *data_decode(mrp_dbus_msg_t *m, uint16_t *tagp,
                         const char **sender_id)
{
#define HANDLE_SIMPLE(_i, _mtype, _dtype, _var)                           \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_##_dtype,       \
                                         &(_var)))                        \
                goto fail;                                                \
            break

#define HANDLE_QUIRKY(_i, _mtype, _dtype, _mvar, _dvar)                   \
        case MRP_MSG_FIELD_##_mtype:                                      \
            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_##_dtype,       \
                                         &(_dvar)))                       \
                goto fail;                                                \
                                                                          \
            _mvar = _dvar;                                                \
            break

    void              *data;
    mrp_data_descr_t  *descr;
    mrp_data_member_t *fields, *f;
    int                nfield;
    uint16_t           tag, type, base;
    mrp_msg_value_t   *v;
    uint32_t           n, j, size;
    int                i;
    const char        *sender, *sig;
    uint32_t           u32;
    uint16_t           u16;
    int16_t            s16;

    tag  = 0;
    data = NULL;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_OBJECT_PATH, &sender))
        goto fail;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &tag))
        goto fail;

    descr = mrp_msg_find_type(tag);

    if (descr == NULL)
        goto fail;

    *tagp = tag;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &nfield))
        goto fail;

    if (nfield != descr->nfield)
        goto fail;

    fields = descr->fields;
    data   = mrp_allocz(descr->size);

    if (MRP_UNLIKELY(data == NULL))
        goto fail;

    for (i = 0; i < nfield; i++) {
        if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &tag))
            goto fail;

        if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT16, &type))
            goto fail;

        f = member_type(fields, nfield, tag);

        if (MRP_UNLIKELY(f == NULL))
            goto fail;

        v = (mrp_msg_value_t *)(data + f->offs);

        switch (type) {
            HANDLE_SIMPLE(&im, STRING, STRING , v->str);
            HANDLE_QUIRKY(&im, BOOL  , BOOLEAN, v->bln, u32);
            HANDLE_QUIRKY(&im, UINT8 , UINT16 , v->u8 , u16);
            HANDLE_QUIRKY(&im, SINT8 ,  INT16 , v->s8 , s16);
            HANDLE_SIMPLE(&im, UINT16, UINT16 , v->u16);
            HANDLE_SIMPLE(&im, SINT16,  INT16 , v->s16);
            HANDLE_SIMPLE(&im, UINT32, UINT32 , v->u32);
            HANDLE_SIMPLE(&im, SINT32,  INT32 , v->s32);
            HANDLE_SIMPLE(&im, UINT64, UINT64 , v->u64);
            HANDLE_SIMPLE(&im, SINT64,  INT64 , v->s64);
            HANDLE_SIMPLE(&im, DOUBLE, DOUBLE , v->dbl);

        case MRP_MSG_FIELD_BLOB:
            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT32, &size))
                goto fail;

            sig = MRP_DBUS_TYPE_BYTE_AS_STRING;

            if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, sig))
                goto fail;

            {
                uint8_t blb[size];

                for (j = 0; j < size; j++)
                    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_BYTE,
                                                 blb + j))
                        goto fail;

                v->blb = mrp_alloc(size);

                if (v->blb == NULL && size != 0)
                    goto fail;

                memcpy(v->blb, blb, size);
            }

            if (!mrp_dbus_msg_exit_container(m))
                goto fail;
            break;

        default:
            if (!(f->type & MRP_MSG_FIELD_ARRAY))
                goto fail;

            base = type & ~(MRP_MSG_FIELD_ARRAY);

            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT32, &n))
                goto fail;

            if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, NULL))
                goto fail;

            size = n;

            switch (base) {
            case MRP_MSG_FIELD_STRING: size *= sizeof(*v->astr); break;
            case MRP_MSG_FIELD_BOOL:   size *= sizeof(*v->abln); break;
            case MRP_MSG_FIELD_UINT8:  size *= sizeof(*v->au8);  break;
            case MRP_MSG_FIELD_SINT8:  size *= sizeof(*v->as8);  break;
            case MRP_MSG_FIELD_UINT16: size *= sizeof(*v->au16); break;
            case MRP_MSG_FIELD_SINT16: size *= sizeof(*v->as16); break;
            case MRP_MSG_FIELD_UINT32: size *= sizeof(*v->au32); break;
            case MRP_MSG_FIELD_SINT32: size *= sizeof(*v->as32); break;
            case MRP_MSG_FIELD_UINT64: size *= sizeof(*v->au64); break;
            case MRP_MSG_FIELD_SINT64: size *= sizeof(*v->as64); break;
            case MRP_MSG_FIELD_DOUBLE: size *= sizeof(*v->adbl); break;
            default:
                goto fail;
            }

            v->aany = mrp_allocz(size);
            if (v->aany == NULL)
                goto fail;

            for (j = 0; j < n; j++) {
                uint32_t  dbln[n];
                uint16_t  au16[n];
                int16_t   as16[n];

                switch (base) {
                    HANDLE_SIMPLE(&ia, STRING, STRING , v->astr[j]);
                    HANDLE_QUIRKY(&ia, BOOL  , BOOLEAN, v->abln[j], dbln[j]);
                    HANDLE_QUIRKY(&ia, UINT8 , UINT16 , v->au8[j] , au16[j]);
                    HANDLE_QUIRKY(&ia, SINT8 ,  INT16 , v->as8[j] , as16[j]);
                    HANDLE_SIMPLE(&ia, UINT16, UINT16 , v->au16[j]);
                    HANDLE_SIMPLE(&ia, SINT16,  INT16 , v->as16[j]);
                    HANDLE_SIMPLE(&ia, UINT32, UINT32 , v->au32[j]);
                    HANDLE_SIMPLE(&ia, SINT32,  INT32 , v->as32[j]);
                    HANDLE_SIMPLE(&ia, UINT64, UINT64 , v->au64[j]);
                    HANDLE_SIMPLE(&ia, SINT64,  INT64 , v->as64[j]);
                    HANDLE_SIMPLE(&ia, DOUBLE, DOUBLE , v->adbl[j]);
                }

                if (base == MRP_MSG_FIELD_STRING) {
                    v->astr[j] = mrp_strdup(v->astr[j]);
                    if (v->astr[j] == NULL)
                        goto fail;
                }
            }

            if (!mrp_dbus_msg_exit_container(m))
                goto fail;
        }

        if (f->type == MRP_MSG_FIELD_STRING) {
            v->str = mrp_strdup(v->str);
            if (v->str == NULL)
                goto fail;
        }
    }

    if (sender_id != NULL)
        *sender_id = sender;

    return data;

 fail:
    mrp_data_free(data, tag);
    errno = EBADMSG;

    return NULL;
}


static mrp_dbus_msg_t *raw_encode(mrp_dbus_t *dbus, const char *destination,
                                  const char *path, const char *interface,
                                  const char *member, const char *sender_id,
                                  void *data, size_t size)
{
    mrp_dbus_msg_t *m;
    const char     *sig;
    uint32_t        i, n;

    m = mrp_dbus_msg_method_call(dbus, destination, path, interface, member);

    if (m == NULL)
        return NULL;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_OBJECT_PATH,
                                   (void *)sender_id))
        goto fail;

    sig = MRP_DBUS_TYPE_BYTE_AS_STRING;
    n   = size;

    if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_UINT32, &n))
        goto fail;

    if (!mrp_dbus_msg_open_container(m, MRP_DBUS_TYPE_ARRAY, sig))
        goto fail;

    for (i = 0; i < n; i++)
        if (!mrp_dbus_msg_append_basic(m, MRP_DBUS_TYPE_BYTE, data + i))
            goto fail;

    if (!mrp_dbus_msg_close_container(m))
        goto fail;

    return m;

 fail:
    mrp_dbus_msg_unref(m);

    errno = ECOMM;

    return NULL;
}


static void *raw_decode(mrp_dbus_msg_t *m, size_t *sizep,
                        const char **sender_id)
{
    const char *sender, *sig;
    void       *data;
    uint32_t    n, i;

    data = NULL;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_OBJECT_PATH, &sender))
        goto fail;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_UINT32, &n))
        goto fail;

    sig = MRP_DBUS_TYPE_BYTE_AS_STRING;

    if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, sig))
        goto fail;

    {
        uint8_t databuf[n];

        for (i = 0; i < n; i++)
            if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_BYTE, databuf + i))
                goto fail;

        data = mrp_alloc(n);

        if (data == NULL && n != 0)
            goto fail;

        memcpy(data, databuf, n);
    }

    if (!mrp_dbus_msg_exit_container(m))
        goto fail;

    if (sizep != NULL)
        *sizep = (size_t)n;

    if (sender_id != NULL)
        *sender_id = sender;

    return data;

 fail:
    errno = EBADMSG;

    return NULL;
}


MRP_REGISTER_TRANSPORT(dbus, DBUS, dbus_t, dbus_resolve,
                       dbus_open, dbus_createfrom, dbus_close, NULL,
                       dbus_bind, NULL, NULL,
                       dbus_connect, dbus_disconnect,
                       dbus_sendmsg, dbus_sendmsgto,
                       dbus_sendraw, dbus_sendrawto,
                       dbus_senddata, dbus_senddatato,
                       NULL, NULL,
                       NULL, NULL);
