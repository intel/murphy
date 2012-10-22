#ifndef __MURPHY_DOMAIN_CONTROL_MESSAGE_H__
#define __MURPHY_DOMAIN_CONTROL_MESSAGE_H__

#include <murphy/common/msg.h>

#include "domain-control-types.h"
#include "client.h"


#define MRP_PEPMSG_UINT16(tag, value) \
    MRP_MSG_TAG_UINT16(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_SINT16(tag, value) \
    MRP_MSG_TAG_SINT16(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_UINT32(tag, value) \
    MRP_MSG_TAG_UINT32(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_SINT32(tag, value) \
    MRP_MSG_TAG_SINT32(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_DOUBLE(tag, value) \
    MRP_MSG_TAG_DOUBLE(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_STRING(tag, value) \
    MRP_MSG_TAG_STRING(MRP_PEPTAG_##tag, value)

#define MRP_PEPMSG_ANY(tag, typep, valuep) \
    MRP_MSG_TAG_ANY(MRP_PEPTAG_##tag, typep, valuep)

/*
 * message types
 */

typedef enum {
    MRP_PEPMSG_REGISTER   = 0x1,         /* client: register me */
    MRP_PEPMSG_UNREGISTER = 0x2,         /* client: unregister me */
    MRP_PEPMSG_SET        = 0x3,         /* client: set table data */
    MRP_PEPMSG_NOTIFY     = 0x4,         /* server: table changes */
    MRP_PEPMSG_ACK        = 0x5,         /* server: ok */
    MRP_PEPMSG_NAK        = 0x6,         /* server: request failed */
} mrp_pepmsg_type_t;


/*
 * message-specific tags
 */

typedef enum {
    /*
     * fixed common tags
     */
    MRP_PEPTAG_MSGTYPE = 0x1,            /* message type */
    MRP_PEPTAG_MSGSEQ  = 0x2,            /* sequence number */

    /*
     * fixed tags in registration messages
     */
    MRP_PEPTAG_NAME     = 0x3,           /* enforcement point name */
    MRP_PEPTAG_NTABLE   = 0x4,           /* number of owned tables */
    MRP_PEPTAG_NWATCH   = 0x5,           /* number of watched tables */
    MRP_PEPTAG_TBLNAME  = 0x6,           /* table name */
    MRP_PEPTAG_COLUMNS  = 0x8,           /* column definitions/list */
    MRP_PEPTAG_INDEX    = 0x9,           /* index definition */
    MRP_PEPTAG_WHERE    = 0xa,           /* where clause for select */
    MRP_PEPTAG_MAXROWS  = 0xb,           /* max number of rows to select */

    /*
     * fixed tags in NAKs
     */
    MRP_PEPTAG_ERRCODE  = 0x3,           /* error code */
    MRP_PEPTAG_ERRMSG   = 0x4,           /* error message */

    /*
     * fixed tags in data notification messages
     */
    MRP_PEPTAG_NCHANGE = 0x3,            /* number of tables in notification */
    MRP_PEPTAG_NTOTAL  = 0x4,            /* total columns in notification */
    MRP_PEPTAG_TBLID   = 0x5,            /* table id */
    MRP_PEPTAG_NROW    = 0x6,            /* number of table rows */
    MRP_PEPTAG_NCOL    = 0x7,            /* number of columns in a row */
    MRP_PEPTAG_DATA    = 0x8,            /* a data column */
} mrp_pepmsg_tag_t;


mrp_msg_t *create_register_message(mrp_domctl_t *dc);
int decode_register_message(mrp_msg_t *msg,
                            mrp_domctl_table_t *tables, int ntable,
                            mrp_domctl_watch_t *watches, int nwatch);

mrp_msg_t *create_ack_message(uint32_t seq);
mrp_msg_t *create_nak_message(uint32_t seq, int error, const char *errmsg);
mrp_msg_t *create_notify_message(void);
int update_notify_message(mrp_msg_t *msg, int id, mqi_column_def_t *columns,
                          int ncolumn, mrp_domctl_value_t *data, int nrow);

mrp_msg_t *create_set_message(uint32_t seq, mrp_domctl_data_t *tables,
                              int ntable);

#endif /* __MURPHY_DOMAIN_CONTROL_MESSAGE_H__ */
