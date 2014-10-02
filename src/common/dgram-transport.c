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

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>

#ifndef UNIX_PATH_MAX
#    define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)NULL)->sun_path)
#endif

#define UDP4  "udp4"
#define UDP4L 4
#define UDP6  "udp6"
#define UDP6L 4
#define UNXD  "unxd"
#define UNXDL 4


#define DEFAULT_SIZE 1024                /* default input buffer size */

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    int             sock;                /* UDP socket */
    int             family;              /* socket family */
    mrp_io_watch_t *iow;                 /* socket I/O watch */
    void           *ibuf;                /* input buffer */
    size_t          isize;               /* input buffer size */
    size_t          idata;               /* amount of input data */
} dgrm_t;


static void dgrm_recv_cb(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
                         void *user_data);
static int dgrm_disconnect(mrp_transport_t *mu);
static int open_socket(dgrm_t *u, int family);


/*
 * XXX TODO:
 *
 *     There is an almost verbatim copy of this in stream-transport.c
 *     The only differences are the actual address type specifier
 *     prefixes... Combine these and separate the result out to a
 *     new transport-priv.[hc].
 */


static int parse_address(const char *str, int *familyp, char *nodep,
                         size_t nsize, char **servicep, const char **typep)
{
    char       *node, *service;
    const char *type;
    int         family;
    size_t      l, nl;

    node = (char *)str;

    if (!strncmp(node, UDP4":", l=UDP4L+1)) {
        family = AF_INET;
        type   = UDP4;
        node  += l;
    }
    else if (!strncmp(node, UDP6":", l=UDP6L+1)) {
        family = AF_INET6;
        type   = UDP6;
        node  += l;
    }
    else if (!strncmp(node, UNXD":", l=UNXDL+1)) {
        family = AF_UNIX;
        type   = UNXD;
        node  += l;
    }
    else {
        if      (node[0] == '[') family = AF_INET6;
        else if (node[0] == '/') family = AF_UNIX;
        else if (node[0] == '@') family = AF_UNIX;
        else                     family = AF_UNSPEC;

        type = NULL;
    }

    switch (family) {
    case AF_INET:
        service = strrchr(node, ':');
        if (service == NULL) {
            errno = EINVAL;
            return -1;
        }

        nl = service - node;
        service++;

    case AF_INET6:
        service = strrchr(node, ':');

        if (service == NULL || service == node) {
            errno = EINVAL;
            return -1;
        }

        if (node[0] == '[') {
            node++;

            if (service[-1] != ']') {
                errno = EINVAL;
                return -1;
            }

            nl = service - node - 1;
        }
        else
            nl = service - node;
        service++;
        break;

    case AF_UNSPEC:
        if (!strncmp(node, "tcp:", l=4))
            node += l;
        service = strrchr(node, ':');

        if (service == NULL || service == node) {
            errno = EINVAL;
            return -1;
        }

        if (node[0] == '[') {
            node++;
            family = AF_INET6;

            if (service[-1] != ']') {
                errno = EINVAL;
                return -1;
            }

            nl = service - node - 1;
        }
        else {
            family = AF_INET;
            nl = service - node;
        }
        service++;
        break;

    case AF_UNIX:
        service = NULL;
        nl      = strlen(node);
    }

    if (nl >= nsize) {
        errno = ENOMEM;
        return -1;
    }

    strncpy(nodep, node, nl);
    nodep[nl] = '\0';
    *servicep = service;
    *familyp  = family;
    if (typep != NULL)
        *typep = type;

    return 0;
}


static socklen_t dgrm_resolve(const char *str, mrp_sockaddr_t *addr,
                              socklen_t size, const char **typep)
{
    struct addrinfo    *ai, hints;
    struct sockaddr_un *un;
    char                node[UNIX_PATH_MAX], *port;
    socklen_t           len;

    mrp_clear(&hints);

    if (parse_address(str, &hints.ai_family, node, sizeof(node),
                      &port, typep) < 0)
        return 0;

    switch (hints.ai_family) {
    case AF_UNIX:
        un  = &addr->unx;
        len = MRP_OFFSET(typeof(*un), sun_path) + strlen(node) + 1;

        if (size < len)
            errno = ENOMEM;
        else {
            un->sun_family = AF_UNIX;
            strncpy(un->sun_path, node, UNIX_PATH_MAX-1);
            if (un->sun_path[0] == '@')
                un->sun_path[0] = '\0';
        }

        /* When binding the socket, we don't need the null at the end */
        len--;

        break;

    case AF_INET:
    case AF_INET6:
    default:
        if (getaddrinfo(node, port, &hints, &ai) == 0) {
            if (ai->ai_addrlen <= size) {
                memcpy(addr, ai->ai_addr, ai->ai_addrlen);
                len = ai->ai_addrlen;
            }
            else
                len = 0;

            freeaddrinfo(ai);
        }
        else
            len = 0;
    }

    return len;
}


