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
    static const char _func##_method_signature[] =        \
        #_return_type" __ "#_arglist;                     \
                                                          \
    static _return_type _func _arglist

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
    static const char _funcptr##_method_signature[] =    \
        #_return_type" __ "#_arglist;                    \
                                                         \
    static _return_type (*_funcptr) _arglist

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
