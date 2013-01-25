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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include <libwebsockets.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>

#include "websocklib.h"
#include "wsck-transport.h"

#define WSCKP "wsck"                     /* websocket transport prefix */
#define WSCKL 4                          /* websocket transport prefix length */


/*
 * a websocket transport instance
 */

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    wsl_ctx_t *ctx;                      /* websocket context */
    wsl_sck_t *sck;                      /* websocket instance */
} wsck_t;


static int resolve_address(const char *str, mrp_wsckaddr_t *wa, socklen_t alen);

static void connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                          void *user_data, void *proto_data);
static void closed_cb(wsl_sck_t *sck, int error, void *user_data,
                      void *proto_data);
static void recv_cb(wsl_sck_t *sck, void *data, size_t size, void *user_data,
                    void *proto_data);
static int  check_cb(wsl_sck_t *sck, void *user_data, void *proto_data);


static socklen_t wsck_resolve(const char *str, mrp_sockaddr_t *addr,
                              socklen_t size, const char **typep)
{
    mrp_wsckaddr_t *wa = (mrp_wsckaddr_t *)addr;
    socklen_t       len;

    len = resolve_address(str, wa, size);

    if (len <= 0)
        return 0;
    else {
        if (typep != NULL)
            *typep = WSCKP;

        return len;
    }
}


static int wsck_open(mrp_transport_t *mt)
{
    MRP_UNUSED(mt);

    wsl_set_loglevel(WSL_LOG_ALL);

    return TRUE;
}


static int wsck_createfrom(mrp_transport_t *mt, void *conn)
{
    MRP_UNUSED(mt);
    MRP_UNUSED(conn);

    return FALSE;
}


static void wsck_close(mrp_transport_t *mt)
{
    wsck_t    *t   = (wsck_t *)mt;
    wsl_ctx_t *ctx = t->ctx;
    wsl_sck_t *sck = t->sck;
    void      *user_data;

    t->sck = NULL;
    t->ctx = NULL;

    user_data = wsl_close(sck);

    if (user_data == t)                  /* was our associated context */
        wsl_unref_context(ctx);
}


static int wsck_bind(mrp_transport_t *mt, mrp_sockaddr_t *addr,
                     socklen_t addrlen)
{
    static wsl_proto_t proto = {
        .name       = "murphy",
        .cbs        = { .connection = connection_cb,
                        .closed     = closed_cb,
                        .recv       = recv_cb,
                        .check      = check_cb,      },
        .framed     = FALSE,
        .proto_data = NULL
    };

    wsck_t          *t  = (wsck_t *)mt;
    mrp_wsckaddr_t  *wa;
    struct sockaddr *sa;

    if (addr->any.sa_family != MRP_AF_WSCK || addrlen != sizeof(*wa))
        return FALSE;

    if (t->ctx != NULL)
        return FALSE;

    /*
     * Unfortunately instead of binding to an address/port pair, the
     * underlying libwebsockets library API allows one to bind to a
     * device/port pair, with NULL being a wildcard device.
     *
     * XXX TODO:
     * For the time being, we ignore the given address and always bind
     * to all interfaces. Later we can try to be a bit cleverer and eg.
     * add glue code that digs out the device name based on the address
     * (whenever this is unique).
     */

    wa = (mrp_wsckaddr_t *)addr;

    switch (wa->wsck_addr.family) {
    case AF_INET:  sa = (struct sockaddr *)&wa->wsck_addr.v4; break;
    case AF_INET6: sa = (struct sockaddr *)&wa->wsck_addr.v6; break;
    default:
        errno = EAFNOSUPPORT;
        return FALSE;
    }

    t->ctx = wsl_create_context(t->ml, sa, &proto, 1, t);

    if (t->ctx != NULL)
        return TRUE;
    else
        return FALSE;
}


