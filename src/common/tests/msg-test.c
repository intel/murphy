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

#include <murphy/common.h>

#include <murphy/common/msg.h>
#include <murphy/common/msg.c>

#define TYPE(type, name) [MRP_MSG_FIELD_##type] = name
const char *types[] = {
    TYPE(INVALID, "invalid"),
    TYPE(STRING , "string" ),
    TYPE(BOOL   , "bool"   ),
    TYPE(SINT8  , "sint8"  ),
    TYPE(UINT8  , "uint8"  ),
    TYPE(SINT16 , "sint16" ),
    TYPE(UINT16 , "uint16" ),
    TYPE(SINT32 , "sint32" ),
    TYPE(UINT32 , "uint32" ),
    TYPE(SINT64 , "sint64" ),
    TYPE(UINT64 , "uint64" ),
    TYPE(DOUBLE , "double" ),
    TYPE(BLOB   , "blob"   ),
    NULL,
};
#undef TYPE


uint16_t get_type(const char **types, const char *name)
{
    const char **t;

    for (t = types; *t != NULL; t++) {
        if (!strcmp(*t, name))
            return (uint16_t)(t - types);
    }

    return MRP_MSG_FIELD_INVALID;
}


void test_default_encode_decode(int argc, char **argv)
{
    mrp_msg_t *msg, *decoded;
    void      *encoded;
    ssize_t    size;
    uint16_t   tag, type, prev_tag;
    uint8_t    u8;
    int8_t     s8;
    uint16_t   u16;
    int16_t    s16;
    uint32_t   u32;
    int32_t    s32;
    uint64_t   u64;
    int64_t    s64;
    double     dbl;
    bool       bln;
    char      *val, *end;
    int        i, ok;

    if ((msg = mrp_msg_create_empty()) == NULL) {
        mrp_log_error("Failed to create new message.");
        exit(1);
    }

    prev_tag = 0;
    i        = 1;
    while (i < argc) {

        if ('0' <= *argv[i] && *argv[i] <= '9') {
            if (argc <= i + 2) {
                mrp_log_error("Missing field type or value.");
                exit(1);
            }

            tag = prev_tag = (uint16_t)strtoul(argv[i++], &end, 0);
            if (end && *end) {
                mrp_log_error("Invalid field tag '%s'.", argv[i]);
                exit(1);
            }
        }
        else {
            if (argc <= i + 1) {
                mrp_log_error("Missing field type or value.");
                exit(1);
            }

            tag = ++prev_tag;
        }

        type = get_type(types, argv[i++]);
        val  = argv[i++];

        if (type == MRP_MSG_FIELD_INVALID) {
            mrp_log_error("Invalid field type '%s'.", argv[i + 1]);
            exit(1);
        }

        switch (type) {
        case MRP_MSG_FIELD_STRING:
            ok = mrp_msg_append(msg, tag, type, val);
            break;

        case MRP_MSG_FIELD_BOOL:
            if (!strcasecmp(val, "true"))
                bln = TRUE;
            else if (!strcasecmp(val, "false"))
                bln = FALSE;
            else {
                mrp_log_error("Invalid boolean value '%s'.", val);
                exit(1);
            }
            ok = mrp_msg_append(msg, tag, type, bln);
            break;

#define HANDLE_INT(_bits, _uget, _sget)                                   \
        case MRP_MSG_FIELD_UINT##_bits:                                   \
            u##_bits = (uint##_bits##_t)strtoul(val, &end, 0);            \
            if (end && *end) {                                            \
                mrp_log_error("Invalid uint%d value '%s'.", _bits, val);  \
                exit(1);                                                  \
            }                                                             \
            ok = mrp_msg_append(msg, tag, type, u##_bits);                \
            break;                                                        \
        case MRP_MSG_FIELD_SINT##_bits:                                   \
            s##_bits = (int##_bits##_t)strtol(val, &end, 0);              \
            if (end && *end) {                                            \
                mrp_log_error("Invalid sint%d value '%s'.", _bits, val);  \
                exit(1);                                                  \
            }                                                             \
            ok = mrp_msg_append(msg, tag, type, s##_bits);                \
            break

            HANDLE_INT(8 , strtol , strtoul);
            HANDLE_INT(16, strtol , strtoul);
            HANDLE_INT(32, strtol , strtoul);
            HANDLE_INT(64, strtoll, strtoull);

        case MRP_MSG_FIELD_DOUBLE:
            dbl = strtod(val, &end);
            if (end && *end) {
                mrp_log_error("Invalid double value '%s'.", val);
                exit(1);
            }
            ok = mrp_msg_append(msg, tag, type, dbl);
            break;

        default:
            mrp_log_error("Invalid (or unimplemented) type 0x%x (%s).",
                          type, argv[i + 1]);
            ok = FALSE;
        }

        if (!ok) {
            mrp_log_error("Failed to add field to message.");
            exit(1);
        }
    }

    mrp_msg_dump(msg, stdout);

    size = mrp_msg_default_encode(msg, &encoded);
    if (size <= 0) {
        mrp_log_error("Failed to encode message with default encoder.");
        exit(1);
    }

    mrp_log_info("encoded message size: %d", (int)size);

    decoded = mrp_msg_default_decode(encoded, size);
    if (decoded == NULL) {
        mrp_log_error("Failed to decode message with default decoder.");
        exit(1);
    }

    mrp_msg_dump(decoded, stdout);

    mrp_msg_unref(msg);
    mrp_msg_unref(decoded);
}


typedef struct {
    char     *str1;
    uint16_t  u16;
    int32_t   s32;
    char     *str2;
    double    dbl1;
    bool      bln1;
    double    dbl2;
    char     *str3;
    bool      bln2;
} data1_t;

typedef struct {
    char     *str;
    uint8_t   u8;
    bool      bln;
} data2_t;

typedef struct {
    char     *str;
    uint16_t  u16;
    int32_t   s32;
    double    dbl;
} data3_t;

#if 0
typedef struct {
    uint16_t offs;                       /* member offset within structure */
    uint16_t tag;                        /* tag for member */
    uint16_t type;                       /* type of this member */
} mrp_msg_member_t;

typedef struct {
    uint16_t          tag;               /* structure tag */
    size_t            size;              /* structure size */
    int               nfield;            /* number of members */
    mrp_msg_member_t *fields;            /* member descriptor */
} mrp_msg_descr_t;
#endif

#define DUMP_FIELD(memb, fmt) printf("    %s: "fmt"\n", #memb, d->memb)

int cmp_data1(data1_t *d1, data1_t *d2)
{
    return
        !strcmp(d1->str1, d2->str1) &&
        !strcmp(d1->str2, d2->str2) &&
        !strcmp(d1->str3, d2->str3) &&
        d1->u16  == d2->u16  &&
        d1->s32  == d2->s32  &&
        d1->dbl1 == d2->dbl1 &&
        d1->bln1 == d2->bln1 &&
        d1->dbl2 == d2->dbl2 &&
        d1->bln2 == d2->bln2;
}

int cmp_data2(data2_t *d1, data2_t *d2)
{
    return
        !strcmp(d1->str, d2->str) &&
        d1->u8  == d2->u8  &&
        d1->bln == d2->bln;
}

int cmp_data3(data3_t *d1, data3_t *d2)
{
    return
        !strcmp(d1->str, d2->str) &&
        d1->u16 == d2->u16 &&
        d1->s32 == d2->s32 &&
        d1->dbl == d2->dbl;
}

void dump_data1(char *prefix, data1_t *d)
{
    printf("%s{\n", prefix);
    DUMP_FIELD(str1, "%s");
    DUMP_FIELD(u16 , "%u");
    DUMP_FIELD(s32 , "%d");
    DUMP_FIELD(str2, "%s");
    DUMP_FIELD(dbl1, "%f");
    DUMP_FIELD(bln1, "%d");
    DUMP_FIELD(dbl2, "%f");
    DUMP_FIELD(str2, "%s");
    DUMP_FIELD(bln2, "%d");
    printf("}\n");

}

void dump_data2(char *prefix, data2_t *d)
{
    printf("%s{\n", prefix);
    DUMP_FIELD(str, "%s");
    DUMP_FIELD(u8 , "%u");
    DUMP_FIELD(bln, "%d");
    printf("}\n");
}

void dump_data3(char *prefix, data3_t *d)
{
    printf("%s{\n", prefix);
    DUMP_FIELD(str, "%s");
    DUMP_FIELD(u16, "%u");
    DUMP_FIELD(s32, "%d");
    DUMP_FIELD(dbl, "%f");
    printf("}\n");
}

#undef DUMP_FIELD

static size_t mrp_msg_encode(void **bufp, void *data,
                             mrp_data_member_t *fields, int nfield);

static void *mrp_msg_decode(void **bufp, size_t *sizep, size_t data_size,
                            mrp_data_member_t *fields, int nfield);

void test_custom_encode_decode(void)
{
#define DESCRIBE(_type, _memb, _tag, _ftype) {                            \
        .offs  = MRP_OFFSET(_type, _memb),                                \
        .tag   = _tag,                                                    \
        .type  = MRP_MSG_FIELD_##_ftype,                                  \
        .guard = FALSE,                                                   \
        { NULL },                                                         \
        .hook  = { NULL, NULL }                                           \
 }

    mrp_data_member_t data1_descr[] = {
        DESCRIBE(data1_t, str1, 0x1, STRING),
        DESCRIBE(data1_t,  u16, 0x2, UINT16),
        DESCRIBE(data1_t, str1, 0x1, STRING),
        DESCRIBE(data1_t, u16 , 0x2, UINT16),
        DESCRIBE(data1_t, s32 , 0x3, SINT32),
        DESCRIBE(data1_t, str2, 0x4, STRING),
        DESCRIBE(data1_t, dbl1, 0x5, DOUBLE),
        DESCRIBE(data1_t, bln1, 0x6, BOOL  ),
        DESCRIBE(data1_t, dbl2, 0x7, DOUBLE),
        DESCRIBE(data1_t, str3, 0x8, STRING),
        DESCRIBE(data1_t, bln2, 0x9, BOOL  ),
    };
    int data1_nfield = MRP_ARRAY_SIZE(data1_descr);

    mrp_data_member_t data2_descr[] = {
        DESCRIBE(data2_t, str, 0x1, STRING),
        DESCRIBE(data2_t, u8 , 0x2, UINT8 ),
        DESCRIBE(data2_t, bln, 0x3, BOOL  ),
    };
    int data2_nfield = MRP_ARRAY_SIZE(data2_descr);

    mrp_data_member_t data3_descr[] = {
        DESCRIBE(data3_t, str, 0x1, STRING),
        DESCRIBE(data3_t, u16, 0x2, UINT16),
        DESCRIBE(data3_t, s32, 0x3, SINT32),
        DESCRIBE(data3_t, dbl, 0x4, DOUBLE),
    };
    int data3_nfield = MRP_ARRAY_SIZE(data3_descr);

#define TAG_DATA1 0x1
#define TAG_DATA2 0x2
#define TAG_DATA3 0x3


    data1_t data1 = {
        .str1 = "data1, str1",
        .u16  = 32768U,
        .s32  = -12345678,
        .str2 = "data1, str2",
        .dbl1 = 9.81,
        .bln1 = TRUE,
        .dbl2 = -3.141,
        .str3 = "data1, str3",
        .bln2 = FALSE
    };
    data2_t data2 = {
        .str = "data2, str",
        .u8  = 128,
        .bln = TRUE
    };
    data3_t data3 = {
        .str = "data3, str",
        .u16 = 32768U,
        .s32 = -12345678,
        .dbl = 1.2345
    };

    data1_t *d1;
    data2_t *d2;
    data3_t *d3;
    void    *buf;
    size_t  size;

    size = mrp_msg_encode(&buf, &data1, data1_descr, data1_nfield);

    if (size <= 0) {
        mrp_log_error("failed to encode data1_t");
        exit(1);
    }

    d1 = mrp_msg_decode(&buf, &size, sizeof(data1_t), data1_descr,data1_nfield);

    if (d1 == NULL) {
        mrp_log_error("failed to decode encoded data1_t");
        exit(1);
    }

    dump_data1("original data1: ", &data1);
    dump_data1("decoded  data1: ", d1);
    if (!cmp_data1(&data1, d1)) {
        mrp_log_error("Original and decoded data1_t do not match!");
        exit(1);
    }
    else
        mrp_log_info("ok, original and decoded match...");


    size = mrp_msg_encode(&buf, &data2, data2_descr, data2_nfield);

    if (size <= 0) {
        mrp_log_error("failed to encode data2_t");
        exit(1);
    }

    d2 = mrp_msg_decode(&buf, &size, sizeof(data2_t), data2_descr,data2_nfield);

    if (d2 == NULL) {
        mrp_log_error("failed to decode encoded data2_t");
        exit(1);
    }

    dump_data2("original data2: ", &data2);
    dump_data2("decoded  data2: ", d2);
    if (!cmp_data2(&data2, d2)) {
        mrp_log_error("Original and decoded data2_t do not match!");
        exit(1);
    }
    else
        mrp_log_info("ok, original and decoded match...");


    size = mrp_msg_encode(&buf, &data3, data3_descr, data3_nfield);

    if (size <= 0) {
        mrp_log_error("failed to encode data3_t");
        exit(1);
    }

    d3 = mrp_msg_decode(&buf, &size, sizeof(data3_t), data3_descr,data3_nfield);

    if (d3 == NULL) {
        mrp_log_error("failed to decode encoded data3_t");
        exit(1);
    }

    dump_data3("original data3: ", &data3);
    dump_data3("decoded  data3: ", d3);
    if (!cmp_data3(&data3, d3)) {
        mrp_log_error("Original and decoded data3_t do not match!");
        exit(1);
    }
    else
        mrp_log_info("ok, original and decoded match...");
}


static void test_basic(void)
{
    mrp_msg_t *msg;
    char      *str1, *str2;
    uint16_t   u16;
    int16_t    s16;
    uint32_t   u32;
    int32_t    s32;
    double     dbl1, dbl2;
    int        i;

    struct field_t {
        uint16_t  tag;
        uint16_t  type;
        void     *ptr;
    } f[] = {
        { 0x1, MRP_MSG_FIELD_STRING, &str1 },
        { 0x2, MRP_MSG_FIELD_STRING, &str2 },
        { 0x3, MRP_MSG_FIELD_UINT16, &u16  },
        { 0x4, MRP_MSG_FIELD_SINT16, &s16  },
        { 0x5, MRP_MSG_FIELD_UINT32, &u32  },
        { 0x6, MRP_MSG_FIELD_SINT32, &s32  },
        { 0x7, MRP_MSG_FIELD_DOUBLE, &dbl1 },
        { 0x8, MRP_MSG_FIELD_DOUBLE, &dbl2 }
    };

    msg = mrp_msg_create(MRP_MSG_TAG_STRING(0x1, "string 0x1"),
                         MRP_MSG_TAG_STRING(0x2, "string 0x2"),
                         MRP_MSG_TAG_UINT16(0x3,  3),
                         MRP_MSG_TAG_SINT16(0x4, -4),
                         MRP_MSG_TAG_UINT32(0x5,  5),
                         MRP_MSG_TAG_SINT32(0x6, -6),
                         MRP_MSG_TAG_DOUBLE(0x7,  3.14),
                         MRP_MSG_TAG_DOUBLE(0x8, -9.81),
                         MRP_MSG_END);

    if (msg == NULL) {
        mrp_log_error("Failed to create message.");
        exit(1);
    }
    else
        mrp_log_info("Message created OK.");


    if (!mrp_msg_get(msg,
                     0x1, MRP_MSG_FIELD_STRING, &str1,
                     0x2, MRP_MSG_FIELD_STRING, &str2,
                     0x3, MRP_MSG_FIELD_UINT16, &u16,
                     0x4, MRP_MSG_FIELD_SINT16, &s16,
                     0x5, MRP_MSG_FIELD_UINT32, &u32,
                     0x6, MRP_MSG_FIELD_SINT32, &s32,
                     0x7, MRP_MSG_FIELD_DOUBLE, &dbl1,
                     0x8, MRP_MSG_FIELD_DOUBLE, &dbl2,
                     MRP_MSG_END)) {
        mrp_log_error("Failed to get message fields.");
        exit(1);
    }
    else {
        mrp_log_info("Got message fields:");
        mrp_log_info("  str1='%s', str2='%s'", str1, str2);
        mrp_log_info("  u16=%u, s16=%d", u16, s16);
        mrp_log_info("  u32=%u, s32=%d", u32, s32);
        mrp_log_info("  dbl1=%f, dbl2=%f", dbl1, dbl2);
    }

    if (!mrp_msg_get(msg,
                     0x8, MRP_MSG_FIELD_DOUBLE, &dbl2,
                     0x7, MRP_MSG_FIELD_DOUBLE, &dbl1,
                     0x6, MRP_MSG_FIELD_SINT32, &s32,
                     0x5, MRP_MSG_FIELD_UINT32, &u32,
                     0x4, MRP_MSG_FIELD_SINT16, &s16,
                     0x3, MRP_MSG_FIELD_UINT16, &u16,
                     0x2, MRP_MSG_FIELD_STRING, &str2,
                     0x1, MRP_MSG_FIELD_STRING, &str1,
                     MRP_MSG_END)) {
        mrp_log_error("Failed to get message fields.");
        exit(1);
    }
    else {
        mrp_log_info("Got message fields:");
        mrp_log_info("  str1='%s', str2='%s'", str1, str2);
        mrp_log_info("  u16=%u, s16=%d", u16, s16);
        mrp_log_info("  u32=%u, s32=%d", u32, s32);
        mrp_log_info("  dbl1=%f, dbl2=%f", dbl1, dbl2);
    }


#define TAG(idx) f[(idx) & 0x7].tag
#define TYPE(idx) f[(idx) & 0x7].type
#define PTR(idx) f[(idx) & 0x7].ptr
#define FIELD(idx) TAG((idx)), TYPE((idx)), PTR((idx))

    for (i = 0; i < (int)MRP_ARRAY_SIZE(f); i++) {
        if (!mrp_msg_get(msg,
                         FIELD(i+0), FIELD(i+1), FIELD(i+2), FIELD(i+3),
                         FIELD(i+4), FIELD(i+5), FIELD(i+6), FIELD(i+7),
                         MRP_MSG_END)) {
            mrp_log_error("Failed to get message fields for offset %d.", i);
            exit(1);
        }
        else {
            mrp_log_info("Got message fields for offset %d:", i);
            mrp_log_info("  str1='%s', str2='%s'", str1, str2);
            mrp_log_info("  u16=%u, s16=%d", u16, s16);
            mrp_log_info("  u32=%u, s32=%d", u32, s32);
            mrp_log_info("  dbl1=%f, dbl2=%f", dbl1, dbl2);
        }
    }

    if (mrp_msg_get(msg,
                    0x9, MRP_MSG_FIELD_STRING, &str1, MRP_MSG_END)) {
        mrp_log_error("Hmm... non-existent field found.");
        exit(1);
    }
    else
        mrp_log_info("Ok, non-existent field not found...");
}


int main(int argc, char *argv[])
{
    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_DEBUG));
    mrp_log_set_target(MRP_LOG_TO_STDOUT);

    test_basic();

    test_default_encode_decode(argc, argv);
    test_custom_encode_decode();

    return 0;
}


