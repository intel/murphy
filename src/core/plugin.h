#ifndef __MURPHY_PLUGIN_H__
#define __MURPHY_PLUGIN_H__

#include <stdbool.h>
#include <dlfcn.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/core/context.h>
#include <murphy/core/console-command.h>

#ifndef MRP_DEFAULT_PLUGIN_DIR
#    define MRP_DEFAULT_PLUGIN_DIR LIBDIR"/murphy/plugins"
#endif

#define MRP_PLUGIN_DESCRIPTOR "mrp_get_plugin_descriptor"


/*
 * plugin arguments
 */

typedef enum {
    MRP_PLUGIN_ARG_TYPE_UNKNOWN = 0,
    MRP_PLUGIN_ARG_TYPE_STRING,
    MRP_PLUGIN_ARG_TYPE_BOOL,
    MRP_PLUGIN_ARG_TYPE_UINT32,
    MRP_PLUGIN_ARG_TYPE_INT32,
    MRP_PLUGIN_ARG_TYPE_DOUBLE,
    /* add more as needed */
} mrp_plugin_arg_type_t;

typedef struct {
    char                  *key;          /* plugin argument name */
    mrp_plugin_arg_type_t  type;         /* plugin argument type */
    union {                              /* default/supplied value */
        char              *str;          /* string values */
        bool               bln;          /* boolean values */
        uint32_t           u32;          /* 32-bit unsigned values */
        int32_t            i32;          /* 32-bit signed values */
        double             dbl;          /* double prec. floating pt. values */
    };
} mrp_plugin_arg_t;


/** Convenience macros for setting up argument tables with type and defaults. */
#define MRP_PLUGIN_ARG_STRING(name, defval)                                \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_STRING, { str: defval } }

#define MRP_PLUGIN_ARG_BOOL(name, defval)                                  \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_BOOL  , { bln: !!defval } }

#define MRP_PLUGIN_ARG_UINT32(name, defval)                                \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_UINT32, { u32: defval } }

#define MRP_PLUGIN_ARG_INT32(name, defval)                                 \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_INT32 , { i32: defval } }

#define MRP_PLUGIN_ARG_DOUBLE(name, defval)                                \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_DOUBLE, { dbl: defval } }

/** Similar convenience macros for indexed argument access. */
#define MRP_PLUGIN_ARGIDX_STRING(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_STRING(name, defval)

#define MRP_PLUGIN_ARGIDX_STRING(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_STRING(name, defval)

#define MRP_PLUGIN_ARGIDX_BOOL(idx, name, defval)   \
    [idx] MRP_PLUGIN_ARG_BOOL(name, defval)

#define MRP_PLUGIN_ARGIDX_UINT32(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_UINT32(name, defval)

#define MRP_PLUGIN_ARGIDX_INT32(idx, name, defval)  \
    [idx] MRP_PLUGIN_ARG_INT32(name, defval)

#define MRP_PLUGIN_ARGIDX_DOUBLE(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_DOUBLE(name, defval)

/**
 * Generic convenience macro for indexed argument access.
 *
 * Here is how you can use these macros to declare and access your plugin
 * arguments:
 *
 * #define TEST_HELP "Just a stupid test..."
 * #define TEST_DESCRIPTION "A test plugin."
 * #define TEST_AUTHORS "D. Duck <donald.duck@ducksburg.org>"
 *
 * enum {
 *     ARG_FOO,
 *     ARG_BAR,
 *     ARG_FOOBAR,
 *     ARG_BARFOO
 * };
 *
 * mrp_plugin_arg_t test_args[] = {
 *     MRP_PLUGIN_ARGIDX(ARG_FOO   , STRING, "foo"   , "default foo"),
 *     MRP_PLUGIN_ARGIDX(ARG_BAR   , BOOL  , "bar"   , FALSE        ),
 *     MRP_PLUGIN_ARGIDX(ARG_FOOBAR, UINT32, "foobar", 1984         ),
 *     MRP_PLUGIN_ARGIDX(ARG_BARFOO, DOUBLE, "barfoo", 3.141        ),
 * };
 *
 * static int test_init(mrp_plugin_t *plugin)
 * {
 *     mrp_plugin_arg_t *args = plugin->args;
 *
 *     if (args[ARG_BAR].bln) {
 *         mrp_log_info("   foo: %s", args[ARG_FOO].str);
 *         mrp_log_info("foobar: %u", args[ARG_FOOBAR].u32);
 *         mrp_log_info("barfoo: %f", args[ARG_BARFOO].dbl);
 *     }
 *     else
 *         mrp_log_info("I was not asked to dump my arguments...");
 *     ...
 * }
 *
 * MURPHY_REGISTER_PLUGIN("test", TEST_DESCRIPTION, TEST_AUTHORS, TEST_HELP,
 *                        MRP_MULTIPLE, test_init, test_exit, test_args);
 */

