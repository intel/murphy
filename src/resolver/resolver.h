#ifndef __MURPHY_RESOLVER_H__
#define __MURPHY_RESOLVER_H__

#include <stdio.h>
#include <stdbool.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

typedef struct mrp_resolver_s    mrp_resolver_t;
typedef struct mrp_interpreter_s mrp_interpreter_t;
typedef struct mrp_script_s      mrp_script_t;


/*
 * data types to pass to/from scripts
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

typedef struct {
    mrp_script_type_t type;
    MRP_SCRIPT_VALUE_UNION;
} mrp_script_value_t;

#define mrp_script_typed_value_t mrp_script_value_t

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


/** Parse the given resolver input file into a resolver context. */
mrp_resolver_t *mrp_resolver_parse(const char *path);

/** Destroy the given resolver context, freeing all associated resources. */
void mrp_resolver_cleanup(mrp_resolver_t *r);

/** Update the given target. The NULL-terminated variable argument list
    after the target name sepcifies the resolver context variables to
    set during the update. Use a single NULL to omit variables. */
int mrp_resolver_update_targetl(mrp_resolver_t *r,
                                const char *target, ...) MRP_NULLTERM;

/** Update the given target. The variable name and type/value arrays
    specify the resolver context variables to set during the update. */
int mrp_resolver_update_targetv(mrp_resolver_t *r, const char *target,
                                const char **variables,
                                mrp_script_value_t *values,
                                int nvariable);

/** Declare a context variable with a given type. */
int mrp_resolver_declare_variable(mrp_resolver_t *r, const char *name,
                                  mrp_script_type_t type);


/** Get the value of a context variable by id. */
int mrp_resolver_get_value(mrp_resolver_t *r, int id, mrp_script_value_t *v);
#define mrp_resolver_get_value_by_id mrp_resolver_get_value

/** Get the value of a context variable by name. */
int mrp_resolver_get_value_by_name(mrp_resolver_t *r, const char *name,
                                   mrp_script_value_t *v);

/** Print the given value to the given buffer. */
char *mrp_print_value(char *buf, size_t size, mrp_script_value_t *value);

/** Produce a debug dump of all targets. */
void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp);

/** Produce a debug dump of all tracked facts. */
void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp);

/** Register a script interpreter. */
int mrp_resolver_register_interpreter(mrp_interpreter_t *i);

/** Unregister a script interpreter. */
int mrp_resolver_unregister_interpreter(const char *name);


MRP_CDECL_END

#endif /* __MURPHY_RESOLVER_H__ */
