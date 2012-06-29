#ifndef __MURPHY_SIGNALLING_CLIENT_H__
#define __MURPHY_SIGNALLING_CLIENT_H__

#include <stdint.h>

#include "plugin.h"
#include "transaction.h"

typedef struct {
    char *name;
    uint32_t ndomains;
    char **domains;
    mrp_transport_t *t;   /* associated transport */

    /* TODO: function pointers for handling the internal/external EP
     * case? */

    bool registered;      /* if the client is registered to server */
    data_t *u;
} client_t;


void deregister_and_free_client(client_t *c, data_t *ctx);
int send_policy_decision(data_t *ctx, client_t *c, transaction_t *tx);

void free_client(client_t *c);


int socket_setup(data_t *data);
int type_init(void);


#endif /* __MURPHY_SIGNALLING_CLIENT_H__ */
