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

#ifndef __MURPHY_PLUGIN_H__
#define __MURPHY_PLUGIN_H__

#include <stdbool.h>
#include <dlfcn.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/refcnt.h>
#include <murphy/common/json.h>
#include <murphy/core/context.h>
#include <murphy/core/console-command.h>

typedef struct mrp_plugin_s mrp_plugin_t;

#include <murphy/core/method.h>

#ifndef MRP_DEFAULT_PLUGIN_DIR
#    define MRP_DEFAULT_PLUGIN_DIR LIBDIR"/murphy/plugins"
#endif

#define MRP_PLUGIN_DESCRIPTOR "mrp_get_plugin_descriptor"


/*
 * names of plugin-related events we emit
 */

#define MRP_PLUGIN_BUS            "plugin-bus"
#define MRP_PLUGIN_EVENT_LOADED   "plugin-loaded"
#define MRP_PLUGIN_EVENT_STARTED  "plugin-started"
#define MRP_PLUGIN_EVENT_FAILED   "plugin-failed"
#define MRP_PLUGIN_EVENT_STOPPING "plugin-stopping"
#define MRP_PLUGIN_EVENT_STOPPED  "plugin-stopped"
#define MRP_PLUGIN_EVENT_UNLOADED "plugin-unloaded"


/*
 * event message data tags
 */

#define MRP_PLUGIN_TAG_PLUGIN     ((uint16_t)1)  /* plugin name string */
#define MRP_PLUGIN_TAG_INSTANCE   ((uint16_t)2)  /* plugin instance string */


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
    MRP_PLUGIN_ARG_TYPE_OBJECT,
    MRP_PLUGIN_ARG_TYPE_UNDECL,
    /* add more as needed */
} mrp_plugin_arg_type_t;

typedef struct mrp_plugin_arg_s mrp_plugin_arg_t;

struct mrp_plugin_arg_s {
    char                  *key;          /* plugin argument name */
    mrp_plugin_arg_type_t  type;         /* plugin argument type */
    union {                              /* default/supplied value */
        char              *str;          /* string values */
        bool               bln;          /* boolean values */
        uint32_t           u32;          /* 32-bit unsigned values */
        int32_t            i32;          /* 32-bit signed values */
        double             dbl;          /* double prec. floating pt. values */
        struct {                         /* a JSON object */
            char       *str;
            mrp_json_t *json;
        } obj;
        struct {                         /* other undeclared arguments */
            mrp_plugin_arg_t *args;
            int               narg;
        } rest;
    };
};


/** Macro for declaring a plugin argument table. */
#define MRP_PLUGIN_ARGUMENTS(table, ...)     \
    static mrp_plugin_arg_t table[] =        \
        __VA_ARGS__                          \


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

#define MRP_PLUGIN_ARG_OBJECT(name, defval)                                \
    { key: name, type: MRP_PLUGIN_ARG_TYPE_OBJECT,                         \
            { obj: { str: defval, json: NULL } } }

#define MRP_PLUGIN_ARG_UNDECL(name, defval)                                \
    { key: "*", type: MRP_PLUGIN_ARG_TYPE_UNDECL,  { str: NULL } }

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

#define MRP_PLUGIN_ARGIDX_OBJECT(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_OBJECT(name, defval)

#define MRP_PLUGIN_ARGIDX_UNDECL(idx, name, defval) \
    [idx] MRP_PLUGIN_ARG_UNDECL(name, defval)

/** Macro for looping through all collected undeclared arguments. */
#define mrp_plugin_foreach_undecl_arg(_undecl, _arg)                    \
    for ((_arg) = (_undecl)->rest.args;                                 \
         (_arg) - (_undecl)->rest.args < (_undecl)->rest.narg;          \
         (_arg)++)


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
    mrp_method_descr_t  *exports;              /* exported methods */
    int                  nexport;              /* number of exported methods */
    mrp_method_descr_t  *imports;              /* imported methods */
    int                  nimport;              /* number of imported methods */
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
    mrp_refcnt_t         refcnt;               /* reference count */
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
                                     _narg,                               \
                                     _exports,                            \
                                     _nexport,                            \
                                     _imports,                            \
                                     _nimport,                            \
                                     _cmds)                               \
                                                                          \
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
            .narg        = _narg,                                         \
            .cmds        = _cmds,                                         \
            .exports     = _exports,                                      \
            .nexport     = _nexport,                                      \
            .imports     = _imports,                                      \
            .nimport     = _nimport,                                      \
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
                                     _narg,                               \
                                     _exports,                            \
                                     _nexport,                            \
                                     _imports,                            \
                                     _nimport,                            \
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
            .narg        = _narg,                                         \
            .cmds        = _cmds,                                         \
            .exports     = _exports,                                      \
            .nexport     = _nexport,                                      \
            .imports     = _imports,                                      \
            .nimport     = _nimport,                                      \
        };                                                                \
                                                                          \
        return &descriptor;                                               \
    }                                                                     \
    struct mrp_allow_trailing_semicolon
#endif


#define MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, _s, _i, _e,          \
                               _args, _narg,                            \
                               _exports, _nexport,                      \
                               _imports, _nimport,                      \
                               _cmds)                                   \
    __MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, FALSE, _s, _i, _e,     \
                             _args, _narg,                              \
                             _exports, _nexport,                        \
                             _imports, _nimport,                        \
                             _cmds)

#define MURPHY_REGISTER_CORE_PLUGIN(_n, _v, _d, _a, _h, _s, _i, _e,     \
                                    _args, _narg,                       \
                                    _exports, _nexport,                 \
                                    _imports, _nimport,                 \
                                    _cmds)                              \
    __MURPHY_REGISTER_PLUGIN(_n, _v, _d, _a, _h, TRUE, _s, _i, _e,      \
                             _args, _narg,                              \
                             _exports, _nexport,                        \
                             _imports, _nimport,                        \
                             _cmds)

#define MRP_REGISTER_PLUGIN MURPHY_REGISTER_PLUGIN
#define MRP_REGISTER_CORE_PLUGIN MURPHY_REGISTER_CORE_PLUGIN


int mrp_register_builtin_plugin(mrp_plugin_descr_t *descr);
int mrp_plugin_exists(mrp_context_t *ctx, const char *name);
mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
                              const char *instance, mrp_plugin_arg_t *args,
                              int narg);
int mrp_load_all_plugins(mrp_context_t *ctx);
int mrp_unload_plugin(mrp_plugin_t *plugin);
int mrp_plugin_loaded(mrp_context_t *ctx, const char *name);
int mrp_start_plugins(mrp_context_t *ctx);
int mrp_start_plugin(mrp_plugin_t *plugin);
int mrp_plugin_running(mrp_context_t *ctx, const char *name);
int mrp_stop_plugin(mrp_plugin_t *plugin);
int mrp_request_plugin(mrp_context_t *ctx, const char *name,
                       const char *instance);
void mrp_block_blacklisted_plugins(mrp_context_t *ctx);

mrp_plugin_arg_t *mrp_plugin_find_undecl_arg(mrp_plugin_arg_t *undecl,
                                             const char *key,
                                             mrp_plugin_arg_type_t type);


static inline mrp_plugin_t *mrp_ref_plugin(mrp_plugin_t *plugin)
{
    return mrp_ref_obj(plugin, refcnt);
}


static inline int mrp_unref_plugin(mrp_plugin_t *plugin)
{
    return mrp_unref_obj(plugin, refcnt);
}


#endif /* __MURPHY_PLUGIN_H__ */
