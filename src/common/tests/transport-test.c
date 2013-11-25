/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>


/*
 * tags for generic message fields
 */

#define TAG_SEQ   ((uint16_t)0x1)
#define TAG_MSG   ((uint16_t)0x2)
#define TAG_U8    ((uint16_t)0x3)
#define TAG_S8    ((uint16_t)0x4)
#define TAG_U16   ((uint16_t)0x5)
#define TAG_S16   ((uint16_t)0x6)
#define TAG_DBL   ((uint16_t)0x7)
#define TAG_BLN   ((uint16_t)0x8)
#define TAG_ASTR  ((uint16_t)0x9)
#define TAG_AU32  ((uint16_t)0xa)
#define TAG_RPL   ((uint16_t)0xb)
#define TAG_END   MRP_MSG_FIELD_END

#define U32_GUARD (uint32_t)-1

/*
 * our test custom data type
 */

#define TAG_CUSTOM 0x1

typedef struct {
    uint32_t   seq;
    char      *msg;
    uint8_t     u8;
    int8_t      s8;
    uint16_t   u16;
    int16_t    s16;
    double     dbl;
    bool       bln;
    char     **astr;
    uint32_t   nstr;
    uint32_t   fsck;
    uint32_t  *au32;
    char      *rpl;
} custom_t;


typedef custom_t native_t;

static uint32_t native_id;

