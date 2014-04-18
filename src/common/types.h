/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#ifndef __MURPHY_COMMON_TYPES_H__
#define __MURPHY_COMMON_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <murphy/common/macros.h>
#include <murphy/common/list.h>

MRP_CDECL_BEGIN

/*
 * type identifiers
 */

typedef enum {
    MRP_TYPE_INVALID = -1,
    MRP_TYPE_UNKNOWN =  0,
    MRP_TYPE_INT_START,                  /* integer type range start */
    MRP_TYPE_INT8 = MRP_TYPE_INT_START,  /* signed 8-bit int */
    MRP_TYPE_UINT8,                      /* unsigned 8-bit int */
    MRP_TYPE_INT16,                      /* signed 16-bit int */
    MRP_TYPE_UINT16,                     /* unsigned 16-bit int */
    MRP_TYPE_INT32,                      /* signed 32-bit int */
    MRP_TYPE_UINT32,                     /* unsigned 32-bit int */
    MRP_TYPE_INT64,                      /* signed 64-bit int */
    MRP_TYPE_UINT64,                     /* unsigned 64-bit int */
    MRP_TYPE_SHORT,                      /* signed platform short */
    MRP_TYPE_USHORT,                     /* unsigned platform short */
    MRP_TYPE_ENUM,                       /* alias for MRP_TYPE_INT */
    MRP_TYPE_INT,                        /* signed platform int */
    MRP_TYPE_UINT,                       /* unsigned platform int */
    MRP_TYPE_LONG,                       /* signed platform long */
    MRP_TYPE_ULONG,                      /* unsigned platform long */
    MRP_TYPE_SSIZE,                      /* platform ssize_t */
    MRP_TYPE_SIZE,                       /* platform ssize_t */
    MRP_TYPE_INT_END = MRP_TYPE_SIZE,    /* integer type range end */
    MRP_TYPE_FLOAT,                      /* floating point number */
    MRP_TYPE_DOUBLE,                     /* double prec. floating point number */
    MRP_TYPE_BOOL,                       /* boolean */
    MRP_TYPE_STRING,                     /* string */
    MRP_TYPE_HOOK,                       /* a list hook (mrp_list_hook_t) */
    MRP_TYPE_CUSTOM_MIN,                 /* lowest user defined type id */
    /*                                    * custom type ids */
    MRP_TYPE_CUSTOM_MAX = 0xffff,        /* highest user defined type id */

    /*
     * type modifiers
     */

    MRP_TYPE_SCALAR     = 0,             /* a scalar type */
    MRP_TYPE_ARRAY      = 1,             /* an array type */
    MRP_TYPE_LIST       = 2,             /* a list type */
    MRP_TYPE_UNION_KEY  = 3,             /* a union type selector */
    MRP_TYPE_UNION      = 4,             /* a union */
} mrp_type_id_t;

#define MRP_NUM_BASIC_TYPES MRP_TYPE_CUSTOM_MIN


/*
 * array types
 */

typedef enum {
    MRP_ARRAY_SIZED  = -2,               /* array with an explicit size */
    MRP_ARRAY_GUARD  = -1,               /* array with a sentinel-guard */
    MRP_ARRAY_FIXED  =  0,               /* array of a fixed number of items */
} mrp_array_type_t;


/*
 * type member layout
 */

typedef enum {
    MRP_LAYOUT_DEFAULT,                  /* type-specific default */
    MRP_LAYOUT_INLINED,                  /* stored inlined in type */
    MRP_LAYOUT_INDIRECT,                 /* stored behind a pointer in type */
} mrp_layout_t;


/*
 * a value of any type
 */

typedef union {
    int8_t             s8;
    uint8_t            u8;
    int16_t           s16;
    uint16_t          u16;
    int32_t           s32;
    uint32_t          u32;
    int64_t           s64;
    uint64_t          u64;
    short             ssi;
    unsigned short    usi;
    int                si;
    unsigned int       ui;
    long              sli;
    unsigned long     uli;
    ssize_t           ssz;
    size_t            usz;
    float             flt;
    double            dbl;
    bool              bln;
    char              str[0];
    char             *strp;
} mrp_value_t;


/*
 * a member definition
 */

