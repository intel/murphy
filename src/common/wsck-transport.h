#ifndef __MURPHY_WEBSOCKET_TRANSPORT_H__
#define __MURPHY_WEBSOCKET_TRANSPORT_H__

#include <murphy/common/macros.h>
#include <murphy/common/transport.h>

MRP_CDECL_BEGIN

#define MRP_AF_WSCK 0xDC                 /* stolen address family */

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


/*
 * websocket transport address
 */

typedef struct {
    MRP_WSCKADDR_BASE;                   /* websocket address */
    char wsck_proto[MRP_WSCK_PROTOLEN];  /* websocket protocol */
} mrp_wsckaddr_t;

MRP_CDECL_END

#endif /* __MURPHY_WEBSOCKET_TRANSPORT_H__ */