MRP_DATA_DESCRIPTOR(custom_descr, TAG_CUSTOM, custom_t,
                    MRP_DATA_MEMBER(custom_t,  seq, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_MEMBER(custom_t,  msg, MRP_MSG_FIELD_STRING),
                    MRP_DATA_MEMBER(custom_t,   u8, MRP_MSG_FIELD_UINT8 ),
                    MRP_DATA_MEMBER(custom_t,   s8, MRP_MSG_FIELD_SINT8 ),
                    MRP_DATA_MEMBER(custom_t,  u16, MRP_MSG_FIELD_UINT16),
                    MRP_DATA_MEMBER(custom_t,  s16, MRP_MSG_FIELD_SINT16),
                    MRP_DATA_MEMBER(custom_t,  dbl, MRP_MSG_FIELD_DOUBLE),
                    MRP_DATA_MEMBER(custom_t,  bln, MRP_MSG_FIELD_BOOL  ),
                    MRP_DATA_MEMBER(custom_t,  rpl, MRP_MSG_FIELD_STRING),
                    MRP_DATA_MEMBER(custom_t, nstr, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_MEMBER(custom_t, fsck, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_ARRAY_COUNT(custom_t, astr, nstr,
                                         MRP_MSG_FIELD_STRING),
                    MRP_DATA_ARRAY_GUARD(custom_t, au32, u32, U32_GUARD,
                                         MRP_MSG_FIELD_UINT32));

MRP_DATA_DESCRIPTOR(buggy_descr, TAG_CUSTOM, custom_t,
                    MRP_DATA_MEMBER(custom_t,  seq, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_MEMBER(custom_t,  msg, MRP_MSG_FIELD_STRING),
                    MRP_DATA_MEMBER(custom_t,   u8, MRP_MSG_FIELD_UINT8 ),
                    MRP_DATA_MEMBER(custom_t,   s8, MRP_MSG_FIELD_SINT8 ),
                    MRP_DATA_MEMBER(custom_t,  u16, MRP_MSG_FIELD_UINT16),
                    MRP_DATA_MEMBER(custom_t,  s16, MRP_MSG_FIELD_SINT16),
                    MRP_DATA_MEMBER(custom_t,  dbl, MRP_MSG_FIELD_DOUBLE),
                    MRP_DATA_MEMBER(custom_t,  bln, MRP_MSG_FIELD_BOOL  ),
                    MRP_DATA_MEMBER(custom_t,  rpl, MRP_MSG_FIELD_STRING),
                    MRP_DATA_MEMBER(custom_t, nstr, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_MEMBER(custom_t, fsck, MRP_MSG_FIELD_UINT32),
                    MRP_DATA_ARRAY_COUNT(custom_t, astr, fsck,
                                         MRP_MSG_FIELD_STRING),
                    MRP_DATA_ARRAY_GUARD(custom_t, au32, u32, U32_GUARD,
                                         MRP_MSG_FIELD_UINT32));



mrp_data_descr_t *data_descr;


typedef enum {
    MODE_DEFAULT = 0,
    MODE_MESSAGE = 1,
    MODE_DATA    = 2,
    MODE_RAW     = 3,
    MODE_NATIVE  = 4,
} msg_mode_t;


typedef struct {
    mrp_mainloop_t  *ml;
    mrp_transport_t *lt, *t;
    char            *addrstr;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    const char      *atype;
    int              server;
    int              sock;
    mrp_io_watch_t  *iow;
    mrp_timer_t     *timer;
    int              mode;
    int              buggy;
    int              connect;
    int              stream;
    int              log_mask;
    const char      *log_target;
    uint32_t         seqno;
} context_t;


void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);
void recvfrom_msg(mrp_transport_t *t, mrp_msg_t *msg, mrp_sockaddr_t *addr,
                  socklen_t addrlen, void *user_data);

void recv_data(mrp_transport_t *t, void *data, uint16_t tag, void *user_data);
void recvfrom_data(mrp_transport_t *t, void *data, uint16_t tag,
                   mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data);

void recvraw(mrp_transport_t *t, void *data, size_t size, void *user_data);
void recvrawfrom(mrp_transport_t *t, void *data, size_t size,
                 mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data);


void dump_msg(mrp_msg_t *msg, FILE *fp)
{
    mrp_msg_dump(msg, fp);
}


void recvfrom_msg(mrp_transport_t *t, mrp_msg_t *msg, mrp_sockaddr_t *addr,
                  socklen_t addrlen, void *user_data)
{
    context_t       *c = (context_t *)user_data;
    mrp_msg_field_t *f;
    uint32_t         seq;
    char             buf[256];
    int              status;

    mrp_log_info("received a message");
    dump_msg(msg, stdout);

    if (c->server) {
        seq = 0;
        if ((f = mrp_msg_find(msg, TAG_SEQ)) != NULL) {
            if (f->type == MRP_MSG_FIELD_UINT32)
                seq = f->u32;
        }

        snprintf(buf, sizeof(buf), "reply to message #%u", seq);

        if (!mrp_msg_append(msg, TAG_RPL, MRP_MSG_FIELD_STRING, buf,
                            TAG_END)) {
            mrp_log_info("failed to append to received message");
            exit(1);
        }

        if (c->connect)
            status = mrp_transport_send(t, msg);
        else
            status = mrp_transport_sendto(t, msg, addr, addrlen);

        if (status)
            mrp_log_info("reply successfully sent");
        else
            mrp_log_error("failed to send reply");

        /* message unreffed by transport layer */
    }
}


void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(t, msg, NULL, 0, user_data);
}


void dump_custom(custom_t *msg, FILE *fp)
{
    uint32_t i;

    mrp_data_dump(msg, data_descr, fp);
    fprintf(fp, "{\n");
    fprintf(fp, "    seq = %u\n"  , msg->seq);
    fprintf(fp, "    msg = '%s'\n", msg->msg);
    fprintf(fp, "     u8 = %u\n"  , msg->u8);
    fprintf(fp, "     s8 = %d\n"  , msg->s8);
    fprintf(fp, "    u16 = %u\n"  , msg->u16);
    fprintf(fp, "    s16 = %d\n"  , msg->s16);
    fprintf(fp, "    dbl = %f\n"  , msg->dbl);
    fprintf(fp, "    bln = %s\n"  , msg->bln ? "true" : "false");
    fprintf(fp, "   astr = (%u)\n", msg->nstr);
    for (i = 0; i < msg->nstr; i++)
        fprintf(fp, "           %s\n", msg->astr[i]);
    fprintf(fp, "   au32 =\n");
    for (i = 0; msg->au32[i] != U32_GUARD; i++)
        fprintf(fp, "           %u\n", msg->au32[i]);
    fprintf(fp, "    rpl = '%s'\n", msg->rpl);
    fprintf(fp, "}\n");
}


void free_custom(custom_t *msg)
{
    mrp_data_free(msg, data_descr->tag);
}


void recvfrom_data(mrp_transport_t *t, void *data, uint16_t tag,
                   mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    context_t *c   = (context_t *)user_data;
    custom_t  *msg = (custom_t *)data;
    custom_t   rpl;
    char       buf[256];
    uint32_t   au32[] = { 9, 8, 7, 6, 5, -1 };
    int        status;

    mrp_log_info("received custom message of type 0x%x", tag);
    dump_custom(data, stdout);

    if (tag != data_descr->tag) {
        mrp_log_error("Tag 0x%x != our custom type (0x%x).",
                      tag, data_descr->tag);
        exit(1);
    }

    if (c->server) {
        rpl = *msg;
        snprintf(buf, sizeof(buf), "reply to message #%u", msg->seq);
        rpl.rpl  = buf;
        rpl.au32 = au32;

        if (c->connect)
            status = mrp_transport_senddata(t, &rpl, data_descr->tag);
        else
            status = mrp_transport_senddatato(t, &rpl, data_descr->tag,
                                              addr, addrlen);
        if (status)
            mrp_log_info("reply successfully sent");
        else
            mrp_log_error("failed to send reply");
    }

    free_custom(msg);
}


void recv_data(mrp_transport_t *t, void *data, uint16_t tag, void *user_data)
{
    recvfrom_data(t, data, tag, NULL, 0, user_data);
}


void dump_raw(void *data, size_t size, FILE *fp)
{
    int len = (int)size;

    fprintf(fp, "[%*.*s]\n", len, len, (char *)data);
}


void recvfrom_raw(mrp_transport_t *t, void *data, size_t size,
                  mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    context_t *c   = (context_t *)user_data;
    char       rpl[256];
    size_t     rpl_size;
    int        status;

    rpl_size = snprintf(rpl, sizeof(rpl), "reply to message [%*.*s]",
                        (int)size, (int)size, (char *)data);

    mrp_log_info("received raw message");
    dump_raw(data, size, stdout);

    if (strncmp((char *)data, "reply to ", 9) != 0) {
        if (c->connect)
            status = mrp_transport_sendraw(t, rpl, rpl_size);
        else
            status = mrp_transport_sendrawto(t, rpl, rpl_size, addr, addrlen);

        if (status)
            mrp_log_info("reply successfully sent");
        else
            mrp_log_error("failed to send reply");
    }
}


void recv_raw(mrp_transport_t *t, void *data, size_t size, void *user_data)
{
    recvfrom_raw(t, data, size, NULL, 0, user_data);
}


void free_native(native_t *msg)
{
    mrp_free_native(msg, native_id);
}


void recvfrom_native(mrp_transport_t *t, void *data, uint32_t type_id,
                     mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    context_t *c   = (context_t *)user_data;
    native_t  *msg = (native_t *)data;
    native_t   rpl;
    char       buf[256];
    uint32_t   au32[] = { 9, 8, 7, 6, 5, -1 };
    int        status;

    mrp_log_info("received native message of type 0x%x", type_id);
    dump_custom(data, stdout);

    if (type_id != native_id) {
        mrp_log_error("Received type 0x%x, expected 0x%x.", type_id, native_id);
        exit(1);
    }

    if (c->server) {
        rpl = *msg;
        snprintf(buf, sizeof(buf), "reply to message #%u", msg->seq);
        rpl.rpl  = buf;
        rpl.au32 = au32;

        if (c->connect)
            status = mrp_transport_sendnative(t, &rpl, native_id);
        else
            status = mrp_transport_sendnativeto(t, &rpl, native_id,
                                                addr, addrlen);
        if (status)
            mrp_log_info("reply successfully sent");
        else
            mrp_log_error("failed to send reply");
    }

    free_native(msg);
}


void recv_native(mrp_transport_t *t, void *data, uint32_t type_id,
                 void *user_data)
{
    recvfrom_native(t, data, type_id, NULL, 0, user_data);
}


void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(c);

    if (error) {
        mrp_log_error("Connection closed with error %d (%s).", error,
                      strerror(error));
        exit(1);
    }
    else {
        mrp_log_info("Peer has closed the connection.");
        exit(0);
    }
}


void connection_evt(mrp_transport_t *lt, void *user_data)
{
    context_t *c = (context_t *)user_data;
    int        flags;

    flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
    c->t = mrp_transport_accept(lt, c, flags);

    if (c->t == NULL) {
        mrp_log_error("Failed to accept new connection.");
        exit(1);
    }
}


void type_init(context_t *c)
{
    if (c->buggy && c->server) {
        data_descr = &buggy_descr;
        mrp_log_info("Deliberately using buggy data descriptor...");
    }
    else
        data_descr = &custom_descr;

    if (!mrp_msg_register_type(data_descr)) {
        mrp_log_error("Failed to register custom data type.");
        exit(1);
    }
}


void register_native(void)
{
    MRP_NATIVE_TYPE(native_type, native_t,
                    MRP_UINT32(native_t, seq        , DEFAULT),
                    MRP_STRING(native_t, msg        , DEFAULT),
                    MRP_UINT8 (native_t, u8         , DEFAULT),
                    MRP_INT8  (native_t, s8         , DEFAULT),
                    MRP_UINT16(native_t, u16        , DEFAULT),
                    MRP_INT16 (native_t, s16        , DEFAULT),
                    MRP_DOUBLE(native_t, dbl        , DEFAULT),
                    MRP_BOOL  (native_t, bln        , DEFAULT),
                    MRP_ARRAY (native_t, astr       , DEFAULT, SIZED,
                               char *, nstr),
                    MRP_UINT32(native_t, nstr       , DEFAULT),
                    MRP_ARRAY (native_t, au32       , DEFAULT, GUARDED,
                               uint32_t, "", .u32 = -1),
                    MRP_STRING(native_t, rpl        , DEFAULT));


    if ((native_id = mrp_register_native(&native_type)) != MRP_INVALID_TYPE)
        mrp_log_info("Successfully registered native type 'native_t'.");
    else {
        mrp_log_error("Failed to register native type 'native_t'.");
        exit(1);
    }
}


void server_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
        { .recvmsg     = NULL },
        { .recvmsgfrom = NULL },
        .closed        = NULL,
        .connection    = NULL,
    };

    int flags;

    type_init(c);

    switch (c->mode) {
    case MODE_DATA:
        evt.recvdata     = recv_data;
        evt.recvdatafrom = recvfrom_data;
        break;
    case MODE_RAW:
        evt.recvraw      = recv_raw;
        evt.recvrawfrom  = recvfrom_raw;
        break;
    case MODE_NATIVE:
        evt.recvnative     = recv_native;
        evt.recvnativefrom = recvfrom_native;
        break;
    case MODE_MESSAGE:
    default:
        evt.recvmsg      = recv_msg;
        evt.recvmsgfrom  = recvfrom_msg;
    }

    if (c->stream) {
        evt.connection = connection_evt;
        evt.closed     = closed_evt;
    }

    flags = MRP_TRANSPORT_REUSEADDR;

    switch (c->mode) {
    case MODE_DATA:    flags |= MRP_TRANSPORT_MODE_DATA;   break;
    case MODE_RAW:     flags |= MRP_TRANSPORT_MODE_RAW;    break;
    case MODE_NATIVE:  flags |= MRP_TRANSPORT_MODE_NATIVE; break;
    default:
    case MODE_MESSAGE: flags |= MRP_TRANSPORT_MODE_MSG;
    }

    c->lt = mrp_transport_create(c->ml, c->atype, &evt, c, flags);

    if (c->lt == NULL) {
        mrp_log_error("Failed to create listening server transport.");
        exit(1);
    }

    if (!mrp_transport_bind(c->lt, &c->addr, c->alen)) {
        mrp_log_error("Failed to bind transport to address %s.", c->addrstr);
        exit(1);
    }

    if (c->stream) {
        if (!mrp_transport_listen(c->lt, 0)) {
            mrp_log_error("Failed to listen on server transport.");
            exit(1);
        }
    }
}


