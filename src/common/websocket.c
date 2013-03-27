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

#include <murphy/common/macros.h>
#include <murphy/common/websocket.h>


void mrp_websock_set_loglevel(mrp_websock_loglevel_t mask)
{
    wsl_set_loglevel(mask);
}


mrp_websock_context_t *mrp_websock_create_context(mrp_mainloop_t *ml,
                                                  mrp_websock_config_t *cfg)
{
    return wsl_create_context(ml, cfg);
}


mrp_websock_context_t *mrp_websock_ref_context(mrp_websock_context_t *ctx)
{
    return wsl_ref_context(ctx);
}


int mrp_websock_unref_context(mrp_websock_context_t *ctx)
{
    return wsl_unref_context(ctx);
}


mrp_websock_t *mrp_websock_connect(mrp_websock_context_t *ctx,
                                   struct sockaddr *sa, const char *protocol,
                                   mrp_wsl_ssl_t ssl, void *user_data)
{
    return wsl_connect(ctx, sa, protocol, ssl, user_data);
}


mrp_websock_t *mrp_websock_accept_pending(mrp_websock_context_t *ctx,
                                          void *user_data)
{
    return wsl_accept_pending(ctx, user_data);
}


void mrp_websock_reject_pending(mrp_websock_context_t *ctx)
{
    wsl_reject_pending(ctx);
}


void *mrp_websock_close(mrp_websock_t *sck)
{
    return wsl_close(sck);
}


int mrp_websock_send(mrp_websock_t *sck, void *payload, size_t size)
{
    return wsl_send(sck, payload, size);
}


int mrp_websock_server_http_file(mrp_websock_t *sck, const char *path,
                                 const char *mime)
{
    return wsl_serve_http_file(sck, path, mime);
}