static int wsck_listen(mrp_transport_t *mt, int backlog)
{
    MRP_UNUSED(mt);
    MRP_UNUSED(backlog);

    mt->listened = TRUE;

    return TRUE;
}


static int wsck_accept(mrp_transport_t *mt, mrp_transport_t *mlt)
{
    wsck_t *lt = (wsck_t *)mlt;
    wsck_t *t  = (wsck_t *)mt;

    t->sck = wsl_accept_pending(lt->ctx, t);

    if (t->sck != NULL) {
        mrp_debug("accepted websocket connection %p", mlt);

        return TRUE;
    }
    else {
        mrp_debug("failed to accept websocket connection on %p", mlt);

        return FALSE;
    }
}


static int wsck_connect(mrp_transport_t *mt, mrp_sockaddr_t *addr,
                        socklen_t addrlen)
{
    static wsl_proto_t proto = {
        .name       = "murphy",
        .cbs        = { .connection = connection_cb,
                        .closed     = closed_cb,
                        .recv       = recv_cb,
                        .check      = check_cb,      },
        .framed     = FALSE,
        .proto_data = NULL
    };

    wsck_t          *t = (wsck_t *)mt;
    mrp_wsckaddr_t  *wa;
    struct sockaddr *sa;

    if (addr->any.sa_family != MRP_AF_WSCK || addrlen != sizeof(*wa))
        return FALSE;

    if (t->ctx != NULL)
        return FALSE;

    wa = (mrp_wsckaddr_t *)addr;

    switch (wa->wsck_addr.family) {
    case AF_INET:  sa = (struct sockaddr *)&wa->wsck_addr.v4; break;
    case AF_INET6: sa = (struct sockaddr *)&wa->wsck_addr.v6; break;
    default:
        errno = EAFNOSUPPORT;
        return FALSE;
    }

    t->ctx = wsl_create_context(t->ml, NULL, &proto, 1, t);

    if (t->ctx == NULL)
        return FALSE;

    t->sck = wsl_connect(t->ctx, sa, "murphy", t);

    if (t->sck != NULL) {
        t->connected = TRUE;

        return TRUE;
    }
    else {
        wsl_unref_context(t->ctx);
        t->ctx = NULL;
    }

    return FALSE;
}


static int wsck_disconnect(mrp_transport_t *mt)
{
    wsck_t    *t   = (wsck_t *)mt;
    wsl_ctx_t *ctx = t->ctx;
    wsl_sck_t *sck = t->sck;
    void      *user_data;

    t->sck = NULL;
    t->ctx = NULL;

    user_data = wsl_close(sck);

    if (user_data == t)                  /* was our associated context */
        wsl_unref_context(ctx);

    return TRUE;
}


static int wsck_send(mrp_transport_t *mt, mrp_msg_t *msg)
{
    wsck_t  *t = (wsck_t *)mt;
    void    *buf;
    ssize_t  size;
    int      success;

    size = mrp_msg_default_encode(msg, &buf);

    if (wsl_send(t->sck, buf, size))
        success = TRUE;
    else
        success = FALSE;

    mrp_free(buf);

    return success;
}


static int wsck_sendraw(mrp_transport_t *mt, void *data, size_t size)
{
    wsck_t  *t = (wsck_t *)mt;

    return wsl_send(t->sck, data, size);
}


static int wsck_senddata(mrp_transport_t *mt, void *data, uint16_t tag)
{
    wsck_t           *t = (wsck_t *)mt;
    mrp_data_descr_t *type;
    void             *buf;
    size_t            size, reserve;
    uint16_t         *tagp;
    int               status;

    type = mrp_msg_find_type(tag);

    if (type != NULL) {
        reserve = sizeof(*tagp);
        size    = mrp_data_encode(&buf, data, type, reserve);

        if (size > 0) {
            tagp  = buf;
            *tagp = htobe16(tag);

            status = wsl_send(t->sck, buf, size);

            mrp_free(buf);
            return status;
        }
    }

    return FALSE;
}