void send_msg(context_t *c)
{
    mrp_msg_t *msg;
    uint32_t   seq;
    char       buf[256];
    char      *astr[] = { "this", "is", "an", "array", "of", "strings" };
    uint32_t   au32[] = { 1, 2, 3,
                          1 << 16, 2 << 16, 3 << 16,
                          1 << 24, 2 << 24, 3 << 24 };
    uint32_t   nstr = MRP_ARRAY_SIZE(astr);
    uint32_t   nu32 = MRP_ARRAY_SIZE(au32);
    int        status;

    seq = c->seqno++;
    snprintf(buf, sizeof(buf), "this is message #%u", (unsigned int)seq);

    msg = mrp_msg_create(TAG_SEQ , MRP_MSG_FIELD_UINT32, seq,
                         TAG_MSG , MRP_MSG_FIELD_STRING, buf,
                         TAG_U8  , MRP_MSG_FIELD_UINT8 ,   seq & 0xf,
                         TAG_S8  , MRP_MSG_FIELD_SINT8 , -(seq & 0xf),
                         TAG_U16 , MRP_MSG_FIELD_UINT16,   seq,
                         TAG_S16 , MRP_MSG_FIELD_SINT16, - seq,
                         TAG_DBL , MRP_MSG_FIELD_DOUBLE, seq / 3.0,
                         TAG_BLN , MRP_MSG_FIELD_BOOL  , seq & 0x1,
                         TAG_ASTR, MRP_MSG_FIELD_ARRAY_OF(STRING), nstr, astr,
                         TAG_AU32, MRP_MSG_FIELD_ARRAY_OF(UINT32), nu32, au32,
                         TAG_END);

    if (msg == NULL) {
        mrp_log_error("Failed to create new message.");
        exit(1);
    }

    if (c->connect)
        status = mrp_transport_send(c->t, msg);
    else
        status = mrp_transport_sendto(c->t, msg, &c->addr, c->alen);

    if (!status) {
        mrp_log_error("Failed to send message #%d.", seq);
        exit(1);
    }
    else
        mrp_log_info("Message #%d succesfully sent.", seq);

    mrp_msg_unref(msg);
}