#define MRP_PLUGIN_ARGIDX(idx, type, name, defval) \
    [idx] MRP_PLUGIN_ARG_##type(name, defval)


/*
 * plugin API version
 */

#define MRP_PLUGIN_API_MAJOR 0
#define MRP_PLUGIN_API_MINOR 1

#define MRP_PLUGIN_API_VERSION \
    MRP_VERSION_INT(MRP_PLUGIN_API_MAJOR, MRP_PLUGIN_API_MINOR, 0)

#define MRP_PLUGIN_API_VERSION_STRING \
    MRP_VERSION_STRING(MRP_PLUGIN_API_MAJOR, MRP_PLUGIN_API_MINOR, 0)


/*
 * plugin descriptors
 */


typedef struct mrp_plugin_s mrp_plugin_t;

typedef int  (*mrp_plugin_init_t)(mrp_plugin_t *);
typedef void (*mrp_plugin_exit_t)(mrp_plugin_t *);

#define MRP_SINGLETON TRUE
#define MRP_MULTIPLE  FALSE

typedef struct {
    char                *name;                 /* plugin name */
    char                *path;                 /* plugin path */
    mrp_plugin_init_t    init;                 /* initialize plugin */
    mrp_plugin_exit_t    exit;                 /* cleanup plugin */
    mrp_plugin_arg_t    *args;                 /* table of valid arguments */
    int                  narg;                 /* number of valid arguments */
    int                  core : 1;             /* is this a core plugin? */
    int                  singleton : 1;        /* deny multiple instances? */
    int                  ninstance;            /* number of instances */
    /* miscallaneous plugin metadata */
    int                  version;              /* plugin version */
    int                  mrp_version;          /* murphy API version */
    const char          *description;          /* plugin description */
    const char          *authors;              /* plugin authors */
    const char          *help;                 /* plugin help string */
    mrp_console_group_t *cmds;                 /* default console commands */
} mrp_plugin_descr_t;


/*
 * plugins
 */

typedef enum {
    MRP_PLUGIN_LOADED = 0,                     /* has been loaded */
    MRP_PLUGIN_RUNNING,                        /* has been started */
    MRP_PLUGIN_STOPPED,                        /* has been stopped */
} mrp_plugin_state_t;

struct mrp_plugin_s {
    char                *path;                 /* plugin path */
    char                *instance;             /* plugin instance */
    mrp_list_hook_t      hook;                 /* hook to list of plugins */
    mrp_context_t       *ctx;                  /* murphy context */
    mrp_plugin_descr_t  *descriptor;           /* plugin descriptor */
    void                *handle;               /* DSO handle */
    mrp_plugin_state_t   state;                /* plugin state */
    int                  refcnt;               /* reference count */
    void                *data;                 /* private plugin data */
    mrp_plugin_arg_t    *args;                 /* plugin arguments */
    mrp_console_group_t *cmds;                 /* default console commands */
    int                  may_fail : 1;         /* load / start may fail */
};


