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

#ifndef __MURPHY_MSG_H__
#define __MURPHY_MSG_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#include <murphy/common/list.h>
#include <murphy/common/refcnt.h>

MRP_CDECL_BEGIN

/*
 * message field types
 */

#define A(t) MRP_MSG_FIELD_##t
typedef enum {
    MRP_MSG_FIELD_INVALID  = 0x00,       /* defined invalid type */
    MRP_MSG_FIELD_STRING   = 0x01,       /* mqi_varchar */
    MRP_MSG_FIELD_INTEGER  = 0x02,       /* mqi_integer */
    MRP_MSG_FIELD_UNSIGNED = 0x03,       /* mqi_unsignd */
    MRP_MSG_FIELD_DOUBLE   = 0x04,       /* mqi_floating */
    MRP_MSG_FIELD_BOOL     = 0x05,       /* boolean */
    MRP_MSG_FIELD_UINT8    = 0x06,       /* unsigned 8-bit integer */
    MRP_MSG_FIELD_SINT8    = 0x07,       /* signed 8-bit integer */
    MRP_MSG_FIELD_INT8     = A(SINT8),   /* alias for SINT8 */
    MRP_MSG_FIELD_UINT16   = 0x08,       /* unsigned 16-bit integer */
    MRP_MSG_FIELD_SINT16   = 0x09,       /* signed 16-bit integer */
    MRP_MSG_FIELD_INT16    = A(SINT16),  /* alias for SINT16 */
    MRP_MSG_FIELD_UINT32   = 0x0a,       /* unsigned 32-bit integer */
    MRP_MSG_FIELD_SINT32   = 0x0b,       /* signed 32-bit integer */
    MRP_MSG_FIELD_INT32    = A(SINT32),  /* alias for SINT32 */
    MRP_MSG_FIELD_UINT64   = 0x0c,       /* unsigned 64-bit integer */
    MRP_MSG_FIELD_SINT64   = 0x0d,       /* signed 64-bit integer */
    MRP_MSG_FIELD_INT64    = A(SINT64),  /* alias for SINT64 */
    MRP_MSG_FIELD_BLOB     = 0x0e,       /* a blob (not allowed in arrays) */
    MRP_MSG_FIELD_MAX      = 0x0e,
    MRP_MSG_FIELD_ANY      = 0x0f,       /* any type of field when querying */

    MRP_MSG_FIELD_ARRAY    = 0x80,       /* bit-mask to mark arrays */
} mrp_msg_field_type_t;
#undef A

#define MRP_MSG_END ((char *)MRP_MSG_FIELD_INVALID) /* NULL */

#define MRP_MSG_FIELD_ARRAY_OF(t)   (MRP_MSG_FIELD_ARRAY | MRP_MSG_FIELD_##t)
#define MRP_MSG_FIELD_IS_ARRAY(t)   ((t) & MRP_MSG_FIELD_ARRAY)
#define MRP_MSG_FIELD_ARRAY_TYPE(t) ((t) & ~MRP_MSG_FIELD_ARRAY)

#define MRP_MSG_TAG_STRING(tag, arg) (tag), MRP_MSG_FIELD_STRING, (arg)
#define MRP_MSG_TAG_BOOL(tag, arg)   (tag), MRP_MSG_FIELD_BOOL  , (arg)
#define MRP_MSG_TAG_UINT8(tag, arg)  (tag), MRP_MSG_FIELD_UINT8 , (arg)
#define MRP_MSG_TAG_SINT8(tag, arg)  (tag), MRP_MSG_FIELD_SINT8 , (arg)
#define MRP_MSG_TAG_UINT16(tag, arg) (tag), MRP_MSG_FIELD_UINT16, (arg)
#define MRP_MSG_TAG_SINT16(tag, arg) (tag), MRP_MSG_FIELD_SINT16, (arg)
#define MRP_MSG_TAG_UINT32(tag, arg) (tag), MRP_MSG_FIELD_UINT32, (arg)
#define MRP_MSG_TAG_SINT32(tag, arg) (tag), MRP_MSG_FIELD_SINT32, (arg)
#define MRP_MSG_TAG_UINT64(tag, arg) (tag), MRP_MSG_FIELD_UINT64, (arg)
#define MRP_MSG_TAG_SINT64(tag, arg) (tag), MRP_MSG_FIELD_SINT64, (arg)
#define MRP_MSG_TAG_DOUBLE(tag, arg) (tag), MRP_MSG_FIELD_DOUBLE, (arg)
#define MRP_MSG_TAG_BLOB(tag, arg)   (tag), MRP_MSG_FIELD_BLOB  , (arg)

