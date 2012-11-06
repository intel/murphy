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

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>
#include <murphy/core/plugin.h>
#include <murphy/core/method.h>


typedef struct {
    char            *name;               /* name of methods in this chain */
    mrp_list_hook_t  methods;            /* list of exported methods */
} method_list_t;


typedef struct {
    __MRP_METHOD_FIELDS();               /* method fields */
    mrp_list_hook_t hook;                /* to method table */
} method_t;


static void purge_method_list(void *key, void *object);


static mrp_htbl_t *methods = NULL;       /* hash table of method lists */


static int create_method_table(void)
{
    mrp_htbl_config_t hcfg;

    mrp_clear(&hcfg);
    hcfg.comp = mrp_string_comp;
    hcfg.hash = mrp_string_hash;
    hcfg.free = purge_method_list;

    methods = mrp_htbl_create(&hcfg);

    if (methods != NULL)
        return 0;
    else
        return -1;
}


MRP_EXIT static void destroy_method_table(void)
{
    mrp_htbl_destroy(methods, TRUE);
    methods = NULL;
}


static void free_method(method_t *m)
{
    if (m != NULL) {
        mrp_free(m->name);
        mrp_free(m->signature);
        mrp_free(m);
    }
}


static method_t *alloc_method(mrp_method_descr_t *method)
{
    method_t *m;

    m = mrp_allocz(sizeof(*m));

    if (m != NULL) {
        mrp_list_init(&m->hook);
        m->name      = mrp_strdup(method->name);
        m->signature = mrp_strdup(method->signature);

        if (m->name != NULL &&
            (m->signature != NULL || method->signature == NULL)) {
            m->native_ptr = method->native_ptr;
            m->script_ptr = method->script_ptr;
            m->plugin     = method->plugin;
        }
        else {
            free_method(m);
            m = NULL;
        }
    }

    return m;
}


static method_list_t *create_method_list(const char *name)
{
    method_list_t *l;

    if (MRP_UNLIKELY(methods == NULL)) {
        if (create_method_table() < 0)
            return NULL;
    }

    l = mrp_allocz(sizeof(*l));

    if (l != NULL) {
        mrp_list_init(&l->methods);
        l->name = mrp_strdup(name);

        if (l->name != NULL) {
            if (mrp_htbl_insert(methods, (void *)l->name, (void *)l))
                return l;
        }

        mrp_free(l->name);
        mrp_free(l);
    }

    return NULL;
}


static void free_method_list(method_list_t *l)
{
    mrp_list_hook_t *p, *n;
    method_t        *m;

    if (l != NULL) {
        mrp_list_foreach(&l->methods, p, n) {
            m = mrp_list_entry(p, typeof(*m), hook);

            mrp_list_delete(&m->hook);
            free_method(m);
        }

        mrp_free(l->name);
        mrp_free(l);
    }
}


static void purge_method_list(void *key, void *object)
{
    MRP_UNUSED(key);

    free_method_list(object);
}


static method_list_t *lookup_method_list(const char *name)
{
    method_list_t *l;

    if (methods != NULL)
        l = mrp_htbl_lookup(methods, (void *)name);
    else
        l = NULL;

    return l;
}


static inline int check_signatures(const char *sig1, const char *sig2)
{
    static int warned = FALSE;

    MRP_UNUSED(sig1);
    MRP_UNUSED(sig2);

    if (!warned) {
        mrp_log_warning("XXX TODO: implement signature checking (%s@%s:%d)",
                        __FUNCTION__, __FILE__, __LINE__);
        warned = TRUE;
    }

    return TRUE;
}


static method_t *lookup_method(const char *name, const char *signature,
                               void *native_ptr,
                               int (*script_ptr)(mrp_plugin_t *plugin,
                                                 const char *name,
                                                 mrp_script_env_t *env),
                               mrp_plugin_t *plugin)
{
    method_list_t   *l;
    method_t        *m;
    mrp_list_hook_t *p, *n;

    l = lookup_method_list(name);

    if (l != NULL) {
        mrp_list_foreach(&l->methods, p, n) {
            m = mrp_list_entry(p, typeof(*m), hook);

            if (((m->signature == NULL && signature == NULL) ||
                 (m->signature != NULL && signature != NULL &&
                  !strcmp(m->signature, signature))) &&
                m->native_ptr == native_ptr && m->script_ptr == script_ptr &&
                m->plugin == plugin)
                return m;
        }
    }

    return NULL;
}