static int dgrm_open(mrp_transport_t *mu)
{
    dgrm_t *u = (dgrm_t *)mu;

    u->sock   = -1;
    u->family = -1;

    return TRUE;
}


static int dgrm_createfrom(mrp_transport_t *mu, void *conn)
{
    dgrm_t         *u = (dgrm_t *)mu;
    int             on;
    mrp_io_event_t  events;

    u->sock = *(int *)conn;

    if (u->sock >= 0) {
        if (mu->flags & MRP_TRANSPORT_REUSEADDR) {
            on = 1;
            setsockopt(u->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        }
        if (mu->flags & MRP_TRANSPORT_NONBLOCK) {
            on = 1;
            fcntl(u->sock, F_SETFL, O_NONBLOCK, on);
        }

        events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
        u->iow = mrp_add_io_watch(u->ml, u->sock, events, dgrm_recv_cb, u);

        if (u->iow != NULL)
            return TRUE;
    }

    return FALSE;
}


static int dgrm_bind(mrp_transport_t *mu, mrp_sockaddr_t *addr,
                     socklen_t addrlen)
{
    dgrm_t *u = (dgrm_t *)mu;

    if (u->sock != -1 || !u->connected) {
        if (open_socket(u, addr->any.sa_family))
            if (bind(u->sock, &addr->any, addrlen) == 0)
                return TRUE;
    }

    return FALSE;
}


static int dgrm_listen(mrp_transport_t *mt, int backlog)
{
    MRP_UNUSED(mt);
    MRP_UNUSED(backlog);

    return TRUE;            /* can be connected to without listening */
}


static void dgrm_close(mrp_transport_t *mu)
{
    dgrm_t *u = (dgrm_t *)mu;

    mrp_del_io_watch(u->iow);
    u->iow = NULL;

    mrp_free(u->ibuf);
    u->ibuf  = NULL;
    u->isize = 0;
    u->idata = 0;

    if (u->sock >= 0){
        close(u->sock);
        u->sock = -1;
    }
}


static void dgrm_recv_cb(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
                         void *user_data)
{
    dgrm_t          *u  = (dgrm_t *)user_data;
    mrp_transport_t *mu = (mrp_transport_t *)u;
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    uint32_t         size;
    ssize_t          n;
    void            *data;
    int              old, error;

    MRP_UNUSED(w);

    if (events & MRP_IO_EVENT_IN) {
        if (u->idata == u->isize) {
            if (u->isize != 0) {
                old      = u->isize;
                u->isize *= 2;
            }
            else {
                old      = 0;
                u->isize = DEFAULT_SIZE;
            }
            if (!mrp_reallocz(u->ibuf, old, u->isize)) {
                error = ENOMEM;
            fatal_error:
            closed:
                dgrm_disconnect(mu);

                if (u->evt.closed != NULL)
                    MRP_TRANSPORT_BUSY(mu, {
                            mu->evt.closed(mu, error, mu->user_data);
                        });

                u->check_destroy(mu);
                return;
            }
        }

        if (recv(fd, &size, sizeof(size), MSG_PEEK) != sizeof(size)) {
            error = EIO;
            goto fatal_error;
        }

        size = ntohl(size);

        if (u->isize < size + sizeof(size)) {
            old      = u->isize;
            u->isize = size + sizeof(size);

            if (!mrp_reallocz(u->ibuf, old, u->isize)) {
                error = ENOMEM;
                goto fatal_error;
            }
        }

        addrlen = sizeof(addr);
        n = recvfrom(fd, u->ibuf, size + sizeof(size), 0, &addr.any, &addrlen);

        if (n != (ssize_t)(size + sizeof(size))) {
            error = n < 0 ? EIO : EPROTO;
            goto fatal_error;
        }

        data  = u->ibuf + sizeof(size);
        error = mu->recv_data(mu, data, size, &addr, addrlen);

        if (error)
            goto fatal_error;

        if (u->check_destroy(mu))
            return;
    }

    if (events & MRP_IO_EVENT_HUP) {
        error = 0;
        goto closed;
    }
}


static int open_socket(dgrm_t *u, int family)
{
    mrp_io_event_t events;
    int            on;
    long           nb;

    u->sock = socket(family, SOCK_DGRAM, 0);

    if (u->sock != -1) {
        if (u->flags & MRP_TRANSPORT_REUSEADDR) {
            on = 1;
            setsockopt(u->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        }
        if (u->flags & MRP_TRANSPORT_NONBLOCK) {
            nb = 1;
            fcntl(u->sock, F_SETFL, O_NONBLOCK, nb);
        }
        if (u->flags & MRP_TRANSPORT_CLOEXEC) {
            on = 1;
            fcntl(u->sock, F_SETFL, O_CLOEXEC, on);
        }

        events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
        u->iow = mrp_add_io_watch(u->ml, u->sock, events, dgrm_recv_cb, u);

        if (u->iow != NULL)
            return TRUE;
        else {
            close(u->sock);
            u->sock = -1;
        }
    }

    return FALSE;
}


static int dgrm_connect(mrp_transport_t *mu, mrp_sockaddr_t *addr,
                        socklen_t addrlen)
{
    dgrm_t *u = (dgrm_t *)mu;
    int     on;
    long    nb;

    if (MRP_UNLIKELY(u->family != -1 && u->family != addr->any.sa_family))
        return FALSE;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, addr->any.sa_family))
            return FALSE;
    }

    if (connect(u->sock, &addr->any, addrlen) == 0) {
        on = 1;
        setsockopt(u->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        nb = 1;
        fcntl(u->sock, F_SETFL, O_NONBLOCK, nb);

        return TRUE;
    }

    return FALSE;
}