#ifdef __MURPHY_BUILTIN_PLUGIN__
/*   statically linked in plugins */
#    define __MURPHY_REGISTER_PLUGIN(_name,                               \
                                     _version,                            \
                                     _description,                        \
                                     _authors,                            \
                                     _help,                               \
                                     _core,                               \
                                     _single,                             \
                                     _init,                               \
                                     _exit,                               \
                                     _args,                               \
                                     _cmds)                               \
    static void register_plugin(void) __attribute__((constructor));       \
                                                                          \
    static void register_plugin(void) {                                   \
        char *path = __FILE__, *base;                                     \
        static mrp_plugin_descr_t descriptor = {                          \
            .name        = _name,                                         \
            .version     = _version,                                      \
            .description = _description,                                  \
            .authors     = _authors,                                      \
            .mrp_version = MRP_PLUGIN_API_VERSION,                        \
            .help        = _help,                                         \
            .init        = _init,                                         \
            .exit        = _exit,                                         \
            .core        = _core,                                         \
            .singleton   = _single,                                       \
            .ninstance   = 0,                                             \
            .args        = _args,                                         \
            .narg        = MRP_ARRAY_SIZE(_args),                         \
            .cmds = _cmds,                                                \
        };                                                                \
                                                                          \
        if ((base = strrchr(path, '/')) != NULL)                          \
            descriptor.path = base + 1;                                   \
        else                                                              \
            descriptor.path = (char *)path;                               \
                                                                          \
        mrp_register_builtin_plugin(&descriptor);                         \
    }                                                                     \
    struct mrp_allow_trailing_semicolon
#else /* dynamically loaded plugins */
#    define __MURPHY_REGISTER_PLUGIN(_name,                               \
                                     _version,                            \
                                     _description,                        \
                                     _authors,                            \
                                     _help,                               \
                                     _core,                               \
                                     _single,                             \
                                     _init,                               \
                                     _exit,                               \
                                     _args,                               \
                                     _cmds)                               \
                                                                          \
    mrp_plugin_descr_t *mrp_get_plugin_descriptor(void) {                 \
        static mrp_plugin_descr_t descriptor = {                          \
            .name        = _name,                                         \
            .version     = _version,                                      \
            .description = _description,                                  \
            .authors     = _authors,                                      \
            .mrp_version = MRP_PLUGIN_API_VERSION,                        \
            .help        = _help,                                         \
            .init        = _init,                                         \
            .exit        = _exit,                                         \
            .core        = _core,                                         \
            .singleton   = _single,                                       \
            .ninstance   = 0,                                             \
            .args        = _args,                                         \
            .narg        = MRP_ARRAY_SIZE(_args),                         \
            .cmds        = _cmds,                                         \
        };                                                                \
                                                                          \
        return &descriptor;                                               \
    }                                                                     \
    struct mrp_allow_trailing_semicolon
#endif


#define MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, _s, _i, _e, _args, _c) \
    __MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, FALSE, _s, _i, _e, _args, _c)

#define MURPHY_REGISTER_CORE_PLUGIN(_n, _v, _d, _a, _h, _s, _i, _e, _args, _c) \
    __MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, TRUE, _s, _i, _e, _args, _c)

#define MRP_REGISTER_PLUGIN MURPHY_REGISTER_PLUGIN
#define MRP_REGISTER_CORE_PLUGIN MURPHY_REGISTER_CORE_PLUGIN


int mrp_register_builtin_plugin(mrp_plugin_descr_t *descr);
int mrp_plugin_exists(mrp_context_t *ctx, const char *name);
mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
                              const char *instance, mrp_plugin_arg_t *args,
                              int narg);
int mrp_load_all_plugins(mrp_context_t *ctx);
int mrp_unload_plugin(mrp_plugin_t *plugin);
int mrp_start_plugins(mrp_context_t *ctx);
int mrp_start_plugin(mrp_plugin_t *plugin);
int mrp_stop_plugin(mrp_plugin_t *plugin);
int mrp_request_plugin(mrp_context_t *ctx, const char *name,
                       const char *instance);


#endif /* __MURPHY_PLUGIN_H__ */