#define MRP_MSG_TAGGED(tag, type, ...) (tag), (type), __VA_ARGS__
#define MRP_MSG_TAG_ARRAY(tag, type, cnt, arr)                        \
    (tag), MRP_MSG_FIELD_ARRAY | MRP_MSG_FIELD_##type, (cnt), (arr)
#define MRP_MSG_TAG_STRING_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), STRING, (cnt), (arr))
#define MRP_MSG_TAG_BOOL_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), BOOL, (cnt), (arr))
#define MRP_MSG_TAG_UINT8_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), UINT8, (cnt), (arr))
#define MRP_MSG_TAG_SINT8_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), SINT8, (cnt), (arr))
#define MRP_MSG_TAG_UINT16_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), UINT16, (cnt), (arr))
#define MRP_MSG_TAG_SINT16_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), SINT16, (cnt), (arr))
#define MRP_MSG_TAG_UINT32_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), UINT32, (cnt), (arr))
#define MRP_MSG_TAG_SINT32_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), SINT32, (cnt), (arr))
#define MRP_MSG_TAG_UINT64_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), UINT64, (cnt), (arr))
#define MRP_MSG_TAG_SINT64_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), SINT64, (cnt), (arr))
#define MRP_MSG_TAG_DOUBLE_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), DOUBLE, (cnt), (arr))
#define MRP_MSG_TAG_BLOB_ARRAY(tag, cnt, arr) \
    MRP_MSG_TAG_ARRAY((tag), BLOB, (cnt), (arr))

#define MRP_MSG_TAG_ANY(tag, typep, valuep) \
    (tag), MRP_MSG_FIELD_ANY, (typep), (valuep)


/** Sentinel to pass in as the last argument to mrp_msg_create. */
#define MRP_MSG_FIELD_END NULL


/*
 * generic messages
 *
 * A generic message is just a collection of message fields. By default
 * transports are in generic messaging mode in which case they take messages
 * as input (for transmission) and provide messages as events (for receiption).
 * A generic message field consists of a field tag, a field type, the actual
 * type-specific field value, and for certain types a size.
 *
 * The field tag is used by the communicating parties to attach semantic
 * meaning to the field data. One can think of it as the 'name' of the field
 * within a message. It is not interpreted by the messaging layer in any way.
 * The field type defines what kind of data the field contains contains and
 * it must be one of the predefined MRP_MSG_FIELD_* types. The actual field
 * data then depends on the type. size is only used for those data types that
 * require a size (blobs and arrays).
 */

#define MRP_MSG_VALUE_UNION union {                                       \
        char      *str;                                                   \
        bool       bln;                                                   \
        uint8_t    u8;                                                    \
        int8_t     s8;                                                    \
        uint16_t   u16;                                                   \
        int16_t    s16;                                                   \
        uint32_t   u32;                                                   \
        int32_t    s32;                                                   \
        uint64_t   u64;                                                   \
        int64_t    s64;                                                   \
        double     dbl;                                                   \
        void      *blb;                                                   \
        void      *aany;                                                  \
        char     **astr;                                                  \
        bool      *abln;                                                  \
        uint8_t   *au8;                                                   \
        int8_t    *as8;                                                   \
        uint16_t  *au16;                                                  \
        int16_t   *as16;                                                  \
        uint32_t  *au32;                                                  \
        int32_t   *as32;                                                  \
        uint64_t  *au64;                                                  \
        int64_t   *as64;                                                  \
        double    *adbl;                                                  \
    }