static int dgrm_disconnect(mrp_transport_t *mu)
{
    dgrm_t          *u    = (dgrm_t *)mu;
    struct sockaddr  none = { .sa_family = AF_UNSPEC, };


    if (u->connected) {
        connect(u->sock, &none, sizeof(none));

        return TRUE;
    }
    else
        return FALSE;
}


static int dgrm_send(mrp_transport_t *mu, mrp_msg_t *msg)
{
    dgrm_t       *u = (dgrm_t *)mu;
    struct iovec  iov[2];
    void         *buf;
    ssize_t       size, n;
    uint32_t      len;

    if (u->connected) {
        size = mrp_msg_default_encode(msg, &buf);

        if (size >= 0) {
            len = htonl(size);
            iov[0].iov_base = &len;
            iov[0].iov_len  = sizeof(len);
            iov[1].iov_base = buf;
            iov[1].iov_len  = size;

            n = writev(u->sock, iov, 2);
            mrp_free(buf);

            if (n == (ssize_t)(size + sizeof(len)))
                return TRUE;
            else {
                if (n == -1 && errno == EAGAIN) {
                    mrp_log_error("%s(): XXX TODO: this sucks, need to add "
                                  "output queuing for dgrm-transport.",
                                  __FUNCTION__);
                }
            }
        }
    }

    return FALSE;
}


static int dgrm_sendto(mrp_transport_t *mu, mrp_msg_t *msg,
                       mrp_sockaddr_t *addr, socklen_t addrlen)
{
    dgrm_t          *u = (dgrm_t *)mu;
    struct iovec     iov[2];
    void            *buf;
    ssize_t          size, n;
    uint32_t         len;
    struct msghdr    hdr;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
            return FALSE;
    }

    size = mrp_msg_default_encode(msg, &buf);

    if (size >= 0) {
        len = htonl(size);
        iov[0].iov_base = &len;
        iov[0].iov_len  = sizeof(len);
        iov[1].iov_base = buf;
        iov[1].iov_len  = size;

        hdr.msg_name    = addr;
        hdr.msg_namelen = addrlen;
        hdr.msg_iov     = iov;
        hdr.msg_iovlen  = MRP_ARRAY_SIZE(iov);

        hdr.msg_control    = NULL;
        hdr.msg_controllen = 0;
        hdr.msg_flags      = 0;

        n = sendmsg(u->sock, &hdr, 0);
        mrp_free(buf);

        if (n == (ssize_t)(size + sizeof(len)))
            return TRUE;
        else {
            if (n == -1 && errno == EAGAIN) {
                mrp_log_error("%s(): XXX TODO: dgrm-transport send failed",
                              __FUNCTION__);
            }
        }
    }

    return FALSE;
}


