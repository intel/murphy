#ifndef __MURPHY_SIGNALLING_PROTOCOL_H__
#define __MURPHY_SIGNALLING_PROTOCOL_H__

#include <stdint.h>
#include <murphy/common.h>


#define TAG_REGISTER         0x1
#define TAG_UNREGISTER       0x2 /* implicit with unix domain sockets */
#define TAG_POLICY_DECISION  0x3
#define TAG_ACK              0x4
#define TAG_ERROR            0x5

/* decision status */

#define EP_ACK               0x1
#define EP_NACK              0x2
#define EP_NOT_READY         0x3

typedef struct {
    char *ep_name;       /* EP name */
    uint32_t n_domains;   /* number of domains */
    char **domains;      /* array of domains */
} ep_register_t;

typedef struct {
    uint32_t id;         /* decision id */
    bool reply_required; /* if the EP must ACK/NACK the message */
    uint32_t n_rows;      /* number of rows */
    char **rows;         /* murphy-db database rows */
} ep_decision_t;

typedef struct {
    uint32_t id;         /* decision id */
    uint32_t success;    /* ACK/NACK/... */
} ep_ack_t;


MRP_DATA_DESCRIPTOR(ep_register_descr, TAG_REGISTER, ep_register_t,
        MRP_DATA_MEMBER(ep_register_t, ep_name, MRP_MSG_FIELD_STRING),
        MRP_DATA_MEMBER(ep_register_t, n_domains, MRP_MSG_FIELD_UINT32),
        MRP_DATA_ARRAY_COUNT(ep_register_t, domains, n_domains,
            MRP_MSG_FIELD_STRING));

MRP_DATA_DESCRIPTOR(ep_decision_descr, TAG_POLICY_DECISION, ep_decision_t,
        MRP_DATA_MEMBER(ep_decision_t, id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(ep_decision_t, reply_required, MRP_MSG_FIELD_BOOL),
        MRP_DATA_MEMBER(ep_decision_t, n_rows, MRP_MSG_FIELD_UINT32),
        MRP_DATA_ARRAY_COUNT(ep_decision_t, rows, n_rows,
            MRP_MSG_FIELD_STRING));

MRP_DATA_DESCRIPTOR(ep_ack_descr, TAG_ACK, ep_ack_t,
        MRP_DATA_MEMBER(ep_ack_t, id, MRP_MSG_FIELD_UINT32),
        MRP_DATA_MEMBER(ep_ack_t, success, MRP_MSG_FIELD_UINT32));

#endif /* __MURPHY_SIGNALLING_PROTOCOL_H__ */
