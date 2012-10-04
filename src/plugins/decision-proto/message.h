#ifndef __MURPHY_DECISION_MESSAGE_H__
#define __MURPHY_DECISION_MESSAGE_H__

#include <murphy/common/msg.h>

#include "decision-types.h"
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
    MRP_PEPTAG_NCOLDEF  = 0x6,           /* number of column definitions */
    MRP_PEPTAG_TBLNAME  = 0x7,           /* table name */
    MRP_PEPTAG_NCOLUMN  = 0x8,           /* number of columns */
    MRP_PEPTAG_TBLIDX   = 0x9,           /* column index of index column */
    MRP_PEPTAG_COLNAME  = 0xa,           /* column name */
    MRP_PEPTAG_COLTYPE  = 0xb,           /* column type */

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
    MRP_PEPTAG_DATA    = 0x7,            /* a data column */
} mrp_pepmsg_tag_t;


mrp_msg_t *create_register_message(mrp_pep_t *pep);
int decode_register_message(mrp_msg_t *msg, mrp_pep_table_t *tables, int ntable,
                            mrp_pep_table_t *watches, int nwatch,
                            mqi_column_def_t *columns, int ncolumn);

mrp_msg_t *create_ack_message(uint32_t seq);
mrp_msg_t *create_nak_message(uint32_t seq, int error, const char *errmsg);
mrp_msg_t *create_notify_message(void);
int update_notify_message(mrp_msg_t *msg, int id, mqi_column_def_t *columns,
                          int ncolumn, mrp_pep_value_t *data, int nrow);
int decode_notify_message(mrp_msg_t *msg, void **it, mrp_pep_data_t *t);

mrp_msg_t *create_set_message(uint32_t seq, mrp_pep_data_t *tables, int ntable);
int decode_set_message(mrp_msg_t *msg, void **it, mrp_pep_data_t *data);


#endif /* __MURPHY_DECISION_MESSAGE_H__ */
