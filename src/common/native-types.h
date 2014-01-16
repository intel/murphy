/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#ifndef __MURPHY_COMMON_NATIVE_TYPES_H__
#define __MURPHY_COMMON_NATIVE_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <murphy/common/macros.h>
#include <murphy/common/list.h>

MRP_CDECL_BEGIN

#define MRP_INVALID_TYPE ((uint32_t)-1)


/**
 * pre-defined native type ids
 */

typedef enum {
    MRP_TYPE_UNKNOWN = 0,
    MRP_TYPE_INT8,
    MRP_TYPE_UINT8,
    MRP_TYPE_INT16,
    MRP_TYPE_UINT16,
    MRP_TYPE_INT32,
    MRP_TYPE_UINT32,
    MRP_TYPE_INT64,
    MRP_TYPE_UINT64,
    MRP_TYPE_FLOAT,
    MRP_TYPE_DOUBLE,
    MRP_TYPE_BOOL,
    MRP_TYPE_INT,
    MRP_TYPE_UINT,
    MRP_TYPE_SHORT,
    MRP_TYPE_USHORT,
    MRP_TYPE_SIZET,
    MRP_TYPE_SSIZET,
    MRP_TYPE_STRING,
    MRP_TYPE_BLOB,
    MRP_TYPE_ARRAY,
    MRP_TYPE_STRUCT,
    MRP_TYPE_MAX
} mrp_type_t;


/**
 * data type values
 */

typedef union {
    int8_t    s8;
    int8_t   *s8p;
    uint8_t   u8;
    uint8_t  *u8p;
    int16_t   s16;
    int16_t  *s16p;
    uint16_t  u16;
    uint16_t *u16p;
    int32_t   s32;
    int32_t  *s32p;
    uint32_t  u32;
    uint32_t *u32p;
    int64_t   s64;
    int64_t  *s64p;
    uint64_t  u64;
    uint64_t *u64p;
    float     flt;
    float    *fltp;
    double    dbl;
    double   *dblp;
    bool      bln;
    bool     *blnp;
    void     *blb;
    char      str[0];
    char     *strp;
    int             i;
    int            *ip;
    unsigned int    ui;
    unsigned int   *uip;
    short           si;
    short          *sip;
    unsigned short  usi;
    unsigned short *usip;
    size_t          sz;
    size_t         *szp;
    ssize_t         ssz;
    ssize_t        *sszp;
    void     *ptr;
    void    **ptrp;
} mrp_value_t;


/**
 * type id map (for transport-specific mapping of type ids)
 */

typedef struct {
    uint32_t type_id;                    /* native type id */
    uint32_t mapped;                     /* mapped type id */
} mrp_typemap_t;


/** Macro to initialize a typemap entry. */
#define MRP_TYPEMAP(_mapped_id, _type_id)               \
    { .type_id = _type_id, .mapped = _mapped_id }

/** Macro to set a typemap termination entry. */
#define MRP_TYPEMAP_END                                 \
    { MRP_INVALID_TYPE, MRP_INVALID_TYPE }

/**
 * type and member descriptors
 */

typedef enum {
    MRP_LAYOUT_DEFAULT = 0,              /* default, type-specific layout */
    MRP_LAYOUT_INLINED,                  /* inlined/embedded layout */
    MRP_LAYOUT_INDIRECT,                 /* indirect layout */
} mrp_layout_t;

#define MRP_NATIVE_COMMON_FIELDS         /* fields common to all members */ \
    char              *name;             /* name of this member */          \
    uint32_t           type;             /* type id of this member */       \
    size_t             offs;             /* offset from base pointer */     \
    mrp_layout_t       layout            /* member layout */

typedef struct {
    MRP_NATIVE_COMMON_FIELDS;            /* common fields to all members */
} mrp_native_any_t;

typedef struct {                         /* a blob member */
    MRP_NATIVE_COMMON_FIELDS;            /* common member fields */
    union {                              /* size-indicating member */
        char     *name;                  /*     name */
        uint32_t  idx;                   /*     or index */
    } size;
} mrp_native_blob_t;

typedef enum {
    MRP_ARRAY_SIZE_EXPLICIT,             /* explicitly sized array */
    MRP_ARRAY_SIZE_GUARDED,              /* sentinel-guarded array */
    MRP_ARRAY_SIZE_FIXED,                /* a fixed size array */
} mrp_array_size_t;