typedef struct {
    const char    *name;                 /* member name */
    const char    *type;                 /* member type name */
    mrp_type_id_t  mod;                  /* type modifier (array/list) */
    size_t         offs;                 /* offset within type */
    size_t         size;                 /* size */
    mrp_layout_t   layout;               /* layout (if relevant) */
    bool           chkerr;               /* whether member type check failed */
    union {                              /* type-specific extra info */
        struct {                         /*   array info */
            mrp_array_type_t  type;      /*     sized/guarded/fixed */
            const char       *size;      /*     size/guard member name */
            mrp_value_t       guard;     /*     guard sentinel value */
        } array;
        struct {                         /*   list hook info */
            const char       *hook;      /*     hook member name */
        } list;
        struct {                         /*   union info */
            const char       *key;       /*     type key name */
            mrp_value_t       value;     /*     key value for this type */
        } unio/*n*/;
    };
} mrp_member_def_t;


/*
 * a type definition
 */

typedef struct {
    const char       *name;              /* type name */
    size_t            size;              /* size of this type */
    mrp_member_def_t *members;           /* members of this type */
    int               nmember;           /* number of members */
} mrp_type_def_t;


/*
 * a type map
 *
 * A type map can be used to map dynamically registered type ids to
 * a priori agreed static ones. This is typically used when passing
 * type data over a transport. Often there is no guarantee that the
 * same dynamic id gets assigned to the same type on both ends of
 * the transport. This happens if it cannot be guaranteed that the
 * same set of types are registered and in the exact same order on
 * both ends. Since the communicating parties must know all the
 * types that are potentially passed over the transport they can
 * assign (transport-specific) well known static ids to these and
 * use a type map to pass the necessary mapping information to the
 * encoder and decoder.
 */

typedef struct {
    mrp_type_id_t native;                /* registered type id */
    mrp_type_id_t mapped;                /* mapped type id */
} mrp_type_map_t;


/** Helper macro to check the given type of a member. Uhmmkay... */
#define __MRP_CHKTYPE(_type, _member, _member_type, _array)             \
    ((__builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member),   \
                                   _member_type) ||                     \
      __builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member),   \
                                   const _member_type) ||               \
      (_array &&                                                        \
       (__builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member), \
                                     _member_type *) ||                 \
        __builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member), \
                                     _member_type []) ||                \
        __builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member), \
                                     const _member_type *) ||           \
        __builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member), \
                                     const _member_type []))) ||        \
      (__builtin_types_compatible_p(__MRP_MEMBER_TYPE(_type, _member),  \
                                    char []) &&                         \
       (__builtin_types_compatible_p(_member_type, char *) ||           \
        __builtin_types_compatible_p(_member_type, const char *)))) ?   \
     true : false)

/** Helper macro to check if a type member is inlined. */
#define __MRP_INLINED(_type, _member, _member_type)              \
    (__builtin_types_compatible_p(typeof(((_type *)0)->_member), \
                                  _member_type []) ||            \
    (__builtin_types_compatible_p(typeof(((_type *)0)->_member), \
                                  _member_type)))

/** Helper macro to autodetect member layout. */
#define __MRP_LAYOUT(_type, _member, _member_type)              \
    __MRP_INLINED(_type, _member, char) ?                       \
        MRP_LAYOUT_INLINED :                                    \
            __MRP_INLINED(_type, _member, _member_type) ?       \
                MRP_LAYOUT_INLINED : MRP_LAYOUT_INDIRECT

/** Helper macro to get the C type of the given type member. */
#define __MRP_MEMBER_TYPE(_type, _member) typeof(((_type *)0)->_member)

/** Macro for defining an array member with an explicit size. */
#define __MRP_SIZED_ARRAY(_type, _member_type, _member, _offs,     \
                          _size, _layout, _size_member) {          \
        .name   = #_member,                                        \
        .type   = #_member_type,                                   \
        .mod    = MRP_TYPE_ARRAY,                                  \
        .offs   = _offs,                                           \
        .size   = _size,                                           \
        .layout = _layout,                                         \
        .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 1), \
        .array  = {                                                \
            .type   = MRP_ARRAY_SIZED,                             \
            .size   = #_size_member,                               \
            .guard  = { .u64 = 0ULL },                             \
        },                                                         \
    }

