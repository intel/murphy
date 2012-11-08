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

#ifndef __MURPHY_CORE_SCRIPTING_H__
#define __MURPHY_CORE_SCRIPTING_H__

#include <stdio.h>
#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>


MRP_CDECL_BEGIN

typedef struct mrp_interpreter_s  mrp_interpreter_t;
typedef struct mrp_scriptlet_s    mrp_scriptlet_t;
typedef struct mrp_context_tbl_s  mrp_context_tbl_t;
typedef struct mrp_script_value_s mrp_script_value_t;


/*
 * call/execution context passed to exported boilerplate methods
 *
 * This context is used to pass positional and keyword arguments
 * when calling exported scripting boilerplate methods. For instance
 * the primitive resolver scriptlet interpreter uses this to execute
 * function calls.
 */

typedef struct {
    mrp_script_value_t *args;            /* positional arguments */
    int                 narg;            /* number of arguments */
    mrp_context_tbl_t  *ctbl;            /* named arguments */
} mrp_script_env_t;


/*
 * supported data types to pass to/from scripts (XXX TODO: arrays...)
 */

#define A(t) MRP_SCRIPT_TYPE_##t
typedef enum {
    MRP_SCRIPT_TYPE_UNKNOWN = 0x00,
    MRP_SCRIPT_TYPE_INVALID = 0x00,      /* defined invalid type */
    MRP_SCRIPT_TYPE_STRING  = 0x01,      /* string */
    MRP_SCRIPT_TYPE_BOOL    = 0x02,      /* boolean */
    MRP_SCRIPT_TYPE_UINT8   = 0x03,      /* signed 8-bit integer */
    MRP_SCRIPT_TYPE_SINT8   = 0x04,      /* unsigned 8-bit integer */
    MRP_SCRIPT_TYPE_INT8    = A(SINT8),  /* alias for SINT8 */
    MRP_SCRIPT_TYPE_UINT16  = 0x05,      /* unsigned 16-bit integer */
    MRP_SCRIPT_TYPE_SINT16  = 0x06,      /* signed 16-bit integer */
    MRP_SCRIPT_TYPE_INT16   = A(SINT16), /* alias for SINT16 */
    MRP_SCRIPT_TYPE_UINT32  = 0x07,      /* unsigned 32-bit integer */
    MRP_SCRIPT_TYPE_SINT32  = 0x08,      /* signed 32-bit integer */
    MRP_SCRIPT_TYPE_INT32   = A(SINT32), /* alias for SINT32 */
    MRP_SCRIPT_TYPE_UINT64  = 0x09,      /* unsigned 64-bit integer */
    MRP_SCRIPT_TYPE_SINT64  = 0x0a,      /* signed 64-bit integer */
    MRP_SCRIPT_TYPE_INT64   = A(SINT64), /* alias for SINT64 */
    MRP_SCRIPT_TYPE_DOUBLE  = 0x0b,      /* double-prec. floating point */
    MRP_SCRIPT_TYPE_ARRAY   = 0x80,      /* type/marker for arrays */
} mrp_script_type_t;
#undef A

#define MRP_SCRIPT_VALUE_UNION union {          \
        char      *str;                         \
        bool       bln;                         \
        uint8_t    u8;                          \
        int8_t     s8;                          \
        uint16_t   u16;                         \
        int16_t    s16;                         \
        uint32_t   u32;                         \
        int32_t    s32;                         \
        uint64_t   u64;                         \
        int64_t    s64;                         \
        double     dbl;                         \
    }

typedef MRP_SCRIPT_VALUE_UNION mrp_script_value_u;

struct mrp_script_value_s {
    mrp_script_type_t type;
    MRP_SCRIPT_VALUE_UNION;
};

/** Helper macros for passing values to variadic arglist functions. */
#define MRP_SCRIPT_STRING(s) MRP_SCRIPT_TYPE_STRING, s
#define MRP_SCRIPT_BOOL(b)   MRP_SCRIPT_TYPE_BOOL  , b
#define MRP_SCRIPT_UINT8(u)  MRP_SCRIPT_TYPE_UINT8 , u
#define MRP_SCRIPT_SINT8(s)  MRP_SCRIPT_TYPE_SINT8 , s
#define MRP_SCRIPT_UINT16(u) MRP_SCRIPT_TYPE_UINT16, u
#define MRP_SCRIPT_SINT16(s) MRP_SCRIPT_TYPE_SINT16, s
#define MRP_SCRIPT_UINT32(u) MRP_SCRIPT_TYPE_UINT32, u
#define MRP_SCRIPT_SINT32(s) MRP_SCRIPT_TYPE_SINT32, s
#define MRP_SCRIPT_UINT64(u) MRP_SCRIPT_TYPE_UINT64, u
#define MRP_SCRIPT_DOUBLE(d) MRP_SCRIPT_TYPE_DOUBLE, d