void send_data(context_t *c)
{
    uint32_t  seq = c->seqno++;
    custom_t  msg;
    char      buf[256];
    char     *astr[] = { "this", "is", "a", "test", "string", "array" };
    uint32_t  au32[] = { 1, 2, 3, 4, 5, 6, 7, -1 };
    int       status;

    msg.seq = seq;
    snprintf(buf, sizeof(buf), "this is message #%u", (unsigned int)seq);
    msg.msg  = buf;
    msg.u8   =   seq & 0xf;
    msg.s8   = -(seq & 0xf);
    msg.u16  =   seq;
    msg.s16  = - seq;
    msg.dbl  =   seq / 3.0;
    msg.bln  =   seq & 0x1;
    msg.astr = astr;
    msg.nstr = MRP_ARRAY_SIZE(astr);
    msg.fsck = 1000;
    msg.au32 = au32;
    msg.rpl  = "";

    if (c->connect)
        status = mrp_transport_senddata(c->t, &msg, data_descr->tag);
    else
        status = mrp_transport_senddatato(c->t, &msg, data_descr->tag,
                                          &c->addr, c->alen);

    if (!status) {
        mrp_log_error("Failed to send message #%d.", msg.seq);
        exit(1);
    }
    else
        mrp_log_info("Message #%d succesfully sent.", msg.seq);
}