/** Macro for defining an array member with a sentinel guard member. */
#define __MRP_GUARD_ARRAY(_type, _member_type, _member, _offs,     \
                          _size, _layout, _guard, ...) {           \
        .name   = #_member,                                        \
        .type   = #_member_type,                                   \
        .mod    = MRP_TYPE_ARRAY,                                  \
        .offs   = _offs,                                           \
        .size   = _size,                                           \
        .layout = _layout,                                         \
        .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 1), \
        .array  = {                                                \
            .type   = MRP_ARRAY_GUARD,                             \
            .size   = #_guard,                                     \
            .guard  = __VA_ARGS__,                                 \
        },                                                         \
    }

/** Macro for defining a flexible array member. */
#define __MRP_FLEXI_ARRAY(_type, _member_type, _member, _offs,     \
                          _size, _layout, _guard, ...) {           \
        .name   = #_member,                                        \
        .type   = #_member_type,                                   \
        .mod    = MRP_TYPE_ARRAY,                                  \
        .offs   = _offs,                                           \
        .size   = 0,                                               \
        .layout = MRP_LAYOUT_INLINED,                              \
        .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 1), \
        .array  = {                                                \
            .type   = MRP_ARRAY_GUARD,                             \
            .size   = #_guard,                                     \
            .guard  = __VA_ARGS__,                                 \
        },                                                         \
    }

/** Macro for defining an array member with a fixed size. */
#define __MRP_FIXED_ARRAY(_type, _member_type, _member, _offs,     \
                          _size, _layout, _array_size) {           \
        .name   = _member,                                         \
        .type   = _member_type,                                    \
        .mod    = MRP_TYPE_ARRAY,                                  \
        .offs   = _offs,                                           \
        .size   = _size,                                           \
        .layout = _layout,                                         \
        .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 1), \
        .array  = {                                                \
            .type   = _array_size,                                 \
            .size   = NULL,                                        \
            .guard  = { .u64 = 0ULL },                             \
        },                                                         \
    }

/** Macro for defining a 'type selector key' for a union. */
#define __MRP_UNION_KEY(_type, _member_type, _member) {            \
       .type   = #_member_type,                                    \
       .name   = #_member,                                         \
       .mod    = MRP_TYPE_UNION_KEY,                               \
       .offs   = MRP_OFFSET(_type, _member),                       \
       .size   = sizeof(__MRP_MEMBER_TYPE(_type, _member)),        \
       .layout = __MRP_LAYOUT(_type, _member, _member_type),       \
       .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 0),  \
    }

/** Macro for defining a union type member with a 'key'. */
#define __MRP_UNION_MEMBER(_type, _member_type, _member,           \
                           _key, ...) {                            \
       .type   = #_member_type,                                    \
       .name   = #_member,                                         \
       .mod    = MRP_TYPE_UNION,                                   \
       .offs   = MRP_OFFSET(_type, _member),                       \
       .size   = sizeof(__MRP_MEMBER_TYPE(_type, _member)),        \
       .layout = __MRP_LAYOUT(_type, _member, _member_type),       \
       .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 0),  \
       .unio/*n*/ = {                                              \
           .key   = #_key,                                         \
           .value = __VA_ARGS__,                                   \
       },                                                          \
    }

/** Macro for defining scalar type members. */
#define MRP_SCALAR(_type, _member_type, _member) {                 \
       .type   = #_member_type,                                    \
       .name   = #_member,                                         \
       .mod    = MRP_TYPE_SCALAR,                                  \
       .offs   = MRP_OFFSET(_type, _member),                       \
       .size   = sizeof(__MRP_MEMBER_TYPE(_type, _member)),        \
       .layout = __MRP_LAYOUT(_type, _member, _member_type),       \
       .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 0),  \
    }

/** Macro for defining an array member of any type. */
#define MRP_ARRAY(_type, _member_type, _member,                    \
                  _array_type, ...)                                \
    __MRP_##_array_type##_ARRAY(_type, _member_type, _member,      \
                                MRP_OFFSET(_type, _member),        \
                                sizeof(((_type *)0)->_member),     \
                                __MRP_LAYOUT(_type, _member,       \
                                             _member_type),        \
                                __VA_ARGS__)