static size_t mrp_msg_encode(void **bufp, void *data,
                             mrp_data_member_t *fields, int nfield)
{
    mrp_data_member_t *f;
    mrp_msgbuf_t       mb;
    mrp_msg_value_t   *v;
    uint32_t           len;
    int                i;
    size_t             size;

    size = nfield * (2 * sizeof(uint16_t) + sizeof(uint64_t));

    if (mrp_msgbuf_write(&mb, size)) {
        for (i = 0, f = fields; i < nfield; i++, f++) {
            MRP_MSGBUF_PUSH(&mb, htobe16(f->tag) , 1, nomem);

            v = (mrp_msg_value_t *)(data + f->offs);

            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
                len = strlen(v->str) + 1;
                MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                MRP_MSGBUF_PUSH_DATA(&mb, v->str, len, 1, nomem);
                break;

            case MRP_MSG_FIELD_BOOL:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->bln ? TRUE : FALSE), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT8:
                MRP_MSGBUF_PUSH(&mb, v->u8, 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT8:
                MRP_MSGBUF_PUSH(&mb, v->s8, 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(v->u16), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(v->s16), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->u32), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->s32), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(v->u64), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(v->s64), 1, nomem);
                break;

            case MRP_MSG_FIELD_DOUBLE:
                MRP_MSGBUF_PUSH(&mb, v->dbl, 1, nomem);
                break;

            case MRP_MSG_FIELD_BLOB:
                errno = EOPNOTSUPP;
                /* intentional fall through */

            default:
                if (f->type & MRP_MSG_FIELD_ARRAY) {
                    errno = EOPNOTSUPP;
                    mrp_log_error("XXX TODO: MRP_MSG_FIELD_ARRAY "
                                  "not implemented");
                }
                else
                    errno = EINVAL;

                mrp_msgbuf_cancel(&mb);
            nomem:
                *bufp = NULL;
                return 0;
            }
        }
    }

    *bufp = mb.buf;
    return (size_t)(mb.p - mb.buf);
}