void send_raw(context_t *c)
{
    uint32_t  seq = c->seqno++;
    char      msg[256];
    size_t    size;
    int       status;

    size = snprintf(msg, sizeof(msg), "this is message #%u", seq);

    if (c->connect)
        status = mrp_transport_sendraw(c->t, msg, size);
    else
        status = mrp_transport_sendrawto(c->t, msg, size, &c->addr, c->alen);

    if (!status) {
        mrp_log_error("Failed to send raw message #%d.", seq);
        exit(1);
    }
    else
        mrp_log_info("Message #%u succesfully sent.", seq);
}


void send_native(context_t *c)
{
    uint32_t  seq = c->seqno++;
    custom_t  msg;
    char      buf[256];
    char     *astr[] = { "this", "is", "a", "test", "string", "array" };
    uint32_t  au32[] = { 1, 2, 3, 4, 5, 6, 7, -1 };
    int       status;

    msg.seq = seq;
    snprintf(buf, sizeof(buf), "this is message #%u", (unsigned int)seq);
    msg.msg  = buf;
    msg.u8   =   seq & 0xf;
    msg.s8   = -(seq & 0xf);
    msg.u16  =   seq;
    msg.s16  = - seq;
    msg.dbl  =   seq / 3.0;
    msg.bln  =   seq & 0x1;
    msg.astr = astr;
    msg.nstr = MRP_ARRAY_SIZE(astr);
    msg.fsck = 1000;
    msg.au32 = au32;
    msg.rpl  = "";

    if (c->connect)
        status = mrp_transport_sendnative(c->t, &msg, native_id);
    else
        status = mrp_transport_sendnativeto(c->t, &msg, native_id,
                                            &c->addr, c->alen);

    if (!status) {
        mrp_log_error("Failed to send message #%d.", msg.seq);
        exit(1);
    }
    else
        mrp_log_info("Message #%d succesfully sent.", msg.seq);
}


