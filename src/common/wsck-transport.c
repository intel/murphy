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
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include <libwebsockets.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/transport.h>
#include <murphy/common/json.h>

#include "websocklib.h"
#include "wsck-transport.h"

#define WSCKP "wsck"                     /* websocket transport prefix */
#define WSCKL 4                          /* websocket transport prefix length */


/*
 * a websocket transport instance
 */

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    wsl_ctx_t          *ctx;             /* websocket context */
    wsl_sck_t          *sck;             /* websocket instance */
    int                 send_mode;       /* websocket send mode */
    const char         *http_root;       /* HTTP content root */
    mrp_wsck_urimap_t  *uri_table;       /* URI-to-path table */
    mrp_wsck_mimemap_t *mime_table;      /* suffix to MIME-type table */
    const char         *ssl_cert;        /* path to SSL certificate */
    const char         *ssl_pkey;        /* path to SSL private key */
    const char         *ssl_ca;          /* path to SSL CA */
    wsl_ssl_t           ssl;             /* SSL mode (wsl_ssl_t) */
    char               *protocol;        /* websocket protocol name */
    wsl_proto_t         proto[2];        /* protocol setup */
    mrp_list_hook_t     http_clients;    /* pure HTTP clients */
} wsck_t;


/*
 * a pure HTTP client instance
 */

typedef struct {
    wsl_sck_t          *sck;             /* websocket towards client */
    mrp_list_hook_t     hook;            /* hook to listening socket */
    const char         *http_root;       /* HTTP content root */
    mrp_wsck_urimap_t  *uri_table;       /* URI to path mapping */
    mrp_wsck_mimemap_t *mime_table;      /* suffix to MIME type mapping */
} http_client_t;


/*
 * default file suffix to MIME type mapping table
 */

static mrp_wsck_mimemap_t mime_table[] = {
    { "js"  , "application/javascript" },
    { "html", "text/html"              },
    { "htm ", "text/html"              },
    { "txt" , "text/plain"             },
    { NULL, NULL }
};


static int resolve_address(const char *str, mrp_wsckaddr_t *wa, socklen_t alen);

static void connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                          void *user_data, void *proto_data);
static void closed_cb(wsl_sck_t *sck, int error, void *user_data,
                      void *proto_data);
static void recv_cb(wsl_sck_t *sck, void *data, size_t size, void *user_data,
                    void *proto_data);
static int  check_cb(wsl_sck_t *sck, void *user_data, void *proto_data);

static void http_connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                               void *user_data, void *proto_data);
static void http_closed_cb(wsl_sck_t *sck, int error, void *user_data,
                           void *proto_data);
static void http_req_cb(wsl_sck_t *sck, void *data, size_t size,
                        void *user_data, void *proto_data);
static int  http_check_cb(wsl_sck_t *sck, void *user_data, void *proto_data);
static void http_done_cb(wsl_sck_t *sck, const char *uri, void *user_data,
                         void *proto_data);

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
    wsck_t *t = (wsck_t *)mt;

    mrp_list_init(&t->http_clients);
    wsl_set_loglevel(WSL_LOG_ALL/* | WSL_LOG_EXTRA*/);

    return TRUE;
}


static int wsck_createfrom(mrp_transport_t *mt, void *conn)
{
    wsck_t *t = (wsck_t *)mt;

    MRP_UNUSED(conn);

    mrp_list_init(&t->http_clients);

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
    mrp_free(t->protocol);
    t->protocol = NULL;

    user_data = wsl_close(sck);

    if (user_data == t)                  /* was our associated context */
        wsl_unref_context(ctx);
}


static int wsck_setopt(mrp_transport_t *mt, const char *opt, const void *val)
{
    wsck_t *t = (wsck_t *)mt;
    int     success;

    if (!strcmp(opt, MRP_WSCK_OPT_SENDMODE) && val != NULL) {
        if (!strcmp(val, "binary"))
            t->send_mode = WSL_SEND_BINARY;
        else if (!strcmp(val, "text"))
            t->send_mode = WSL_SEND_TEXT;
        else
            return FALSE;

        if (t->sck != NULL)
            return wsl_set_sendmode(t->sck, t->send_mode);
        else
            return TRUE;
    }

    success = TRUE;

    if (!strcmp(opt, MRP_WSCK_OPT_HTTPDIR))
        t->http_root = val;
    else if (!strcmp(opt, MRP_WSCK_OPT_MIMEMAP))
        t->mime_table = (void *)val;
    else if (!strcmp(opt, MRP_WSCK_OPT_URIMAP))
        t->uri_table = (void *)val;
    else if (!strcmp(opt, MRP_WSCK_OPT_SSL_CERT))
        t->ssl_cert = (const char *)val;
    else if (!strcmp(opt, MRP_WSCK_OPT_SSL_PKEY))
        t->ssl_pkey = (const char *)val;
    else if (!strcmp(opt, MRP_WSCK_OPT_SSL_CA))
        t->ssl_ca = (const char *)val;
    else if (!strcmp(opt, MRP_WSCK_OPT_SSL))
        t->ssl = *(wsl_ssl_t *)val;
    else
        success = FALSE;

    return success;
}


