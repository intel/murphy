#ifndef __MURPHY_SIGNALLING_PLUGIN_H__
#define __MURPHY_SIGNALLING_PLUGIN_H__

#include <stdint.h>

#include <murphy/common.h>
#include <murphy/core.h>

typedef struct {
    const char *address;     /* socket address */
    const char *path;        /* socket file path */
    mrp_transport_t *t;      /* transport we're listening on */
    int sock;                /* main socket for new connections */
    mrp_io_watch_t *iow;     /* main socket I/O watch */
    mrp_context_t *ctx;      /* murphy context */
    mrp_htbl_t *txs;         /* active transactions */
    mrp_htbl_t *clients;     /* active clients */
    int n_clients;
    uint32_t next_id;
    mrp_sockaddr_t addr;
    socklen_t addrlen;
} data_t;




#endif /* __MURPHY_SIGNALLING_PLUGIN_H__ */
