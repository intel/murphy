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

#ifndef __MURPHY_DOMAIN_CONTROL_MESSAGE_H__
#define __MURPHY_DOMAIN_CONTROL_MESSAGE_H__

#include <murphy/common/msg.h>
#include <murphy/common/json.h>
#include <murphy-db/mql.h>

#include "domain-control-types.h"

typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_REGISTER,
    MSG_TYPE_UNREGISTER,
    MSG_TYPE_SET,
    MSG_TYPE_NOTIFY,
    MSG_TYPE_ACK,
    MSG_TYPE_NAK,
    MSG_TYPE_INVOKE,
    MSG_TYPE_RETURN,
} msg_type_t;

typedef enum {
    /* fixed common tags */
    MSGTAG_MSGTYPE = 0x1,            /* message type */
    MSGTAG_MSGSEQ  = 0x2,            /* sequence number */

    /* fixed tags in registration messages */
    MSGTAG_NAME     = 0x3,           /* enforcement point name */
    MSGTAG_NTABLE   = 0x4,           /* number of owned tables */
    MSGTAG_NWATCH   = 0x5,           /* number of watched tables */
    MSGTAG_TBLNAME  = 0x6,           /* table name */
    MSGTAG_COLUMNS  = 0x8,           /* column definitions/list */
    MSGTAG_INDEX    = 0x9,           /* index definition */
    MSGTAG_WHERE    = 0xa,           /* where clause for select */
    MSGTAG_MAXROWS  = 0xb,           /* max number of rows to select */

    /* fixed tags in NAKs */
    MSGTAG_ERRCODE  = 0x3,           /* error code */
    MSGTAG_ERRMSG   = 0x4,           /* error message */

    /* fixed tags in data notification messages */
    MSGTAG_NCHANGE = 0x3,            /* number of tables in notification */
    MSGTAG_NTOTAL  = 0x4,            /* total columns in notification */
    MSGTAG_TBLID   = 0x5,            /* table id */
    MSGTAG_NROW    = 0x6,            /* number of table rows */
    MSGTAG_NCOL    = 0x7,            /* number of columns in a row */
    MSGTAG_DATA    = 0x8,            /* a data column */

    /* fixed tags in invoke and return messages */
    MSGTAG_METHOD  = 0x3,            /* method name */
    MSGTAG_NORET   = 0x4,            /* whether return values ignored */
    MSGTAG_NARG    = 0x5,            /* number of arguments */
    MSGTAG_ARG     = 0x6,            /* argument */
    MSGTAG_ERROR   = 0x7,            /* invocation error */
    MSGTAG_RETVAL  = 0x8,            /* invocation return value */
} msgtag_t;


#define MSG_UINT8(tag, val) MRP_MSG_TAG_UINT8(MSGTAG_##tag, val)
#define MSG_SINT8(tag, val) MRP_MSG_TAG_SINT8(MSGTAG_##tag, val)
#define MSG_UINT16(tag, val) MRP_MSG_TAG_UINT16(MSGTAG_##tag, val)
#define MSG_SINT16(tag, val) MRP_MSG_TAG_SINT16(MSGTAG_##tag, val)
#define MSG_UINT32(tag, val) MRP_MSG_TAG_UINT32(MSGTAG_##tag, val)
#define MSG_SINT32(tag, val) MRP_MSG_TAG_SINT32(MSGTAG_##tag, val)
#define MSG_UINT64(tag, val) MRP_MSG_TAG_UINT64(MSGTAG_##tag, val)
#define MSG_SINT64(tag, val) MRP_MSG_TAG_SINT64(MSGTAG_##tag, val)
#define MSG_DOUBLE(tag, val) MRP_MSG_TAG_DOUBLE(MSGTAG_##tag, val)
#define MSG_STRING(tag, val) MRP_MSG_TAG_STRING(MSGTAG_##tag, val)
#define MSG_BOOL(tag, val) MRP_MSG_TAG_BOOL(MSGTAG_##tag, val)
#define MSG_ANY(tag, typep, valp) MRP_MSG_TAG_ANY(MSGTAG_##tag, typep, valp)
#define MSG_ARRAY(tag, type, size, arr) \
    MRP_MSG_TAGGED(MSGTAG_##tag, type, size, arr)

#define MSG_END MRP_MSG_END

#define COMMON_MSG_FIELDS                /* common message fields */      \
    msg_type_t  type;                    /* message type */               \
    uint32_t    seq;                     /* message sequence number */    \
    void       *wire;                    /* associated on-wire message */ \
    void      (*unref_wire)(void *)      /* function to unref message */



typedef struct {
    COMMON_MSG_FIELDS;
    char               *name;            /* domain controller name */
    mrp_domctl_table_t *tables;          /* owned tables */
    int                 ntable;          /* number of tables */
    mrp_domctl_watch_t *watches;         /* watched tables */
    int                 nwatch;          /* number of watches */
} register_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
} unregister_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
    mrp_domctl_data_t  *tables;          /* data for tables to set */
    int                 ntable;          /* number of tables */
} set_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
    mrp_domctl_data_t *tables;           /* data in changed tables */
    int                ntable;           /* number of changed tables */
} notify_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
} ack_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
    int32_t     error;
    const char *msg;
} nak_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
    const char       *name;
    int               noret;
    uint32_t          narg;
    mrp_domctl_arg_t *args;
} invoke_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
    uint32_t          error;
    int32_t           retval;
    uint32_t          narg;
    mrp_domctl_arg_t *args;
} return_msg_t;


typedef struct {
    COMMON_MSG_FIELDS;
} any_msg_t;


union msg_u {
    any_msg_t        any;
    register_msg_t   reg;
    unregister_msg_t unreg;
    set_msg_t        set;
    notify_msg_t     notify;
    ack_msg_t        ack;
    nak_msg_t        nak;
    invoke_msg_t     invoke;
    return_msg_t     ret;
};


mrp_msg_t *msg_encode_message(msg_t *msg);
msg_t *msg_decode_message(mrp_msg_t *msg);
mrp_json_t *json_encode_message(msg_t *msg);
msg_t *json_decode_message(mrp_json_t *msg);
void msg_free_message(msg_t *msg);

mrp_msg_t *msg_create_notify(void);
int msg_update_notify(mrp_msg_t *msg, int tblid, mql_result_t *r);

mrp_json_t *json_create_notify(void);
int json_update_notify(mrp_json_t *msg, int tblid, mql_result_t *r);

#endif /* __MURPHY_DOMAIN_CONTROL_MESSAGE_H__ */