static inline int looks_ipv4(const char *p)
{
#define DEC_DIGIT(c) ('0' <= (c) && (c) <= '9')
    if (DEC_DIGIT(p[0])) {
        if (p[1] == '.')
            return TRUE;

        if (DEC_DIGIT(p[1])) {
            if (p[2] == '.')
                return TRUE;

            if (DEC_DIGIT(p[2])) {
                if (p[3] == '.')
                    return TRUE;
            }
        }
    }

    return FALSE;
#undef DEC_DIGIT
}


static int resolve_address(const char *str, mrp_wsckaddr_t *wa, socklen_t alen)
{
    struct addrinfo *ai, hints;
    const char      *node, *port, *proto;
    char             nbuf[256], pbuf[32];
    int              family, status;
    size_t           len;

    if (strncmp(str, WSCKP":", WSCKL + 1) != 0)
        return 0;
    else
        str += WSCKL + 1;

    node = (char *)str;

    if (node[0] == '[') {
        node++;
        family = AF_INET6;
        port   = strchr(node, ']');
    }
    else if (looks_ipv4(node)) {
        family = AF_INET;
        port   = strchr(node, ':');
    }
    else {
        family = AF_UNSPEC;
        port   = strrchr(node, ':');
    }

    if (port == NULL || (*port != ':' && *port != ']')) {
        errno = EINVAL;
        return -1;
    }

    len = port - node;

    if (len > sizeof(nbuf) - 1) {
        errno = EOVERFLOW;
        return -1;
    }

    strncpy(nbuf, node, len);
    nbuf[len] = '\0';

    if (*port == ']')
        port++;

    if (*port != ':') {
        errno = EINVAL;
        return -1;
    }

    port++;
    proto = strchr(port, '/');

    if (proto != NULL) {
        len = proto - port;

        if (len > sizeof(pbuf) - 1) {
            errno = EOVERFLOW;
            return -1;
        }

        strncpy(pbuf, port, len);
        pbuf[len] = '\0';

        proto++;
        if (strlen(proto) > sizeof(wa->wsck_proto) - 1) {
            errno = EOVERFLOW;
            return -1;
        }
    }
    else {
        proto = MRP_WSCK_DEFPROTO;
        len   = strlen(port);

        if (len > sizeof(pbuf) - 1) {
            errno = EOVERFLOW;
            return -1;
        }

        strcpy(pbuf, port);
    }

    mrp_clear(&hints);
    hints.ai_family = family;

    status = getaddrinfo(nbuf, pbuf, &hints, &ai);

    switch (status) {
    case 0:
        if (ai->ai_addrlen <= alen) {
            wa->wsck_family = MRP_AF_WSCK;
            memcpy(&wa->wsck_addr, ai->ai_addr, ai->ai_addrlen);
            strcpy(wa->wsck_proto, proto);

            len = sizeof(*wa);
        }
        else {
            errno = EOVERFLOW;
            len   = -1;
        }

        freeaddrinfo(ai);
        return len;

#define MAP_ERROR(ai_err, err)                  \
        case EAI_##ai_err:                      \
            errno = err;                        \
            return -1

        MAP_ERROR(AGAIN     , EAGAIN);
        MAP_ERROR(BADFLAGS  , EADDRNOTAVAIL);
        MAP_ERROR(FAIL      , EHOSTUNREACH);
        MAP_ERROR(FAMILY    , EPFNOSUPPORT);
        MAP_ERROR(MEMORY    , ENOMEM);
        MAP_ERROR(NONAME    , EHOSTUNREACH);
        MAP_ERROR(SERVICE   , EAFNOSUPPORT);
        MAP_ERROR(SOCKTYPE  , EHOSTUNREACH);
        MAP_ERROR(SYSTEM    , EHOSTUNREACH);
#ifdef EAI_ADDRFAMILY
        MAP_ERROR(ADDRFAMILY, EHOSTUNREACH);
#endif
#ifdef EAI_NODATA
        MAP_ERROR(NODATA    , EHOSTUNREACH);
#endif

    default:
        errno = EHOSTUNREACH;
    }

    return -1;
}