#if 0
static mrp_data_member_t *member_type(mrp_data_member_t *fields, int nfield,
                                      uint16_t tag)
{
    mrp_data_member_t *f;
    int                i;

    for (i = 0, f = fields; i < nfield; i++, f++)
        if (f->tag == tag)
            return f;

    return NULL;
}
#endif

static void *mrp_msg_decode(void **bufp, size_t *sizep, size_t data_size,
                            mrp_data_member_t *fields, int nfield)
{
    void              *data;
    mrp_data_member_t *f;
    mrp_msgbuf_t       mb;
    uint16_t           tag;
    mrp_msg_value_t   *v;
    void              *value;
    uint32_t           len;
    int                i;

    if (MRP_UNLIKELY((data = mrp_allocz(data_size)) == NULL))
        return NULL;

    mrp_msgbuf_read(&mb, *bufp, *sizep);

    for (i = 0; i < nfield; i++) {
        tag = be16toh(MRP_MSGBUF_PULL(&mb, typeof(tag) , 1, nodata));
        f   = member_type(fields, nfield, tag);

        if (MRP_UNLIKELY(f == NULL))
            goto unknown_field;

        v = (mrp_msg_value_t *)(data + f->offs);

        switch (f->type) {
        case MRP_MSG_FIELD_STRING:
            len    = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
            value  = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
            v->str = mrp_strdup((char *)value);
            if (v->str == NULL)
                goto nomem;
            break;

        case MRP_MSG_FIELD_BOOL:
            v->bln = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t, 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT8:
            v->u8 = MRP_MSGBUF_PULL(&mb, typeof(v->u8), 1, nodata);
            break;

        case MRP_MSG_FIELD_SINT8:
            v->s8 = MRP_MSGBUF_PULL(&mb, typeof(v->s8), 1, nodata);
            break;

        case MRP_MSG_FIELD_UINT16:
            v->u16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v->u16), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT16:
            v->s16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v->s16), 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT32:
            v->u32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v->u32), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT32:
            v->s32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v->s32), 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT64:
            v->u64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v->u64), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT64:
            v->s64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v->s64), 1, nodata));
            break;

        case MRP_MSG_FIELD_DOUBLE:
            v->dbl = MRP_MSGBUF_PULL(&mb, typeof(v->dbl), 1, nodata);
            break;

        case MRP_MSG_FIELD_BLOB:
            errno = EOPNOTSUPP;
        default:
            if (f->type & MRP_MSG_FIELD_ARRAY) {
                errno = EOPNOTSUPP;
                mrp_log_error("XXX TODO: MRP_MSG_FIELD_ARRAY "
                              "not implemented");
            }
            else {
            unknown_field:
                errno = EINVAL;
            }
            goto fail;
        }
    }

    *bufp   = mb.buf;
    *sizep -= mb.p - mb.buf;
    return data;

 nodata:
 nomem:
 fail:
    if (data != NULL) {
        for (i = 0, f = fields; i < nfield; i++, f++) {
            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
            case MRP_MSG_FIELD_BLOB:
                mrp_free(data + f->offs);
            }
        }

        mrp_free(data);
    }

    return NULL;
}
