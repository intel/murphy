#ifndef __MURPHY_DBUS_TRANSPORT_H__
#define __MURPHY_DBUS_TRANSPORT_H__

#include <murphy/common/transport.h>

#define MRP_AF_DBUS 0xDB

#define MRP_DBUSADDR_BASE                                                 \
    __SOCKADDR_COMMON(db_);                                               \
    char *db_bus;                               /* D-BUS bus address */   \
    char *db_addr;                                /* address on bus */    \
    char *db_path                                /* instance path */      \

typedef struct {
    MRP_DBUSADDR_BASE;
} _mrp_dbusaddr_base_t;


typedef struct {
    MRP_DBUSADDR_BASE;
    char db_fqa[MRP_SOCKADDR_SIZE - sizeof(_mrp_dbusaddr_base_t)];
} mrp_dbusaddr_t;



#endif /* __MURPHY_DBUS_TRANSPORT_H__ */