/** Macro for defining a list member. */
#define MRP_LIST(_type, _member_type, _member, _offs,              \
                 _hook_member) {                                   \
       .name   = #_member,                                         \
       .type   = #_member_type,                                    \
       .mod    = MRP_TYPE_LIST,                                    \
       .offs   = _offs,                                            \
       .size   = sizeof(__MRP_MEMBER_TYPE(_type, _member)),        \
       .layout = MRP_LAYOUT_INLINED,                               \
       .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 0),  \
       .list = {                                                   \
           .hook = #_hook_member,                                  \
       },                                                          \
    }

/** Macro for defining a list hook member. */
#define MRP_HOOK(_type, _member) {                                 \
       .type = "mrp_list_hook_t",                                  \
       .name = #_member,                                           \
       .mod  = MRP_TYPE_SCALAR,                                    \
       .offs = MRP_OFFSET(_type, _member),                         \
       .size = sizeof(__MRP_MEMBER_TYPE(_type, _member)),          \
       .layout = MRP_LAYOUT_INLINED,                               \
       .chkerr = !__MRP_CHKTYPE(_type, _member, _member_type, 0),  \
    }

/** Macro for defining a flexible array member (with [] as size). */
#define MRP_FLEXI(_type, _member_type, _member, ...)               \
    __MRP_FLEXI_ARRAY(_type, _member_type, _member,                \
                      MRP_OFFSET(_type, _member),                  \
                      0,                                           \
                      MRP_LAYOUT_INLINED,                          \
                      __VA_ARGS__)

/** Macro for defining a union key or member. */
#define MRP_UNION(_type, _member_type, _member, _what, ...)        \
    __MRP_UNION_##_what(_type, _member_type, _member ,## __VA_ARGS__)

/** Macro that gives the user full control in defining a member. */
#define MRP_MEMBER(_member_type, _member, _mod, _offs, _size,      \
                   _layout, ...) {                                 \
        .name   = #_member,                                        \
        .type   = #_member_type,                                   \
        .mod    = _mod,                                            \
        .offs   = _offs,                                           \
        .size   = _size,                                           \
        .layout = _layout,                                         \
        .chkerr = false ,## __VA_ARGS__                            \
    }

/** Macro for creating a type definition. */
#define MRP_DEFINE_TYPE(_var, _type, ...)                          \
    mrp_member_def_t __##_var##_members[] = { __VA_ARGS__ };       \
    mrp_type_def_t   _var = {                                      \
        .name    = #_type,                                         \
        .size    = sizeof(_type),                                  \
        .members = __##_var##_members,                             \
        .nmember = MRP_ARRAY_SIZE(__##_var##_members),             \
    }


/** Forward-declare the given type name. */
mrp_type_id_t mrp_declare_type(const char *name);

/** Declare the given type as enumeration (alias to an int). */
mrp_type_id_t mrp_declare_enum(const char *name);

/** Declare and register the given type. */
mrp_type_id_t mrp_register_type(mrp_type_def_t *def);

/** Look up the type id of the given type name. */
mrp_type_id_t mrp_type_id(const char *name);

/** Encode data of the given type. */
int mrp_encode_type(mrp_type_id_t id, void *data, void **bufp, size_t *sizep,
                    mrp_type_map_t *map, size_t reserve);

/** Decode data (of the given or any) type from the given buffer. */
int mrp_decode_type(mrp_type_id_t *idp, void **datap, void *buf, size_t size,
                    mrp_type_map_t *map);

/** Free decoded data of the given type. */
void mrp_free_type(mrp_type_id_t id, void *data);

/** Print data of the given type. */
ssize_t mrp_print_type(char *buf, size_t size, mrp_type_id_t id, void *data);

/** Print the type definition of the given type. */
ssize_t mrp_print_type_def(char *buf, size_t size, mrp_type_id_t id);

/** Print all type definitions. */
ssize_t mrp_print_type_defs(char *buf, size_t size);

MRP_CDECL_END

#endif /* __MURPHY_COMMON_TYPES_H__ */
