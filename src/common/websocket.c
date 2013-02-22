#include <murphy/common/macros.h>
#include <murphy/common/websocket.h>


void mrp_websock_set_loglevel(mrp_websock_loglevel_t mask)
{
    wsl_set_loglevel(mask);
}


mrp_websock_context_t *mrp_websock_create_context(mrp_mainloop_t *ml,
                                                  struct sockaddr *sa,
                                                  mrp_websock_proto_t *proto,
                                                  int nproto,
                                                  const char *ssl_cert,
                                                  const char *ssl_pkey,
                                                  const char *ssl_ca,
                                                  void *user_data)
{
    return wsl_create_context(ml, sa, proto, nproto,
                              ssl_cert, ssl_pkey, ssl_ca,
                              user_data);
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