static int wsck_bind(mrp_transport_t *mt, mrp_sockaddr_t *addr,
                     socklen_t addrlen)
{
    wsck_t      *t       = (wsck_t *)mt;
    wsl_proto_t  proto[] = {
        {
            .name       = "http",
            .cbs        = { .connection = http_connection_cb,
                            .closed     = http_closed_cb,
                            .recv       = http_req_cb,
                            .check      = http_check_cb,
                            .http_done  = http_done_cb,
                            .load_certs = NULL,               },
            .framed     = FALSE,
            .proto_data = NULL
        },
        {
            .name       = "murphy",
            .cbs        = { .connection = connection_cb,
                            .closed     = closed_cb,
                            .recv       = recv_cb,
                            .check      = check_cb,
                            .http_done  = NULL,
                            .load_certs = NULL,               },
            .framed     = FALSE,
            .proto_data = NULL
        }
    };
    wsl_ctx_cfg_t    cfg;
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

    if ((t->protocol = mrp_strdup(wa->wsck_proto)) == NULL)
        return FALSE;

    t->proto[0] = proto[0];
    t->proto[1] = proto[1];

    t->proto[1].name = t->protocol;

    mrp_clear(&cfg);
    cfg.addr      = sa;
    cfg.protos    = &t->proto[0];
    cfg.nproto    = MRP_ARRAY_SIZE(t->proto);
    cfg.ssl_cert  = t->ssl_cert;
    cfg.ssl_pkey  = t->ssl_pkey;
    cfg.ssl_ca    = t->ssl_ca;
    cfg.gid       = WSL_NO_GID;
    cfg.uid       = WSL_NO_UID;
    cfg.user_data = t;

    t->ctx = wsl_create_context(t->ml, &cfg);

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

        /* default to mode inherited from listening transport */
        t->send_mode = lt->send_mode;
        wsl_set_sendmode(t->sck, t->send_mode);

        /* inherit pure HTTP settings by default */
        t->http_root  = lt->http_root;
        t->uri_table  = lt->uri_table;
        t->mime_table = lt->mime_table;

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
    wsck_t      *t     = (wsck_t *)mt;
    wsl_proto_t  proto = {
        .name       = "murphy",
        .cbs        = { .connection = connection_cb,
                        .closed     = closed_cb,
                        .recv       = recv_cb,
                        .check      = check_cb,      },
        .framed     = FALSE,
        .proto_data = NULL
    };

    wsl_ctx_cfg_t    cfg;
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

    if ((t->protocol = mrp_strdup(wa->wsck_proto)) == NULL)
        return FALSE;

    proto.name  = t->protocol;
    t->proto[0] = proto;

    mrp_clear(&cfg);
    cfg.addr      = NULL;
    cfg.protos    = &t->proto[0];
    cfg.nproto    = 1;
    cfg.ssl_cert  = t->ssl_cert;
    cfg.ssl_pkey  = t->ssl_pkey;
    cfg.ssl_ca    = t->ssl_ca;
    cfg.gid       = WSL_NO_GID;
    cfg.uid       = WSL_NO_UID;
    cfg.user_data = t;

    t->ctx = wsl_create_context(t->ml, &cfg);

    if (t->ctx == NULL)
        return FALSE;

    t->sck = wsl_connect(t->ctx, sa, t->protocol, t->ssl, t);

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


static int wsck_sendcustom(mrp_transport_t *mt, void *data)
{
    wsck_t     *t    = (wsck_t *)mt;
    mrp_json_t *json = (mrp_json_t *)data;
    const char *s;
    int         status;

    s = mrp_json_object_to_string(json);

    /*
     * Notes:
     *     Although json-c internally counts the length of the serialized
     *     object, it does not provide an API to get it out together with
     *     the string. Great...
     */

    if (s != NULL)
        status = wsl_send(t->sck, (void *)s, strlen(s));
    else
        status = FALSE;

    return status;
}


static inline int looks_ipv4(const char *p)
{
    if (isdigit(p[0])) {
        if (p[1] == '.')
            return TRUE;

        if (isdigit(p[1])) {
            if (p[2] == '.')
                return TRUE;

            if (isdigit(p[2])) {
                if (p[3] == '.')
                    return TRUE;
            }
        }
    }

    return FALSE;
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


#if 0
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
#endif

static void connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                          void *user_data, void *proto_data)
{
    wsck_t *t = (wsck_t *)user_data;

    MRP_UNUSED(addr);
    MRP_UNUSED(proto_data);

    mrp_debug("incoming connection (%s) for context %p", protocol, ctx);

    if (t->listened) {
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

    MRP_UNUSED(proto_data);

    mrp_debug("%zu bytes on websocket %p", size, sck);

    MRP_TRANSPORT_BUSY(t, {
            if (t->mode != MRP_TRANSPORT_MODE_CUSTOM)
                t->recv_data((mrp_transport_t *)t, data, size, NULL, 0);
            else {
                mrp_json_t *json = mrp_json_string_to_object(data, size);

                if (json != NULL) {
                    t->recv_data((mrp_transport_t *)t, json, 0, NULL, 0);
                    mrp_json_unref(json);
                }
            }
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


static http_client_t *http_create_client(wsck_t *lt)
{
    http_client_t *c;

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        mrp_list_init(&c->hook);
        c->sck = wsl_accept_pending(lt->ctx, c);

        if (c->sck != NULL) {
            c->http_root  = lt->http_root;
            c->uri_table  = lt->uri_table;
            c->mime_table = lt->mime_table;

            return c;
        }
        else {
            mrp_free(c);
            c = NULL;
        }
    }

    return c;
}


static void http_destroy_client(http_client_t *c)
{
    if (c != NULL) {
        mrp_list_delete(&c->hook);
        wsl_close(c->sck);
        mrp_free(c);
    }
}


const char *http_mapuri(http_client_t *c, const char *uri,
                        char *buf, size_t size)
{
    mrp_wsck_urimap_t  *um;
    mrp_wsck_mimemap_t *mm;
    const char         *suff, *root, *r, *s;

    root = c->http_root ? c->http_root : "/";

    if (c->uri_table != NULL) {
        for (um = c->uri_table; um->uri != NULL; um++) {
            if (!strcmp(uri, um->uri)) {
                if (um->path[0] != '/') {
                    r = root;
                    s = "/";
                }
                else {
                    r = "";
                    s = "";
                }

                if (snprintf(buf, size, "%s%s%s", r, s, um->path) < (int)size)
                    return um->type;
                else
                    return NULL;
            }
        }
    }

    if (c->http_root != NULL) {
        if (snprintf(buf, size, "%s/%s", root, uri) >= (int)size)
            return NULL;

        suff = strrchr(uri, '.');

        if (suff == NULL)
            return "text/plain";
        else
            suff++;

        if (c->mime_table != NULL) {
            for (mm = c->mime_table; mm->suffix != NULL; mm++) {
                if (!strcmp(mm->suffix, suff))
                    return mm->type;
            }
        }

        for (mm = mime_table; mm->suffix != NULL; mm++) {
            if (!strcmp(mm->suffix, suff))
                return mm->type;
        }
    }

    return NULL;
}


static void http_connection_cb(wsl_ctx_t *ctx, char *addr, const char *protocol,
                               void *user_data, void *proto_data)
{
    wsck_t        *t = (wsck_t *)user_data;
    http_client_t *c;

    MRP_UNUSED(addr);
    MRP_UNUSED(proto_data);

    mrp_debug("incoming %s connection for context %p", protocol, ctx);

    if (t->http_root != NULL || t->uri_table != NULL) {
        c = http_create_client(t);

        if (c != NULL)
            mrp_debug("accepted pure HTTP client for context %p", ctx);
        else
            mrp_log_error("failed to create new HTTP client");
    }
    else
        mrp_debug("rejecting pure HTTP client for context %p", ctx);
}


static void http_closed_cb(wsl_sck_t *sck, int error, void *user_data,
                           void *proto_data)
{
    http_client_t *c = (http_client_t *)user_data;

    MRP_UNUSED(proto_data);
    MRP_UNUSED(error);

    if (error)
        mrp_debug("HTTP client socket %p closed with error %d", sck, error);
    else
        mrp_debug("HTTP client socket %p closed", sck);

    http_destroy_client(c);
}


static void http_req_cb(wsl_sck_t *sck, void *data, size_t size,
                        void *user_data, void *proto_data)
{
    http_client_t *c   = (http_client_t *)user_data;
    const char    *uri = (const char *)data;
    const char    *type;
    char           path[PATH_MAX];

    MRP_UNUSED(size);
    MRP_UNUSED(proto_data);

    mrp_debug("HTTP request for URI '%s' on socket %p", uri, c->sck);

    type = http_mapuri(c, uri, path, sizeof(path));

    if (type != NULL) {
        mrp_debug("mapped to '%s' (%s)", path, type);
        wsl_serve_http_file(sck, path, type);
    }
    else
        mrp_debug("failed to map URI");
}


static int http_check_cb(wsl_sck_t *sck, void *user_data, void *proto_data)
{
    http_client_t *c = (http_client_t *)user_data;

    MRP_UNUSED(c);
    MRP_UNUSED(sck);
    MRP_UNUSED(user_data);
    MRP_UNUSED(proto_data);

    return FALSE;
}


static void http_done_cb(wsl_sck_t *sck, const char *uri, void *user_data,
                         void *proto_data)
{
    http_client_t *c = (http_client_t *)user_data;

    MRP_UNUSED(proto_data);

    mrp_debug("HTTP request for '%s' done, closing socket %p.", uri, sck);

    http_destroy_client(c);
}


MRP_REGISTER_TRANSPORT(wsck, WSCKP, wsck_t, wsck_resolve,
                       wsck_open, wsck_createfrom, wsck_close, wsck_setopt,
                       wsck_bind, wsck_listen, wsck_accept,
                       wsck_connect, wsck_disconnect,
                       wsck_send, NULL,
                       wsck_sendraw, NULL,
                       wsck_senddata, NULL,
                       wsck_sendcustom, NULL,
                       NULL, NULL,
                       NULL, NULL);