typedef struct {
    MRP_NATIVE_COMMON_FIELDS;            /* common member fields */
    size_t size;                         /* inlined buffer size */
} mrp_native_string_t;

typedef struct {                         /* an array member */
    MRP_NATIVE_COMMON_FIELDS;            /* common member fields */
    mrp_array_size_t kind;               /* which kind of array */
    union {                              /* contained element type */
        char     *name;                  /*     name */
        uint32_t  id;                    /*     or type id */
    } elem;
    union {                              /* size or guard member */
        char     *name;                  /*     name */
        uint32_t  idx;                   /*     or index */
        size_t    nelem;                 /*     or number of elements */
    } size;
    mrp_value_t sentinel;                /* sentinel value, if guarded */
} mrp_native_array_t;

typedef struct {                         /* member of type struct */
    MRP_NATIVE_COMMON_FIELDS;            /* common member fields */
    union {                              /* struct type */
        char     *name;                  /*     name */
        uint32_t  id;                    /*     or type id */
    } data_type;
} mrp_native_struct_t;

typedef union {
    mrp_native_any_t    any;
    mrp_native_string_t str;
    mrp_native_blob_t   blob;
    mrp_native_array_t  array;
    mrp_native_struct_t strct;
} mrp_native_member_t;

typedef struct {
    char                *name;           /* name of this type */
    uint32_t             id;             /* assigned id for this type */
    size_t               size;           /* size of this type */
    mrp_native_member_t *members;        /* members of this type if any */
    size_t               nmember;        /* number of members */
    mrp_list_hook_t      hook;           /* to list of registered types */
} mrp_native_type_t;


/** Helper macro to initialize native member fields. */
#define __MRP_MEMBER_INIT(_objtype, _member, _type)                     \
        .name   = #_member,                                             \
        .type   = _type,                                                \
        .offs   = MRP_OFFSET(_objtype, _member)

/** Helper macro to declare a native member with a given type an layout. */
#define __MRP_MEMBER(_objtype, _type, _member, _layout)                 \
    {                                                                   \
        .any = {                                                        \
            __MRP_MEMBER_INIT(_objtype, _member, _type),                \
            .layout = MRP_LAYOUT_##_layout,                             \
        }                                                               \
    }

/** Declare an indirect string member of the native type. */
#define MRP_INDIRECT_STRING(_objtype, _member, _size)                   \
    __MRP_MEMBER(_objtype, _member, MRP_TYPE_STRING, INDIRECT)

/** Declare an inlined string member of the native type. */
#define MRP_INLINED_STRING(_objtype, _member, _size)                    \
    {                                                                   \
        .str = {                                                        \
            __MRP_MEMBER_INIT(_objtype, _member, MRP_TYPE_STRING),      \
            .layout = MRP_LAYOUT_INLINED,                               \
            .size   = _size,                                            \
        }                                                               \
    }

/** By default declare a string members indirect. */
#define MRP_DEFAULT_STRING(_objtype, _member, _size)                    \
    __MRP_MEMBER(_objtype, MRP_TYPE_STRING, _member, INDIRECT)