/** Helper macro for initializing/assigning to value arrays. */
#define __MRP_SCRIPT_VALUE(_t, _m, _v) \
    (mrp_script_value_t){ .type = MRP_SCRIPT_TYPE_##_t, ._m = _v }

#define MRP_SCRIPT_VALUE_STRING(v) __MRP_SCRIPT_VALUE(STRING, str, v)
#define MRP_SCRIPT_VALUE_BOOL(v)   __MRP_SCRIPT_VALUE(BOOL  , bln, v)
#define MRP_SCRIPT_VALUE_UINT8(v)  __MRP_SCRIPT_VALUE(UINT8 , u8 , v)
#define MRP_SCRIPT_VALUE_SINT8(v)  __MRP_SCRIPT_VALUE(SINT8 , s8 , v)
#define MRP_SCRIPT_VALUE_UINT16(v) __MRP_SCRIPT_VALUE(UINT16, u16, v)
#define MRP_SCRIPT_VALUE_SINT16(v) __MRP_SCRIPT_VALUE(SINT16, s16, v)
#define MRP_SCRIPT_VALUE_UINT32(v) __MRP_SCRIPT_VALUE(UINT32, u32, v)
#define MRP_SCRIPT_VALUE_SINT32(v) __MRP_SCRIPT_VALUE(SINT32, s32, v)
#define MRP_SCRIPT_VALUE_UINT64(v) __MRP_SCRIPT_VALUE(UINT64, u64, v)
#define MRP_SCRIPT_VALUE_SINT64(v) __MRP_SCRIPT_VALUE(SINT64, s64, v)
#define MRP_SCRIPT_VALUE_DOUBLE(v) __MRP_SCRIPT_VALUE(DOUBLE, dbl, v)

/** Print the given value to the given buffer. */
char *mrp_print_value(char *buf, size_t size, mrp_script_value_t *value);


/*
 * a script interpreter as exposed to the resolver
 */

struct mrp_interpreter_s {
    mrp_list_hook_t    hook;             /* to list of interpreters */
    const char        *name;             /* interpreter identifier */
    void              *data;             /* opaque global interpreter data */
    /*                                      interpreter operations */
    int  (*compile)(mrp_scriptlet_t *script);
    int  (*prepare)(mrp_scriptlet_t *script);
    int  (*execute)(mrp_scriptlet_t *script, mrp_context_tbl_t *ctbl);
    void (*cleanup)(mrp_scriptlet_t *script);
};

/** Macro to automatically register an interpreter on startup. */
#define MRP_REGISTER_INTERPRETER(_type, _compile, _prepare, _execute,   \
                                 _cleanup)                              \
    static void auto_register_interpreter(void)                         \
         __attribute__((constructor));                                  \
                                                                        \
    static void auto_register_interpreter(void) {                       \
        static mrp_interpreter_t interpreter = {                        \
            .name    = _type,                                           \
            .compile = _compile,                                        \
            .prepare = _prepare,                                        \
            .execute = _execute,                                        \
            .cleanup = _cleanup                                         \
        };                                                              \
                                                                        \
        if (!mrp_register_interpreter(&interpreter))                    \
            mrp_log_error("Failed to register interpreter '%s'.",       \
                          _type);                                       \
        else                                                            \
            mrp_log_info("Registered interpreter '%s'.", _type);        \
    }                                                                   \
    struct mrp_allow_trailing_semicolon


/** Register a new scriptlet interpreter. */
int mrp_register_interpreter(mrp_interpreter_t *i);

/** Unregister a scriptlet interpreter. */
int mrp_unregister_interpreter(const char *type);

/** Find a scriptlet interpreter by type. */
mrp_interpreter_t *mrp_lookup_interpreter(const char *type);


/*
 * a resolver target update script
 */

struct mrp_scriptlet_s {
    char              *source;           /* scriptlet code */
    mrp_interpreter_t *interpreter;      /* interpreter handling this */
    void              *data;             /* opaque interpreter data */
    void              *compiled;         /* compiled scriptlet */
};

/** Create a scriptlet of the given type and source. */
mrp_scriptlet_t *mrp_create_script(const char *type, const char *source);

/** Destroy the given scriptlet, freeing all of its resources. */
void mrp_destroy_script(mrp_scriptlet_t *script);

/** Compile the given scriptlet. */
int mrp_compile_script(mrp_scriptlet_t *s);

/** Prepare the given scriptlet for execution. */
int mrp_prepare_script(mrp_scriptlet_t *s);

/** Execute the given scriptlet with the given context variables. */
int mrp_execute_script(mrp_scriptlet_t *s, mrp_context_tbl_t *ctbl);



/*
 * Context variable (keyword argument) handling.
 * XXX TODO: Uhmm... this needs to be rethought/redone. :-(
 */

mrp_context_tbl_t *mrp_create_context_table(void);
void mrp_destroy_context_table(mrp_context_tbl_t *tbl);
int mrp_declare_context_variable(mrp_context_tbl_t *tbl, const char *name,
                                 mrp_script_type_t type);
int mrp_push_context_frame(mrp_context_tbl_t *tbl);
int mrp_pop_context_frame(mrp_context_tbl_t *tbl);
int mrp_get_context_id(mrp_context_tbl_t *tbl, const char *name);
int mrp_get_context_value(mrp_context_tbl_t *tbl, int id,
                          mrp_script_value_t *value);
int mrp_set_context_value(mrp_context_tbl_t *tbl, int id,
                          mrp_script_value_t *value);
int mrp_get_context_value_by_name(mrp_context_tbl_t *tbl, const char *name,
                          mrp_script_value_t *value);
int mrp_set_context_value_by_name(mrp_context_tbl_t *tbl, const char *name,
                                  mrp_script_value_t *value);




MRP_CDECL_END




#endif /* __MURPHY_CORE_SCRIPTING_H__ */
