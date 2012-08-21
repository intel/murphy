#ifndef __MRP_RESOLVER_SCRIPT_H__
#define __MRP_RESOLVER_SCRIPT_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <murphy/common/macros.h>
#include <murphy/common/list.h>

MRP_CDECL_BEGIN

typedef struct mrp_interpreter_s mrp_interpreter_t;
typedef struct mrp_script_s      mrp_script_t;

#include "resolver.h"

/*
 * data types to pass to/from scripts
 */

#define A(t) MRP_SCRIPT_TYPE_##t
typedef enum {
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

typedef MRP_SCRIPT_VALUE_UNION mrp_script_value_t;

typedef struct {
    mrp_script_type_t type;
    MRP_SCRIPT_VALUE_UNION;
} mrp_script_typed_value_t;


/*
 * a script interpreter
 */

struct mrp_interpreter_s {
    mrp_list_hook_t    hook;             /* to list of interpreters */
    const char        *name;             /* interpreter identifier */
    void              *data;             /* opaque global interpreter data */
    /*                                      interpreter operations */
    int  (*compile)(mrp_script_t *script);
    int  (*execute)(mrp_script_t *script);
    void (*cleanup)(mrp_script_t *script);
#if 0
    void (*print)(char *buf, size_t size, mrp_script_t *script);
#endif
};

/** Macro to automatically register an interpreter on startup. */
#define MRP_REGISTER_INTERPRETER(_type, _compile, _execute, _cleanup)   \
    static void auto_register_interpreter(void)                         \
         __attribute__((constructor));                                  \
                                                                        \
    static void auto_register_interpreter(void) {                       \
        static mrp_interpreter_t interpreter = {                        \
            .name    = _type,                                           \
            .compile = _compile,                                        \
            .execute = _execute,                                        \
            .cleanup = _cleanup                                         \
        };                                                              \
                                                                        \
        mrp_list_init(&interpreter.hook);                               \
                                                                        \
        if (!mrp_resolver_register_interpreter(&interpreter))           \
            mrp_log_error("Failed to register interpreter '%s'.",       \
                          _type);                                       \
        else                                                            \
            mrp_log_info("Registered interpreter '%s'.", _type);        \
    }                                                                   \
    struct mrp_allow_trailing_semicolon



/*
 * (execution context for) a script
 */

struct mrp_script_s {
    char              *source;           /* script in source form */
    mrp_interpreter_t *interpreter;      /* interpreter for this */
    void              *data;             /* interpreter data */
    void              *compiled;         /* script in compiled form */
};


/** Set the default interpreter type. */
void set_default_interpreter(const char *type);

/** Register the given script interpreter. */
int register_interpreter(mrp_interpreter_t *i);

/** Unregister the given interpreter. */
void unregister_interpreter(mrp_interpreter_t *i);

/** Lookup an interpreter by name. */
mrp_interpreter_t *lookup_interpreter(const char *name);

/** Create (prepare) a script of the given type with the given source. */
mrp_script_t *create_script(char *type, const char *source);

/** Destroy the given script freeing all associated resources. */
void destroy_script(mrp_script_t *script);

/** Compile the given script, preparing it for execution. */
int compile_script(mrp_script_t *script);

/** Execute the given script. */
int execute_script(mrp_resolver_t *r, mrp_script_t *s, va_list ap);

/** Dummy routine that just prints the script to be evaluated. */
int eval_script(mrp_resolver_t *r, char *script, va_list ap);

MRP_CDECL_END

#endif /* __MRP_RESOLVER_SCRIPT_H__ */
