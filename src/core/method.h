#ifndef __MURPHY_CORE_METHOD_H__
#define __MURPHY_CORE_METHOD_H__

typedef struct mrp_method_descr_s mrp_method_descr_t;

#include <murphy/core/plugin.h>
#include <murphy/core/scripting.h>


/*
 * exported methods
 */

#define __MRP_METHOD_FIELDS(...)                                             \
    __VA_ARGS__ char *name;              /* method name */                   \
    __VA_ARGS__ char *signature;         /* method signature */              \
    /* pointers to exported function, native and boilerplate for scripts */  \
    void         *native_ptr;                                                \
    int         (*script_ptr)(mrp_plugin_t *plugin, const char *name,        \
                              mrp_script_env_t *env);                        \
    mrp_plugin_t *plugin                 /* exporting plugin (or NULL) */    \

struct mrp_method_descr_s {
    __MRP_METHOD_FIELDS(const);
};


/*
 * convenience macros for declaring exported methods
 */

#define MRP_EXPORTABLE(_return_type, _func, _arglist)     \
    const char _func##_method_signature[] =               \
        #_return_type" __ "#_arglist;                     \
                                                          \
    _return_type _func _arglist

/** Declare a method along with a boilerplate to call from scripts. */
#define MRP_GENERIC_METHOD(_name, _func, _boilerplate) {  \
            .name       = _name,                          \
            .signature  = _func##_method_signature,       \
            .native_ptr = _func,                          \
            .script_ptr = _boilerplate,                   \
            .plugin     = NULL,                           \
        }

/** Declare a method that cannot be called from scripts. */
#define MRP_NATIVE_METHOD(_name, _func) {                \
            .name       = _name,                         \
            .signature  = _func##_method_signature,      \
            .native_ptr = _func,                         \
            .script_ptr = NULL,                          \
            .plugin     = NULL,                          \
        }

/** Declare a method that can only be called via a boilerplate for scripts. */
#define MRP_SCRIPT_METHOD(_name, _boilerplate) {         \
            .name       = _name,                         \
            .signature  = NULL,                          \
            .native_ptr = NULL,                          \
            .script_ptr = _boilerplate,                  \
            .plugin     = NULL,                          \
        }


/*
 * convenience macros for declaring imported methods
 */

#define MRP_IMPORTABLE(_return_type, _funcptr, _arglist) \
    const char _funcptr##_method_signature[] =           \
        #_return_type" __ "#_arglist;                    \
                                                         \
    _return_type (*_funcptr) _arglist

#define MRP_IMPORT_METHOD(_name, _funcptr) {             \
        .name       = _name,                             \
        .signature  = _funcptr##_method_signature,       \
        .native_ptr = (void *)&_funcptr,                 \
    }

/** Export a method for plugins and/or scripts. */
int mrp_export_method(mrp_method_descr_t *method);

/** Remove an exported method. */
int mrp_remove_method(mrp_method_descr_t *method);

/** Import an exported method. */
int mrp_import_method(const char *name, const char *signature,
                      void **native_ptr,
                      int (**script_ptr)(mrp_plugin_t *plugin, const char *name,
                                         mrp_script_env_t *env),
                      mrp_plugin_t **plugin);

/** Release an imported method. */
int mrp_release_method(const char *name, const char *signature,
                      void **native_ptr,
                      int (**script_ptr)(mrp_plugin_t *plugin, const char *name,
                                         mrp_script_env_t *env));



#endif /* __MURPHY_CORE_METHOD_H__ */
