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

#ifndef __MURPHY_WEBSOCKET_TRANSPORT_H__
#define __MURPHY_WEBSOCKET_TRANSPORT_H__

#include <murphy/common/macros.h>
#include <murphy/common/transport.h>

MRP_CDECL_BEGIN

#define MRP_AF_WSCK 0xDC                 /* stolen address family */


/*
 * websocket transport address
 */

#define MRP_WSCKADDR_BASE                                               \
    __SOCKADDR_COMMON(wsck_);            /* wsck_family: MRP_AF_WSCK */ \
    union {                              /* websocket address */        \
        sa_family_t         family;                                     \
        struct sockaddr_in  v4;                                         \
        struct sockaddr_in6 v6;                                         \
    } wsck_addr                                                         \

typedef struct {
    MRP_WSCKADDR_BASE;
} _mrp_wsckaddr_base_t;


#define MRP_WSCK_DEFPROTO "murphy"
#define MRP_WSCK_PROTOLEN (MRP_SOCKADDR_SIZE - sizeof(_mrp_wsckaddr_base_t))


typedef struct {
    MRP_WSCKADDR_BASE;                   /* websocket address */
    char wsck_proto[MRP_WSCK_PROTOLEN];  /* websocket protocol */
} mrp_wsckaddr_t;


/*
 * websocket transport options and values
 */

#define MRP_WSCK_OPT_SENDMODE    "send-mode"  /* sendmode option name */
#define MRP_WSCK_SENDMODE_TEXT   "text"       /* sendmode text option */
#define MRP_WSCK_SENDMODE_BINARY "binary"     /* sendmode blob option */


#define MRP_WSCK_OPT_HTTPDIR  "http-dir"      /* HTTP content root */
#define MRP_WSCK_OPT_MIMEMAP  "mime-map"      /* suffix-MIME table */
#define MRP_WSCK_OPT_URIMAP   "uri-map"       /* URI-path table */
#define MRP_WSCK_OPT_SSL_CERT "ssl-cert"      /* path to SSL certificate */
#define MRP_WSCK_OPT_SSL_PKEY "ssl-pkey"      /* path to SSL priv. key */
#define MRP_WSCK_OPT_SSL_CA   "ssl-ca"        /* path to SSL CA */
#define MRP_WSCK_OPT_SSL      "ssl"           /* whether to connect with SSL */

/*
 * It is also possible to serve content over HTTP on a websocket transport.
 *
 * This is primarily intended for serving javascript API libraries to
 * clients talking to you via the same websocket transport. The served
 * libraries hide the details of the underlying communication protocol
 * and present a more developer-friendly conventional javascript API.
 *
 * Currently the websocket transport provides two mechanisms for
 * configuring HTTP content serving.
 *
 * 1) You can put all the files you're willing to expose via HTTP to a
 *    dedicated directory and configure it to the transport as the
 *    MRP_WSCK_OPT_HTTPROOT option. If you serve any other types of
 *    files than HTML (*.htm, *.html), javascript (*.js), or text
 *    (*.txt) files than you should also push down a table to map
 *    the extra file suffices to MIME types. You can do this using
 *    the MRP_SCK_OPT_MIMEMAP transport option.
 *
 * 2) You can use a mapping table that maps URIs to file path / mime
 *    type pairs. You can push this table down to the transport as
 *    the MRP_WSCK_URIMAP transport option.
 *
 *  HTTPROOT takes a char *, URIMAP takes a mrp_wsck_urimap_t *, and
 *  MIMEMAP takes a mrp_wsck_mimemap_t * as their values. Both URI
 *  and MIME type tables need to be NULL-terminated. If you set both
 *  HTTPROOT and URIMAP, URIMAP entries with relative path names will
 *  be treated relative to HTTPROOT.
 *
 * Notes:
 *
 *     If you push down any of these options, the websocket backend
 *     will use the provided values as such __without__ making an
 *     internal copy. IOW, you better make sure that the passed values
 *     are valid throughout the full lifetime of the transport (and
 *     if that is a transport you listen on also the lifetime of all
 *     transports accepted on that transport) otherwise you'll end up
 *     with severe memory corruption.
 *
 */

typedef struct {
    const char *uri;                     /* exported URI */
    const char *path;                    /* path to file */
    const char *type;                    /* MIME type to use */
} mrp_wsck_urimap_t;

typedef struct {
    const char *suffix;                  /* filename suffix */
    const char *type;                    /* MIME type */
} mrp_wsck_mimemap_t;




MRP_CDECL_END

#endif /* __MURPHY_WEBSOCKET_TRANSPORT_H__ */