/** Declare an explicitly sized array member of the native typet. */
#define MRP_SIZED_ARRAY(_objtype, _member, _layout, _type, _size)       \
    {                                                                   \
        .array = {                                                      \
            __MRP_MEMBER_INIT(_objtype, _member, MRP_TYPE_ARRAY),       \
            .layout  = MRP_LAYOUT_##_layout,                            \
            .kind    = MRP_ARRAY_SIZE_EXPLICIT,                         \
            .elem    = { .name = #_type, },                             \
            .size    = { .name = #_size, },                             \
        }                                                               \
    }

/** Declare a sentinel-guarded array member of the native type. */
#define MRP_GUARDED_ARRAY(_objtype, _member, _layout, _type, _guard,    \
                          ...)                                          \
    {                                                                   \
        .array = {                                                      \
            __MRP_MEMBER_INIT(_objtype, _member, MRP_TYPE_ARRAY),       \
            .layout   = MRP_LAYOUT_##_layout,                           \
            .kind     = MRP_ARRAY_SIZE_GUARDED,                         \
            .elem     = { .name = #_type, },                            \
            .size     = { .name = #_guard, },                           \
            .sentinel = { __VA_ARGS__ },                                \
        }                                                               \
    }

/** Declare a fixed array member of the native type. */
#define MRP_FIXED_ARRAY(_objtype, _member, _layout, _type)              \
    {                                                                   \
        .array = {                                                      \
            __MRP_MEMBER_INIT(_objtype, _member, MRP_TYPE_ARRAY),       \
            .layout   = MRP_LAYOUT_##_layout,                           \
            .kind     = MRP_ARRAY_SIZE_FIXED,                           \
            .elem     = { .name = #_type, },                            \
            .size     = {                                               \
                .nelem = MRP_ARRAY_SIZE(((_objtype *)0x0)->_member)     \
            },                                                          \
        }                                                               \
    }

/** Declare a struct member of the native type. */
#define MRP_STRUCT(_objtype, _member, _layout, _type)                   \
    {                                                                   \
        .strct = {                                                      \
            __MRP_MEMBER_INIT(_objtype, _member, MRP_TYPE_STRUCT),      \
            .data_type = { .name = #_type },                            \
        }                                                               \
    }

/** Macros for declaring basic members of the native type. */
#define MRP_INT8(_ot, _m, _l)   __MRP_MEMBER(_ot, MRP_TYPE_INT8  , _m, _l)
#define MRP_UINT8(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_UINT8 , _m, _l)
#define MRP_INT16(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_INT16 , _m, _l)
#define MRP_UINT16(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_UINT16, _m, _l)
#define MRP_INT32(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_INT32 , _m, _l)
#define MRP_UINT32(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_UINT32, _m, _l)
#define MRP_INT64(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_INT64 , _m, _l)
#define MRP_UINT64(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_UINT64, _m, _l)
#define MRP_FLOAT(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_FLOAT , _m, _l)
#define MRP_DOUBLE(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_DOUBLE, _m, _l)
#define MRP_BOOL(_ot, _m, _l)   __MRP_MEMBER(_ot, MRP_TYPE_BOOL  , _m, _l)

#define MRP_INT(_ot, _m, _l)    __MRP_MEMBER(_ot, MRP_TYPE_INT   , _m, _l)
#define MRP_UINT(_ot, _m, _l)   __MRP_MEMBER(_ot, MRP_TYPE_UINT  , _m, _l)
#define MRP_SHORT(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_SHORT , _m, _l)
#define MRP_USHORT(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_USHORT, _m, _l)
#define MRP_SIZET(_ot, _m, _l)  __MRP_MEMBER(_ot, MRP_TYPE_SIZET , _m, _l)
#define MRP_SSIZET(_ot, _m, _l) __MRP_MEMBER(_ot, MRP_TYPE_SSIZET, _m, _l)

/** Macro for declaring string members of the native type. */
#define MRP_STRING(_objtype, _member, _layout)                  \
    MRP_##_layout##_STRING(_objtype, _member,                   \
                           sizeof(((_objtype *)0x0)->_member))

/** Macro for declaring array members of the native type. */
#define MRP_ARRAY(_objtype, _member, _layout, _kind, ...)       \
    MRP_##_kind##_ARRAY(_objtype, _member, _layout, __VA_ARGS__)

/** Macro to declare a native type. */
#define MRP_NATIVE_TYPE(_var, _type, ...)                       \
    mrp_native_member_t _var##_members[] = {                    \
        __VA_ARGS__                                             \
    };                                                          \
    mrp_native_type_t   _var = {                                \
        .id      = -1,                                          \
        .name    = #_type,                                      \
        .size    = sizeof(_type),                               \
        .members = _var##_members,                              \
        .nmember = MRP_ARRAY_SIZE(_var##_members),              \
        .hook    = { NULL, NULL },                              \
    }

/** Declare and register the given native type. */
uint32_t mrp_register_native(mrp_native_type_t *type);

/** Look up the type id of the given native type name. */
uint32_t mrp_native_id(const char *type_name);

/** Encode data of the given native type. */
int mrp_encode_native(void *data, uint32_t id, size_t reserve, void **bufp,
                      size_t *sizep, mrp_typemap_t *idmap);

/** Decode data of (the given) native type (if specified). */
int mrp_decode_native(void **bufp, size_t *sizep, void **datap, uint32_t *idp,
                      mrp_typemap_t *idmap);

/** Free data of the given native type, obtained from mrp_decode_native. */
void mrp_free_native(void *data, uint32_t id);

/** Print data of the given native type. */
ssize_t mrp_print_native(char *buf, size_t size, void *data, uint32_t id);

MRP_CDECL_END

#endif /* __MURPHY_COMMON_NATIVE_TYPES_H__ */