static int print_address(char *buf, size_t size, mrp_wsckaddr_t *wa)
{
    struct sockaddr *saddr;
    socklen_t        salen;
    char             nbuf[256], pbuf[32], *b, *e;
    int              status;

    if (wa->wsck_family != MRP_AF_WSCK) {
    invalid:
        errno = EINVAL;
        return -1;
    }

    switch (wa->wsck_addr.family) {
    case AF_INET:
        saddr = (struct sockaddr *)&wa->wsck_addr.v4;
        salen = sizeof(wa->wsck_addr.v4);
        b     = "";
        e     = "";
        break;
    case AF_INET6:
        saddr = (struct sockaddr *)&wa->wsck_addr.v6;
        salen = sizeof(wa->wsck_addr.v6);
        b     = "[";
        e     = "]";
        break;
    default:
        goto invalid;
    }

    status = getnameinfo(saddr, salen, nbuf, sizeof(nbuf), pbuf, sizeof(pbuf),
                         NI_NUMERICHOST | NI_NUMERICSERV);

    if (status == 0)
        return snprintf(buf, size, "wsck:%s%s%s:%s/%s",
                        b, nbuf, e, pbuf, wa->wsck_proto);
    else {
        printf("error: %d: %s\n", status, gai_strerror(status));

        errno = EINVAL;
        return -1;
    }
}


static void connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                          void *user_data, void *proto_data)
{
    wsck_t *t = (wsck_t *)user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(proto_data);

    mrp_debug("incoming connection (%s) for context %p", protocol, ctx);

    if (t->listened && strncmp(protocol, "http", 4)) {
        MRP_TRANSPORT_BUSY(t, {
                t->evt.connection((mrp_transport_t *)t, t->user_data);
            });
    }
    else
        mrp_log_error("connection attempt on non-listened transport %p", t);
}


static void closed_cb(wsl_sck_t *sck, int error, void *user_data,
                      void *proto_data)
{
    wsck_t *t = (wsck_t *)user_data;

    MRP_UNUSED(proto_data);

    mrp_debug("websocket %p closed", sck);

    if (t->evt.closed != NULL)
        MRP_TRANSPORT_BUSY(t, {
                t->evt.closed((mrp_transport_t *)t, error, t->user_data);
            });
}


static void recv_cb(wsl_sck_t *sck, void *data, size_t size, void *user_data,
                    void *proto_data)
{
    wsck_t *t = (wsck_t *)user_data;

    MRP_UNUSED(data);
    MRP_UNUSED(user_data);
    MRP_UNUSED(proto_data);

    mrp_debug("%d bytes on websocket %p", size, sck);

    MRP_TRANSPORT_BUSY(t, {
            t->recv_data((mrp_transport_t *)t, data, size, NULL, 0);
        });
}


static int check_cb(wsl_sck_t *sck, void *user_data, void *proto_data)
{
    wsck_t *t = (wsck_t *)user_data;

    MRP_UNUSED(proto_data);

    mrp_debug("checking if transport %p (%p) has been destroyed", t, sck);

    if (t != NULL) {
        if (t->check_destroy((mrp_transport_t *)t)) {
            mrp_debug("transport has been destroyed");
            return TRUE;
        }
        else
            mrp_debug("transport has not been destroyed");
    }

    return FALSE;
}


MRP_REGISTER_TRANSPORT(wsck, WSCKP, wsck_t, wsck_resolve,
                       wsck_open, wsck_createfrom, wsck_close,
                       wsck_bind, wsck_listen, wsck_accept,
                       wsck_connect, wsck_disconnect,
                       wsck_send, NULL,
                       wsck_sendraw, NULL,
                       wsck_senddata, NULL);
