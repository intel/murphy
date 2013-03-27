/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#ifndef __MURPHY_WEBSOCKLIB_H__
#define __MURPHY_WEBSOCKLIB_H__

#include <sys/socket.h>

#include <libwebsockets.h>

#include <murphy/common/macros.h>
#include <murphy/common/mainloop.h>

MRP_CDECL_BEGIN

/*
 * websocket context
 *
 * A websocket context is basically a libwebsocket_context plus the
 * additional glue data and code necessary to integrate the context
 * into our mainloop. For our transport abstraction, we create one
 * context per transport instance. However, accepted transports do
 * share their context with the listening transport (ie. the server-
 * side libwebsocket) they were accepted on.
 *
 * XXX TODO We probably need to change this so that we create one
 *    context per address/port (or in libwebsockets case device/port).
 *
 */

typedef struct wsl_ctx_s wsl_ctx_t;


/*
 * websocket
 *
 * A websocket is a libwebsocket instance together with its
 * associated websocket context.
 */
typedef struct wsl_sck_s wsl_sck_t;


/*
 * websocket event callbacks to the upper transport layer
 *
 * These callbacks are used to deliver events from the underlying
 * websocket transport layer to the upper murphy transport layer.
 */
typedef struct {
    /** Connection attempt on a websocket. */
    void (*connection)(wsl_ctx_t *ctx, char *addr, const char *protocol,
                       void *user_data, void *proto_data);
    /** Websocket connection closed by peer. */
    void (*closed)(wsl_sck_t *sck, int error, void *user_data,
                   void *proto_data);
    /** Data received on websocket. */
    void (*recv)(wsl_sck_t *sck, void *data, size_t size, void *user_data,
                 void *proto_data);
    /** Check if transport should be destroyed. */
    int  (*check)(wsl_sck_t *sck, void *user_data, void *proto_data);

    /** HTTP (content) request completed. */
    void (*http_done)(wsl_sck_t *sck, const char *uri, void *user_data,
                      void *proto_data);

#ifdef LWS_OPENSSL_SUPPORT
    /** Load extra client or server certificates, if necessary. */
    void (*load_certs)(wsl_ctx_t *ctx, SSL_CTX *ssl, int is_server);
#else
    void (*load_certs)(wsl_ctx_t *, void *, int);
#endif
} wsl_callbacks_t;


/*
 * websocket protocol
 *
 * A websocket protocol is a protocol name together with protocol-specific
 * upper-layer callbacks.
 */
typedef struct {
    const char      *name;               /* protocol name */
    wsl_callbacks_t  cbs;                /* event/request callbacks */
    int              framed;             /* whether a framed protocol */
    void            *proto_data;         /* protocol-specific user data */
} wsl_proto_t;


/*
 * websocket write modes
 */

typedef enum {
    WSL_SEND_TEXT   = LWS_WRITE_TEXT,    /* text mode */
    WSL_SEND_BINARY = LWS_WRITE_BINARY,  /* binary/blob mode */
#if 0
    WSL_SEND_HTTP   = LWS_WRITE_HTTP     /* HTTP mode */
#endif

#define WSL_SEND_TEXT WSL_SEND_TEXT

} wsl_sendmode_t;


/*
 * logging levels
 */

#ifndef WEBSOCKETS_OLD

typedef enum {
    WSL_LOG_NONE    = 0x0,
    WSL_LOG_ERROR   = LLL_ERR,
    WSL_LOG_WARNING = LLL_WARN,
    WSL_LOG_INFO    = LLL_INFO,
    WSL_LOG_DEBUG   = LLL_DEBUG,
    WSL_LOG_ALL     = LLL_ERR | LLL_WARN | LLL_INFO | LLL_DEBUG,
    WSL_LOG_PARSER  = LLL_PARSER,
    WSL_LOG_HEADER  = LLL_HEADER,
    WSL_LOG_EXT     = LLL_EXT,
    WSL_LOG_CLIENT  = LLL_CLIENT,
    WSL_LOG_EXTRA   = LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT,
    WSL_LOG_VERBOSE = WSL_LOG_ALL | WSL_LOG_EXTRA
} wsl_loglevel_t;

#else /* !WEBSOCKETS_OLD */

typedef enum {
    WSL_LOG_NONE    = 0x0,
    WSL_LOG_ERROR   = 0x0,
    WSL_LOG_WARNING = 0x0,
    WSL_LOG_INFO    = 0x0,
    WSL_LOG_DEBUG   = 0x0,
    WSL_LOG_ALL     = 0x0,
    WSL_LOG_PARSER  = 0x0,
    WSL_LOG_HEADER  = 0x0,
    WSL_LOG_EXT     = 0x0,
    WSL_LOG_CLIENT  = 0x0,
    WSL_LOG_EXTRA   = 0x0,
    WSL_LOG_VERBOSE = 0x0,
} wsl_loglevel_t;

#endif /* !WEBSOCKETS_OLD */

typedef enum {
    WSL_NO_SSL         = 0,              /* plain connection, no SSL */
    WSL_SSL            = 1,              /* SSL, deny self-signed certs */
    WSL_SSL_SELFSIGNED = 2,              /* SSL, allow self-signed certs */
} wsl_ssl_t;


/*
 * websockets context configuration
 */

#define WSL_NO_GID -1
#define WSL_NO_UID -1

typedef struct {
    struct sockaddr *addr;               /* address/port to listen on */
    wsl_proto_t     *protos;             /* protocols to serve */
    int              nproto;             /* number of protocols */
    const char      *ssl_cert;           /* SSL certificate path */
    const char      *ssl_pkey;           /* SSL private key path */
    const char      *ssl_ca;             /* SSL CA path */
    const char      *ssl_ciphers;        /* SSL cipher list */
    int              gid;                /* group ID to change to, or -1 */
    int              uid;                /* user ID to change to, or -1 */
    void            *user_data;          /* opaque user data */
    int              timeout;            /* keepalive timeout */
    int              nprobe;             /* number of keepalive probes */
    int              interval;           /* keepalive probe interval */
} wsl_ctx_cfg_t;


/** Set libwebsock logging level _and_ redirect to murphy logging infra. */
void wsl_set_loglevel(wsl_loglevel_t mask);

/** Create a websocket context. */
wsl_ctx_t *wsl_create_context(mrp_mainloop_t *ml, wsl_ctx_cfg_t *cfg);

/** Add a reference to a context. */
wsl_ctx_t *wsl_ref_context(wsl_ctx_t *ctx);

/** Remove a context reference, destroying it once the last is gone. */
int wsl_unref_context(wsl_ctx_t *ctx);

/** Create a new websocket connection using a given protocol. */
wsl_sck_t *wsl_connect(wsl_ctx_t *ctx, struct sockaddr *sa,
                       const char *protocol, wsl_ssl_t ssl, void *user_data);

/** Accept a pending connection. */
wsl_sck_t *wsl_accept_pending(wsl_ctx_t *ctx, void *user_data);

/** Reject a pending connection. */
void wsl_reject_pending(wsl_ctx_t *ctx);

/** Close a websocket connection. Return user_data of the associated context. */
void *wsl_close(wsl_sck_t *sck);

/** Set websocket write mode (binary or text). */
int wsl_set_sendmode(wsl_sck_t *sck, wsl_sendmode_t mode);

/** Send data over a wbesocket. */
int wsl_send(wsl_sck_t *sck, void *payload, size_t size);

/** Serve the given file over the given socket. */
int wsl_serve_http_file(wsl_sck_t *sck, const char *path, const char *mime);

MRP_CDECL_END

#endif /* __MURPHY_WEBSOCKLIB_H__ */