typedef MRP_MSG_VALUE_UNION mrp_msg_value_t;

typedef struct {
    mrp_list_hook_t hook;                /* hook to list of fields */
    uint16_t        tag;                 /* message field tag */
    uint16_t        type;                /* message field type */
    MRP_MSG_VALUE_UNION;                 /* message field value */
    uint32_t        size[0];             /* size, if an array or a blob */
} mrp_msg_field_t;


typedef struct {
    mrp_list_hook_t fields;              /* list of message fields */
    size_t          nfield;              /* number of fields */
    mrp_refcnt_t    refcnt;              /* reference count */
} mrp_msg_t;


/** Create a new message. */
mrp_msg_t *mrp_msg_create(uint16_t tag, ...) MRP_NULLTERM;

/** Create a new message. */
mrp_msg_t *mrp_msg_createv(uint16_t tag, va_list ap);

/** Macro to create an empty message. */
#define mrp_msg_create_empty() mrp_msg_create(MRP_MSG_FIELD_INVALID, NULL)

/** Increase refcount of the given message. */
mrp_msg_t *mrp_msg_ref(mrp_msg_t *msg);

/** Decrease the refcount, free the message if refcount drops to zero. */
void mrp_msg_unref(mrp_msg_t *msg);

/** Append a field to a message. */
int mrp_msg_append(mrp_msg_t *msg, uint16_t tag, ...);

/** Prepend a field to a message. */
int mrp_msg_prepend(mrp_msg_t *msg, uint16_t tag, ...);

/** Set a field in a message to the given value. */
int mrp_msg_set(mrp_msg_t *msg, uint16_t tag, ...);

/** Iterate through the fields of a message. You must not any of the
    fields while iterating. */
int mrp_msg_iterate(mrp_msg_t *msg, void **it, uint16_t *tagp,
                    uint16_t *typep, mrp_msg_value_t *valp, size_t *sizep);

/** Iterate through the matching fields of a message. You should not delete
 * any of the fields while iterating through the message. */
int mrp_msg_iterate_matching(mrp_msg_t *msg, void **it, uint16_t *tagp,
                             uint16_t *typep, mrp_msg_value_t *valp,
                             size_t *sizep);

/** Find a field in a message. */
mrp_msg_field_t *mrp_msg_find(mrp_msg_t *msg, uint16_t tag);

/** Get the given fields (with matching tags and types) from the message. */
int mrp_msg_get(mrp_msg_t *msg, ...) MRP_NULLTERM;

/** Iterate through the message getting the given fields. */
int mrp_msg_iterate_get(mrp_msg_t *msg, void **it, ...);

/** Dump a message. */
int mrp_msg_dump(mrp_msg_t *msg, FILE *fp);

/** Encode the given message using the default message encoder. */
ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp);

/** Decode the given message using the default message decoder. */
mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size);


/*
 * custom data types
 *
 * In addition to generic messages, you can instruct the messaging and
 * transport layers to encode/decode messages directly from/to custom data
 * structures. To do so you need to describe your data structures and register
 * them using data descriptors. A descriptor basically consists of a type
 * tag, structure size, number of members and and array of structure member
 * descriptors.
 *
 * The data type tag is used to identify the descriptor and consequently
 * the custom data type both during sending and receiving (ie. encoding and
 * decoding). It is assigned by the registering entity, it must be unique,
 * and it cannot be MRP_MSG_TAG_DEFAULT (0x0), or else registration will
 * fail. The size is used to allocate necessary memory for the data on the
 * receiving end. The member descriptors are used to describe the offset
 * and types of the members within the custom data type.
 */

#define MRP_MSG_TAG_DEFAULT 0x0          /* tag for default encode/decoder */