static int dgrm_sendraw(mrp_transport_t *mu, void *data, size_t size)
{
    dgrm_t  *u = (dgrm_t *)mu;
    ssize_t  n;

    if (u->connected) {
        n = write(u->sock, data, size);

        if (n == (ssize_t)size)
            return TRUE;
        else {
            if (n == -1 && errno == EAGAIN) {
                mrp_log_error("%s(): XXX TODO: this sucks, need to add "
                              "output queuing for dgrm-transport.",
                              __FUNCTION__);
            }
        }
    }

    return FALSE;
}


static int dgrm_sendrawto(mrp_transport_t *mu, void *data, size_t size,
                          mrp_sockaddr_t *addr, socklen_t addrlen)
{
    dgrm_t  *u = (dgrm_t *)mu;
    ssize_t  n;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
            return FALSE;
    }

    n = sendto(u->sock, data, size, 0, &addr->any, addrlen);

    if (n == (ssize_t)size)
        return TRUE;
    else {
        if (n == -1 && errno == EAGAIN) {
            mrp_log_error("%s(): XXX TODO: dgrm-transport send failed",
                          __FUNCTION__);
        }
    }

    return FALSE;
}


static int senddatato(mrp_transport_t *mu, void *data, uint16_t tag,
                      mrp_sockaddr_t *addr, socklen_t addrlen)
{
    dgrm_t           *u = (dgrm_t *)mu;
    mrp_data_descr_t *type;
    ssize_t           n;
    void             *buf;
    size_t            size, reserve, len;
    uint32_t         *lenp;
    uint16_t         *tagp;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
            return FALSE;
    }

    type = mrp_msg_find_type(tag);

    if (type != NULL) {
        reserve = sizeof(*lenp) + sizeof(*tagp);
        size    = mrp_data_encode(&buf, data, type, reserve);

        if (size > 0) {
            lenp  = buf;
            len   = size - sizeof(*lenp);
            tagp  = buf + sizeof(*lenp);
            *lenp = htobe32(len);
            *tagp = htobe16(tag);

            if (u->connected)
                n = send(u->sock, buf, len + sizeof(*lenp), 0);
            else
                n = sendto(u->sock, buf, len + sizeof(*lenp), 0, &addr->any,
                           addrlen);

            mrp_free(buf);

            if (n == (ssize_t)(len + sizeof(*lenp)))
                return TRUE;
            else {
                if (n == -1 && errno == EAGAIN) {
                    mrp_log_error("%s(): XXX TODO: dgrm-transport send"
                                  " needs queuing", __FUNCTION__);
                }
            }
        }
    }

    return FALSE;
}


static int dgrm_senddata(mrp_transport_t *mu, void *data, uint16_t tag)
{
    if (mu->connected)
        return senddatato(mu, data, tag, NULL, 0);
    else
        return FALSE;
}


static int dgrm_senddatato(mrp_transport_t *mu, void *data, uint16_t tag,
                           mrp_sockaddr_t *addr, socklen_t addrlen)
{
    return senddatato(mu, data, tag, addr, addrlen);
}


static int sendnativeto(mrp_transport_t *mu, void *data, uint32_t type_id,
                        mrp_sockaddr_t *addr, socklen_t addrlen)
{
    dgrm_t        *u   = (dgrm_t *)mu;
    mrp_typemap_t *map = u->map;
    void          *buf;
    size_t         size, reserve;
    uint32_t      *lenp;
    ssize_t        n;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
            return FALSE;
    }

    reserve = sizeof(*lenp);

    if (mrp_encode_native(data, type_id, reserve, &buf, &size, map) > 0) {
        lenp  = buf;
        *lenp = htobe32(size - sizeof(*lenp));

        if (u->connected)
            n = send(u->sock, buf, size, 0);
        else
            n = sendto(u->sock, buf, size, 0, &addr->any, addrlen);

        mrp_free(buf);

        if (n == (ssize_t)size)
            return TRUE;
        else {
            if (n == -1 && errno == EAGAIN) {
                mrp_log_error("%s(): XXX TODO: dgrm-transport send"
                              " needs queuing", __FUNCTION__);
            }
        }
    }

    return FALSE;
}


