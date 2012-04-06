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


typedef struct mrp_plugin_s mrp_plugin_t;

typedef int  (*mrp_plugin_init_t)(mrp_plugin_t *);
typedef void (*mrp_plugin_exit_t)(mrp_plugin_t *);


/*
 * plugin descriptors
 */

typedef struct {
    char                *name;                   /* plugin name */
    char                *path;                   /* plugin path */
    mrp_plugin_init_t    init;                   /* initialize plugin */
    mrp_plugin_exit_t    exit;                   /* cleanup plugin */
    const char          *help;                   /* plugin help */
    int                  core;                   /* whether a core plugin */
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
};


/*
 * plugin arguments
 */

typedef enum {
    MRP_PLUGIN_ARG_TYPE_UNKNOWN = 0,
    MRP_PLUGIN_ARG_TYPE_STRING,
    MRP_PLUGIN_ARG_TYPE_UINT32
} mrp_plugin_arg_type_t;

typedef struct {
    char                  *key;
    mrp_plugin_arg_type_t  type;
    union {
	char              *str;
	bool               bln;
	uint32_t           u32;
	int32_t            i32;
    };
} mrp_plugin_arg_t;


#ifdef __MURPHY_BUILTIN_PLUGIN__
/*   statically linked in plugins */
#    define __MURPHY_REGISTER_PLUGIN(_name,				\
				     _help,				\
				     _init,				\
				     _exit,				\
				     _core)				\
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
				     _core)				\
    									\
    mrp_plugin_descr_t *mrp_get_plugin_descriptor(void) {		\
	static mrp_plugin_descr_t descriptor = {			\
	    .name = _name,						\
	    .init = _init,						\
	    .exit = _exit,						\
	    .help = _help,						\
	    .core = _core,						\
	};								\
									\
	return &descriptor;						\
    }									\
    struct mrp_allow_trailing_semicolon
#endif


#define MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e)		\
    __MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e, FALSE)

#define MURPHY_REGISTER_CORE_PLUGIN(_n, _h, _i, _e)	\
    __MURPHY_REGISTER_PLUGIN(_n, _h, _i, _e, TRUE)


int mrp_register_builtin_plugin(mrp_plugin_descr_t *descr);
int mrp_plugin_exists(mrp_context_t *ctx, const char *name);
mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
			      const char *instance, char **args, int narg);
int mrp_unload_plugin(mrp_plugin_t *plugin);
int mrp_start_plugins(mrp_context_t *ctx);
int mrp_start_plugin(mrp_plugin_t *plugin);
int mrp_stop_plugin(mrp_plugin_t *plugin);

#endif /* __MURPHY_PLUGIN_H__ */