void send_cb(mrp_timer_t *t, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);

    switch (c->mode) {
    case MODE_DATA:    send_data(c);   break;
    case MODE_RAW:     send_raw(c);    break;
    case MODE_NATIVE:  send_native(c); break;
    default:
    case MODE_MESSAGE: send_msg(c);
    }
}


void client_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
        { .recvmsg     = NULL },
        { .recvmsgfrom = NULL },
        .closed        = closed_evt,
        .connection    = NULL
    };

    int flags;

    type_init(c);

    switch (c->mode) {
    case MODE_DATA:
        evt.recvdata     = recv_data;
        evt.recvdatafrom = recvfrom_data;
        flags            = MRP_TRANSPORT_MODE_DATA;
        break;
    case MODE_RAW:
        evt.recvraw      = recv_raw;
        evt.recvrawfrom  = recvfrom_raw;
        flags            = MRP_TRANSPORT_MODE_RAW;
        break;
    case MODE_NATIVE:
        evt.recvnative     = recv_native;
        evt.recvnativefrom = recvfrom_native;
        flags              = MRP_TRANSPORT_MODE_NATIVE;
        break;
    default:
    case MODE_MESSAGE:
        evt.recvmsg      = recv_msg;
        evt.recvmsgfrom  = recvfrom_msg;
        flags            = MRP_TRANSPORT_MODE_MSG;
    }

    c->t = mrp_transport_create(c->ml, c->atype, &evt, c, flags);

    if (c->t == NULL) {
        mrp_log_error("Failed to create new transport.");
        exit(1);
    }

    if (!strcmp(c->atype, "unxd")) {
        char           addrstr[] = "unxd:@stream-test-client";
        mrp_sockaddr_t addr;
        socklen_t      alen;

        alen = mrp_transport_resolve(NULL, addrstr, &addr, sizeof(addr), NULL);
        if (alen <= 0) {
            mrp_log_error("Failed to resolve transport address '%s'.", addrstr);
            exit(1);
        }

        if (!mrp_transport_bind(c->t, &addr, alen)) {
            mrp_log_error("Failed to bind to transport address '%s'.", addrstr);
            exit(1);
        }
    }

    if (c->connect) {
        if (!mrp_transport_connect(c->t, &c->addr, c->alen)) {
            mrp_log_error("Failed to connect to %s.", c->addrstr);
            exit(1);
        }
    }


    c->timer = mrp_add_timer(c->ml, 1000, send_cb, c);

    if (c->timer == NULL) {
        mrp_log_error("Failed to create send timer.");
        exit(1);
    }
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    printf("usage: %s [options] [transport-address]\n\n"
           "The possible options are:\n"
           "  -s, --server                   run as test server (default)\n"
           "  -C, --connect                  connect transport\n"
           "      For connection-oriented transports, this is automatic.\n"
           "  -a, --address                  address to use\n"
           "  -c, --custom                   use custom messages\n"
           "  -m, --message                  use generic messages (default)\n"
           "  -r, --raw                      use raw messages\n"
           "  -n, --native                   use native messages\n"
           "  -b, --buggy                    use buggy data descriptors\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable debug messages\n"
           "  -h, --help                     show help on usage\n",
           argv0);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void config_set_defaults(context_t *ctx)
{
    mrp_clear(ctx);
    ctx->addrstr    = "tcp4:127.0.0.1:3000";
    ctx->server     = FALSE;
    ctx->log_mask   = MRP_LOG_UPTO(MRP_LOG_DEBUG);
    ctx->log_target = MRP_LOG_TO_STDERR;
}