method_t *find_method(const char *name, const char *signature)
{
    method_list_t   *l;
    method_t        *m;
    mrp_list_hook_t *p, *n;
    const char      *base;
    int              plen;

    base = strrchr(name, '.');
    if (base != NULL) {
        plen = base - name;
        base = base + 1;
    }
    else {
        base = name;
        plen = 0;
    }

    l = lookup_method_list(base);

    if (l != NULL) {
        mrp_list_foreach(&l->methods, p, n) {
            m = mrp_list_entry(p, typeof(*m), hook);

            if (signature != NULL && m->signature != NULL)
                if (!check_signatures(signature, m->signature))
                    continue;

            if (plen != 0) {
                if (m->plugin == NULL)
                    continue;
                if (strncmp(name, m->plugin->instance, plen) ||
                    m->plugin->instance[plen] != '\0')
                    continue;
            }

            return m;
        }
    }

    errno = ENOENT;
    return NULL;
}


static int export_method(method_t *method)
{
    method_list_t *l;

    l = lookup_method_list(method->name);

    if (l == NULL) {
        l = create_method_list(method->name);

        if (l == NULL)
            return -1;
    }

    mrp_ref_plugin(method->plugin);
    mrp_list_append(&l->methods, &method->hook);

    return 0;
}


static int remove_method(const char *name, const char *signature,
                         void *native_ptr,
                         int (*script_ptr)(mrp_plugin_t *plugin,
                                           const char *name,
                                           mrp_script_env_t *env),
                         mrp_plugin_t *plugin)
{
    method_t *m;

    m = lookup_method(name, signature, native_ptr, script_ptr, plugin);

    if (m != NULL) {
        mrp_list_delete(&m->hook);
        mrp_unref_plugin(m->plugin);
        free_method(m);

        return 0;
    }
    else {
        errno = ENOENT;
        return -1;
    }
}


int mrp_export_method(mrp_method_descr_t *method)
{
    method_t *m;

    if (lookup_method(method->name, method->signature,
                      method->native_ptr, method->script_ptr,
                      method->plugin) != NULL)
        errno = EEXIST;
    else {
        m = alloc_method(method);

        if (m != NULL) {
            if (export_method(m) == 0) {
                mrp_log_info("exported method %s (%s) %s%s.",
                             method->name,
                             method->signature ? method->signature : "-",
                             method->plugin ? "from plugin " : "",
                             method->plugin ? method->plugin->instance : "");
                return 0;
            }
            else
                free_method(m);
        }
    }

    mrp_log_error("Failed to export method %s (%s) %s%s.",
                  method->name, method->signature,
                  method->plugin ? "from plugin " : "",
                  method->plugin ? method->plugin->instance : "");

    return -1;
}


int mrp_remove_method(mrp_method_descr_t *method)
{
    return remove_method(method->name, method->signature,
                         method->native_ptr, method->script_ptr,
                         method->plugin);
}


int mrp_import_method(const char *name, const char *signature,
                      void **native_ptr,
                      int (**script_ptr)(mrp_plugin_t *plugin, const char *name,
                                         mrp_script_env_t *env),
                      mrp_plugin_t **plugin)
{
    method_t *m;

    if ((script_ptr != NULL && plugin == NULL) ||
        (script_ptr == NULL && plugin != NULL)) {
        errno = EINVAL;
        return -1;
    }

    m = find_method(name, signature);

    if (m == NULL) {
        errno = ENOENT;
        return -1;
    }

    if ((native_ptr != NULL && m->native_ptr == NULL) ||
        (script_ptr != NULL && m->script_ptr == NULL)) {
        errno = EINVAL;
        return -1;
    }

    mrp_ref_plugin(m->plugin);

    if (native_ptr != NULL)
        *native_ptr = m->native_ptr;
    if (script_ptr != NULL) {
        *script_ptr = m->script_ptr;
        *plugin     = m->plugin;
    }

    return 0;
}


int mrp_release_method(const char *name, const char *signature,
                       void **native_ptr,
                       int (**script_ptr)(mrp_plugin_t *plugin,
                                          const char *name,
                                          mrp_script_env_t *env))
{
    method_t *m;

    m = find_method(name, signature);

    if (m == NULL) {
        errno = ENOENT;
        return -1;
    }

    if ((native_ptr != NULL && (*native_ptr != m->native_ptr)) ||
        (script_ptr != NULL && (*script_ptr != m->script_ptr))) {
        errno = EINVAL;
        return -1;
    }

    mrp_unref_plugin(m->plugin);

    if (native_ptr != NULL)
        *native_ptr = NULL;
    if (script_ptr != NULL)
        *script_ptr = NULL;

    return 0;
}
