#ifndef __MURPHY_WEBSOCKET_H__
#define __MURPHY_WEBSOCKET_H__

#include <murphy/common/macros.h>
#include <murphy/common/websocklib.h>

MRP_CDECL_BEGIN

/*
 * websocket types (mapped)
 */

typedef wsl_ctx_t       mrp_websock_context_t;
typedef wsl_sck_t       mrp_websock_t;
typedef wsl_callbacks_t mrp_websock_evt_t;
typedef wsl_proto_t     mrp_websock_proto_t;

/*
 * websocket log levels (mapped)
 */

typedef enum {
#define MAP(mrp, wsl) MRP_WEBSOCK_LOG_##mrp = WSL_LOG_##wsl
    MAP(NONE   , NONE),
    MAP(ERROR  , ERROR),
    MAP(WARNING, WARNING),
    MAP(INFO   , INFO),
    MAP(DEBUG  , DEBUG),
    MAP(ALL    , ALL),
    MAP(PARSER , PARSER),
    MAP(EXT    , EXT),
    MAP(CLIENT , CLIENT),
    MAP(EXTRA  , EXTRA),
    MAP(VERBOSE, VERBOSE)
#undef MAP
} mrp_websock_loglevel_t;



/*
 * websocket function prototypes
 */

/** Set websocket logging level. */
void mrp_websock_set_loglevel(mrp_websock_loglevel_t mask);

/** Create a websocket context. */
mrp_websock_context_t *mrp_websock_create_context(mrp_mainloop_t *ml,
                                                  struct sockaddr *sa,
                                                  mrp_websock_proto_t *proto,
                                                  int nproto,
                                                  void *user_data);

/** Add a reference to a websocket context. */
mrp_websock_context_t *mrp_websock_ref_context(mrp_websock_context_t *ctx);

/** Remove a context reference. */
int mrp_websock_unref_context(mrp_websock_context_t *ctx);

/** Create and connect a websocket to a given address. */
mrp_websock_t *mrp_websock_connect(mrp_websock_context_t *ctx,
                                   struct sockaddr *sa, const char *protocol,
                                   void *user_data);

/** Accept a pending connection of a context. */
mrp_websock_t *mrp_websock_accept_pending(mrp_websock_context_t *ctx,
                                          void *user_data);

/** Reject a pending connection of a context. */
void mrp_websock_reject_pending(mrp_websock_context_t *ctx);

/** Close a websocket. Return the user_data of it's associated context. */
void *mrp_websock_close(mrp_websock_t *sck);

/** Send data over a connected websocket. */
int mrp_websock_send(mrp_websock_t *sck, void *payload, size_t size);

/** Serve the given file, with MIME type, over the given websocket. */
int mrp_websock_server_http_file(mrp_websock_t *sck, const char *path,
                                 const char *mime);

MRP_CDECL_END


#endif /* __MURPHY_WEBSOCKET_H__ */
