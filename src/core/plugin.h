#ifndef __MURPHY_PLUGIN_H__
#define __MURPHY_PLUGIN_H__

#include <stdbool.h>
#include <dlfcn.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/core/context.h>

#ifndef MRP_DEFAULT_PLUGIN_DIR
#    define MRP_DEFAULT_PLUGIN_DIR "/usr/lib/murphy/plugins"
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
    mrp_plugin_arg_type_t  type;         /* expected type */
    union {                              /* default/supplied value */
	char              *str;
	bool               bln;
	uint32_t           u32;
	int32_t            i32;
	double             dbl;
    };
} mrp_plugin_arg_t;


/** Convenience macros for setting up argument tables with type and defaults. */
#define MRP_PLUGIN_ARG_STRING(name, defval)				\
    { key: name, type: MRP_PLUGIN_ARG_TYPE_STRING, { str: defval } }

#define MRP_PLUGIN_ARG_BOOL(name, defval)				\
    { key: name, type: MRP_PLUGIN_ARG_TYPE_BOOL  , { bln: !!defval } }

#define MRP_PLUGIN_ARG_UINT32(name, defval)				\
    { key: name, type: MRP_PLUGIN_ARG_TYPE_UINT32, { u32: defval } }

#define MRP_PLUGIN_ARG_INT32(name, defval)				\
    { key: name, type: MRP_PLUGIN_ARG_TYPE_INT32 , { i32: defval } }

#define MRP_PLUGIN_ARG_DOUBLE(name, defval)				\
    { key: name, type: MRP_PLUGIN_ARG_TYPE_DOUBLE, { dbl: defval } }

/** Similar convenience macros for indexed argument access. */
#define MRP_PLUGIN_ARGIDX_STRING(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_STRING(name, defval)

#define MRP_PLUGIN_ARGIDX_STRING(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_STRING(name, defval)

#define MRP_PLUGIN_ARGIDX_BOOL(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_BOOL(name, defval)

#define MRP_PLUGIN_ARGIDX_UINT32(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_UINT32(name, defval)

#define MRP_PLUGIN_ARGIDX_INT32(idx, name, defval) \
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
 * MURPHY_REGISTER_PLUGIN("test", TEST_HELP, test_init, test_exit, test_args);
 */

#define MRP_PLUGIN_ARGIDX(idx, type, name, defval)	\
    [idx] MRP_PLUGIN_ARG_##type(name, defval)



/*
 * plugin descriptors
 */

typedef struct mrp_plugin_s mrp_plugin_t;

typedef int  (*mrp_plugin_init_t)(mrp_plugin_t *);
typedef void (*mrp_plugin_exit_t)(mrp_plugin_t *);


typedef struct {
    char                *name;                   /* plugin name */
    char                *path;                   /* plugin path */
    mrp_plugin_init_t    init;                   /* initialize plugin */
    mrp_plugin_exit_t    exit;                   /* cleanup plugin */
    const char          *help;                   /* plugin help */
    int                  core;                   /* whether a core plugin */
    mrp_plugin_arg_t    *args;                   /* table of valid arguments */
    int                  narg;                   /* number of valid arguments */
} mrp_plugin_descr_t;


/*
 * plugins
 */

struct mrp_plugin_s {
    char               *path;                    /* plugin path */
    char               *instance;                /* plugin instance */
    mrp_list_hook_t     hook;                    /* hook to list of plugins */
    mrp_context_t      *ctx;                     /* murphy context */
    mrp_plugin_descr_t *descriptor;              /* plugin descriptor */
    void               *handle;                  /* DSO handle */
    int                 refcnt;                  /* reference count */
    void               *data;                    /* private plugin data */
    mrp_plugin_arg_t   *args;                    /* plugin arguments */
};


#ifdef __MURPHY_BUILTIN_PLUGIN__
/*   statically linked in plugins */
#    define __MURPHY_REGISTER_PLUGIN(_name,				\
				     _help,				\
				     _init,				\
				     _exit,				\
				     _core,				\
				     _args)				\
    static void register_plugin(void) __attribute__((constructor));	\
    									\
    static void register_plugin(void) {					\
	char *path = __FILE__, *base;					\
	static mrp_plugin_descr_t descriptor = {			\
	    .name = _name,						\
	    .init = _init,						\
	    .exit = _exit,						\
	    .help = _help,						\
	    .core = _core,						\
            .args = _args,						\
	    .narg = MRP_ARRAY_SIZE(_args),				\
	};								\
									\
	if ((base = strrchr(path, '/')) != NULL)			\
	    descriptor.path = base + 1;					\
	else								\
	    descriptor.path = (char *)path;				\
									\
	mrp_register_builtin_plugin(&descriptor);			\
    }									\
    struct mrp_allow_trailing_semicolon
#else /* dynamically loaded plugins */
#    define __MURPHY_REGISTER_PLUGIN(_name,				\
				     _help,				\
				     _init,				\
				     _exit,				\
				     _core,				\
				     _args)				\
    									\
    mrp_plugin_descr_t *mrp_get_plugin_descriptor(void) {		\
	static mrp_plugin_descr_t descriptor = {			\
	    .name = _name,						\
	    .init = _init,						\
	    .exit = _exit,						\
	    .help = _help,						\
	    .core = _core,						\
	    .args = _args,						\
	    .narg = MRP_ARRAY_SIZE(_args),				\
	};								\
									\
	return &descriptor;						\
    }									\
    struct mrp_allow_trailing_semicolon
#endif


#define MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e, _a)		\
    __MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e, FALSE, _a)

#define MURPHY_REGISTER_CORE_PLUGIN(_n, _h, _i, _e, _a)	\
    __MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e, TRUE, _a)

#define MRP_REGISTER_PLUGIN MURPHY_REGISTER_PLUGIN
#define MRP_REGISTER_CORE_PLUGIN MURPHY_REGISTER_CORE_PLUGIN


int mrp_register_builtin_plugin(mrp_plugin_descr_t *descr);
int mrp_plugin_exists(mrp_context_t *ctx, const char *name);
mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
			      const char *instance, mrp_plugin_arg_t *args,
			      int narg);
int mrp_unload_plugin(mrp_plugin_t *plugin);
int mrp_start_plugins(mrp_context_t *ctx);
int mrp_start_plugin(mrp_plugin_t *plugin);
int mrp_stop_plugin(mrp_plugin_t *plugin);

#endif /* __MURPHY_PLUGIN_H__ */