int parse_cmdline(context_t *ctx, int argc, char **argv)
{
#   define OPTIONS "scmrnbCa:l:t:v:d:h"
    struct option options[] = {
        { "server"    , no_argument      , NULL, 's' },
        { "address"   , required_argument, NULL, 'a' },
        { "custom"    , no_argument      , NULL, 'c' },
        { "message"   , no_argument      , NULL, 'm' },
        { "raw"       , no_argument      , NULL, 'r' },
        { "native"    , no_argument      , NULL, 'n' },
        { "connect"   , no_argument      , NULL, 'C' },

        { "buggy"     , no_argument      , NULL, 'b' },
        { "log-level" , required_argument, NULL, 'l' },
        { "log-target", required_argument, NULL, 't' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "debug"     , required_argument, NULL, 'd' },
        { "help"      , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    config_set_defaults(ctx);

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 's':
            ctx->server = TRUE;
            break;

        case 'c':
            if (ctx->mode == MODE_DEFAULT)
                ctx->mode = MODE_DATA;
            else {
                mrp_log_error("Multiple modes requested.");
                exit(1);
            }
            break;

        case 'm':
            if (ctx->mode == MODE_DEFAULT)
                ctx->mode = MODE_MESSAGE;
            else {
                mrp_log_error("Multiple modes requested.");
                exit(1);
            }
            break;

        case 'r':
            if (ctx->mode == MODE_DEFAULT)
                ctx->mode = MODE_RAW;
            else {
                mrp_log_error("Multiple modes requested.");
                exit(1);
            }
            break;

        case 'n':
            if (ctx->mode == MODE_DEFAULT)
                ctx->mode = MODE_NATIVE;
            else {
                mrp_log_error("Multiple modes requested.");
                exit(1);
            }
            break;

        case 'b':
            ctx->buggy = TRUE;
            break;

        case 'C':
            ctx->connect = TRUE;
            break;

        case 'a':
            ctx->addrstr = optarg;
            break;

        case 'v':
            ctx->log_mask <<= 1;
            ctx->log_mask  |= 1;
            break;

        case 'l':
            ctx->log_mask = mrp_log_parse_levels(optarg);
            if (ctx->log_mask < 0)
                print_usage(argv[0], EINVAL, "invalid log level '%s'", optarg);
            break;

        case 't':
            ctx->log_target = mrp_log_parse_target(optarg);
            if (!ctx->log_target)
                print_usage(argv[0], EINVAL, "invalid log target '%s'", optarg);
            break;

        case 'd':
            ctx->log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_debug_set_config(optarg);
            mrp_debug_enable(TRUE);
            break;

        case 'h':
            print_usage(argv[0], -1, "");
            exit(0);
            break;

        default:
            print_usage(argv[0], EINVAL, "invalid option '%c'", opt);
        }
    }

    return TRUE;
}


int main(int argc, char *argv[])
{
    context_t c;

    if (!parse_cmdline(&c, argc, argv))
        exit(1);

    mrp_log_set_mask(c.log_mask);
    mrp_log_set_target(c.log_target);

    if (c.server)
        mrp_log_info("Running as server, using address '%s'...", c.addrstr);
    else
        mrp_log_info("Running as client, using address '%s'...", c.addrstr);

    switch (c.mode) {
    case MODE_DATA:    mrp_log_info("Using custom data messages..."); break;
    case MODE_RAW:     mrp_log_info("Using raw messages...");         break;
    case MODE_NATIVE:
        register_native();
        mrp_log_info("Using native messages...");
        break;
    default:
    case MODE_MESSAGE: mrp_log_info("Using generic messages...");
    }

    if (!strncmp(c.addrstr, "tcp", 3) || !strncmp(c.addrstr, "unxs", 4) ||
        !strncmp(c.addrstr, "wsck", 4)) {
        c.stream  = TRUE;
        c.connect = TRUE;
    }

    c.alen = mrp_transport_resolve(NULL, c.addrstr,
                                   &c.addr, sizeof(c.addr), &c.atype);
    if (c.alen <= 0) {
        mrp_log_error("Failed to resolve transport address '%s'.", c.addrstr);
        exit(1);
    }

    c.ml = mrp_mainloop_create();

    if (c.server)
        server_init(&c);
    else
        client_init(&c);

    mrp_mainloop_run(c.ml);

    return 0;
}