static int dgrm_sendnative(mrp_transport_t *mu, void *data, uint32_t type_id)
{
    if (mu->connected)
        return sendnativeto(mu, data, type_id, NULL, 0);
    else
        return FALSE;
}


static int dgrm_sendnativeto(mrp_transport_t *mu, void *data, uint32_t type_id,
                             mrp_sockaddr_t *addr, socklen_t addrlen)
{
    return sendnativeto(mu, data, type_id, addr, addrlen);
}


static int sendjsonto(mrp_transport_t *mu, mrp_json_t *msg,
                      mrp_sockaddr_t *addr, socklen_t addrlen)
{
    dgrm_t       *u = (dgrm_t *)mu;
    struct iovec  iov[2];
    const char   *s;
    ssize_t       size, n;
    uint32_t      len;

    if (MRP_UNLIKELY(u->sock == -1)) {
        if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
            return FALSE;
    }

    if (u->connected && (s = mrp_json_object_to_string(msg)) != NULL) {
        size = strlen(s);
        len  = htobe32(size);

        iov[0].iov_base = &len;
        iov[0].iov_len  = sizeof(len);
        iov[1].iov_base = (char *)s;
        iov[1].iov_len  = size;

        if (u->connected)
            n = writev(u->sock, iov, 2);
        else {
            struct msghdr mh;

            mh.msg_name       = &addr->any;
            mh.msg_namelen    = addrlen;
            mh.msg_iov        = iov;
            mh.msg_iovlen     = MRP_ARRAY_SIZE(iov);
            mh.msg_control    = NULL;
            mh.msg_controllen = 0;
            mh.msg_flags      = 0;

            n = sendmsg(u->sock, &mh, 0);
        }

        if (n == (ssize_t)(size + sizeof(len)))
            return TRUE;
        else {
            if (n == -1 && errno == EAGAIN) {
                mrp_log_error("%s(): XXX TODO: this sucks, need to add "
                              "output queuing for dgrm-transport.",
                              __FUNCTION__);
            }
        }
    }

    return FALSE;
}


static int dgrm_sendjson(mrp_transport_t *mu, mrp_json_t *msg)
{
    if (mu->connected)
        return sendjsonto(mu, msg, NULL, 0);
    else
        return FALSE;
}


static int dgrm_sendjsonto(mrp_transport_t *mu, mrp_json_t *msg,
                           mrp_sockaddr_t *addr, socklen_t addrlen)
{
    return sendjsonto(mu, msg, addr, addrlen);
}


MRP_REGISTER_TRANSPORT(udp4, UDP4, dgrm_t, dgrm_resolve,
                       dgrm_open, dgrm_createfrom, dgrm_close, NULL,
                       dgrm_bind, dgrm_listen, NULL,
                       dgrm_connect, dgrm_disconnect,
                       dgrm_send, dgrm_sendto,
                       dgrm_sendraw, dgrm_sendrawto,
                       dgrm_senddata, dgrm_senddatato,
                       NULL, NULL,
                       dgrm_sendnative, dgrm_sendnativeto,
                       dgrm_sendjson, dgrm_sendjsonto);

MRP_REGISTER_TRANSPORT(udp6, UDP6, dgrm_t, dgrm_resolve,
                       dgrm_open, dgrm_createfrom, dgrm_close, NULL,
                       dgrm_bind, dgrm_listen, NULL,
                       dgrm_connect, dgrm_disconnect,
                       dgrm_send, dgrm_sendto,
                       dgrm_sendraw, dgrm_sendrawto,
                       dgrm_senddata, dgrm_senddatato,
                       NULL, NULL,
                       dgrm_sendnative, dgrm_sendnativeto,
                       dgrm_sendjson, dgrm_sendjsonto);

MRP_REGISTER_TRANSPORT(unxdgrm, UNXD, dgrm_t, dgrm_resolve,
                       dgrm_open, dgrm_createfrom, dgrm_close, NULL,
                       dgrm_bind, dgrm_listen, NULL,
                       dgrm_connect, dgrm_disconnect,
                       dgrm_send, dgrm_sendto,
                       dgrm_sendraw, dgrm_sendrawto,
                       dgrm_senddata, dgrm_senddatato,
                       NULL, NULL,
                       dgrm_sendnative, dgrm_sendnativeto,
                       dgrm_sendjson, dgrm_sendjsonto);