typedef struct {
    uint16_t        offs;                /* offset within structure */
    uint16_t        tag;                 /* tag for this member */
    uint16_t        type;                /* type of this member */
    bool            guard;               /* whether sentinel-terminated */
    MRP_MSG_VALUE_UNION;                 /* sentinel or offset of count field */
    mrp_list_hook_t hook;                /* hook to list of extra allocations */
} mrp_data_member_t;


typedef struct {
    mrp_refcnt_t       refcnt;           /* reference count */
    uint16_t           tag;              /* structure tag */
    size_t             size;             /* size of this structure */
    int                nfield;           /* number of members */
    mrp_data_member_t *fields;           /* member descriptors */
    mrp_list_hook_t    allocated;        /* fields needing extra allocation */
} mrp_data_descr_t;


/** Convenience macro to declare a custom data type (and its members). */
#define MRP_DATA_DESCRIPTOR(_var, _tag, _type, ...)                       \
    static mrp_data_member_t _var##_members[] = {                         \
        __VA_ARGS__                                                       \
    };                                                                    \
                                                                          \
    static mrp_data_descr_t _var = {                                      \
        .size   = sizeof(_type),                                          \
        .tag    = _tag,                                                   \
        .fields = _var##_members,                                         \
        .nfield = MRP_ARRAY_SIZE(_var##_members)                          \
 }

/** Convenience macro to declare a data member. */
#define MRP_DATA_MEMBER(_data_type, _member, _member_type) {              \
        .offs  = MRP_OFFSET(_data_type, _member),                         \
        .type  = _member_type,                                            \
        .guard = FALSE                                                    \
 }

/** Convenience macro to declare an array data member with a count field. */
#define MRP_DATA_ARRAY_COUNT(_data_type, _array, _count, _base_type) {    \
        .offs  = MRP_OFFSET(_data_type, _array),                          \
        .type  = MRP_MSG_FIELD_ARRAY | _base_type,                        \
        .guard = FALSE,                                                   \
      { .u32   = MRP_OFFSET(_data_type, _count) }                         \
    }

/** Convenience macro to declare an array data member with a sentinel value. */
#define MRP_DATA_ARRAY_GUARD(_data_type, _array, _guard_member, _guard_val, \
                             _base_type) {                                  \
        .offs          = MRP_OFFSET(_data_type, _array),                    \
        .type          = MRP_MSG_FIELD_ARRAY | _base_type,                  \
        .guard         = TRUE,                                              \
      { ._guard_member = _guard_val }                                       \
    }

/** Convenience macro to declare a blob data member with a count field. */
#define MRP_DATA_BLOB_MEMBER(_data_type, _blob, _count) {                 \
        .offs  = MRP_OFFSET(_data_type, _blob),                           \
        .type  = MRP_MSG_FIELD_BLOB,                                      \
        .guard = FALSE,                                                   \
        .u32   = MRP_OFFSET(_data_type, _count)                           \
    }


/** Encode a structure using the given message descriptor. */
size_t mrp_data_encode(void **bufp, void *data, mrp_data_descr_t *descr,
                       size_t reserve);

/** Decode a structure using the given message descriptor. */
void *mrp_data_decode(void **bufp, size_t *sizep, mrp_data_descr_t *descr);

/** Dump the given data buffer. */
int mrp_data_dump(void *data, mrp_data_descr_t *descr, FILE *fp);

/** Get the size of a data array member. */
int mrp_data_get_array_size(void *data, mrp_data_descr_t *type, int idx);

/** Get the size of a data blob member. */
int mrp_data_get_blob_size(void *data, mrp_data_descr_t *type, int idx);

/** Register a new custom data type with the messaging/transport layer. */
int mrp_msg_register_type(mrp_data_descr_t *type);

/** Look up the data type descriptor corresponding to the given tag. */
mrp_data_descr_t *mrp_msg_find_type(uint16_t tag);

/** Free the given custom data allocated by the messaging layer. */
int mrp_data_free(void *data, uint16_t tag);

/*
 * message encoding/decoding buffer
 *
 * This message buffer and the associated functions and macros can be
 * used to write message encoding/decoding functions for bitpipe-type
 * transports, ie. for transports where the underlying IPC just provides
 * a raw data connection between the communication endpoints and does not
 * impose/expect any structure on/from the data being transmitted.
 *
 * Practically all the basic stream and datagram socket transports are
 * such. They use the default encoding/decoding functions provided by
 * the messaging layer together with a very simple transport frame scheme,
 * where each frame consists of the amount a size indicating the size of
 * the encoded message in the bitpipe and the actual encoded message data.
 *
 * Note that at the moment this framing scheme is rather implicit in the
 * sense that you won't find a data type representing a frame. Rather the
 * framing is simply done in the sending/receiving code of the individual
 * transports.
 */

typedef struct {
    void   *buf;                         /* buffer to encode to/decode from */
    size_t  size;                        /* size of the buffer */
    void   *p;                           /* encoding/decoding pointer */
    size_t  l;                           /* space left in the buffer */
} mrp_msgbuf_t;



/** Initialize the given message buffer for writing. */
void *mrp_msgbuf_write(mrp_msgbuf_t *mb, size_t size);

/** Initialize the given message buffer for reading. */
void mrp_msgbuf_read(mrp_msgbuf_t *mb, void *buf, size_t size);

/** Deinitialize the given message buffer, usually due to some error. */
void mrp_msgbuf_cancel(mrp_msgbuf_t *mb);

/** Reallocate the buffer if needed to accomodate size bytes of data. */
void *mrp_msgbuf_ensure(mrp_msgbuf_t *mb, size_t size);

/** Reserve the given amount of space from the buffer. */
void *mrp_msgbuf_reserve(mrp_msgbuf_t *mb, size_t size, size_t align);

/** Pull the given amount of data from the buffer. */
void *mrp_msgbuf_pull(mrp_msgbuf_t *mb, size_t size, size_t align);

/** Push data with alignment to the buffer, jumping to errlbl on errors. */
#define MRP_MSGBUF_PUSH(mb, data, align, errlbl) do {                     \
        size_t        _size = sizeof(data);                               \
        typeof(data) *_ptr;                                               \
                                                                          \
        _ptr  = mrp_msgbuf_reserve((mb), _size, (align));                 \
                                                                          \
        if (_ptr != NULL)                                                 \
            *_ptr = data;                                                 \
        else                                                              \
            goto errlbl;                                                  \
    } while (0)

/** Push aligned data to the buffer, jumping to errlbl on errors. */
#define MRP_MSGBUF_PUSH_DATA(mb, data, size, align, errlbl) do {          \
        size_t _size = (size);                                            \
        void   *_ptr;                                                     \
                                                                          \
        _ptr  = mrp_msgbuf_reserve((mb), _size, (align));                 \
                                                                          \
        if (_ptr != NULL)                                                 \
            memcpy(_ptr, data, _size);                                    \
        else                                                              \
            goto errlbl;                                                  \
    } while (0)

/** Pull aligned data of type from the buffer, jump to errlbl on errors. */
#define MRP_MSGBUF_PULL(mb, type, align, errlbl) ({                       \
            size_t  _size = sizeof(type);                                 \
            type   *_ptr;                                                 \
                                                                          \
            _ptr = mrp_msgbuf_pull((mb), _size, (align));                 \
                                                                          \
            if (_ptr == NULL)                                             \
                goto errlbl;                                              \
                                                                          \
            *_ptr;                                                        \
        })

/** Pull aligned data of type from the buffer, jump to errlbl on errors. */
#define MRP_MSGBUF_PULL_DATA(mb, size, align, errlbl) ({                  \
            size_t  _size = size;                                         \
            void   *_ptr;                                                 \
                                                                          \
            _ptr = mrp_msgbuf_pull((mb), _size, (align));                 \
                                                                          \
            if (_ptr == NULL)                                             \
                goto errlbl;                                              \
                                                                          \
            _ptr;                                                         \
        })

MRP_CDECL_END

#endif /* __MURPHY_MSG_H__ */
