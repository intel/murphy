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

#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <murphy/common/list.h>
#include <murphy/common/file-utils.h>
#include <murphy/core/event.h>
#include <murphy/core/plugin.h>


#define PLUGIN_PREFIX "plugin-"

static mrp_plugin_descr_t *open_builtin(const char *name);
static mrp_plugin_descr_t *open_dynamic(const char *path, void **handle);
static mrp_plugin_t *find_plugin(mrp_context_t *ctx, char *name);
static mrp_plugin_t *find_plugin_instance(mrp_context_t *ctx,
                                          const char *instance);
static int parse_plugin_args(mrp_plugin_t *plugin,
                             mrp_plugin_arg_t *argv, int argc);

static int export_plugin_methods(mrp_plugin_t *plugin);
static int remove_plugin_methods(mrp_plugin_t *plugin);
static int import_plugin_methods(mrp_plugin_t *plugin);
static int release_plugin_methods(mrp_plugin_t *plugin);


/*
 * list of built-in in plugins
 *
 * Descriptors of built-in plugins, ie. plugins that are linked to
 * the daemon, get collected to this list during startup.
 */

static MRP_LIST_HOOK(builtin_plugins);


/*
 * plugin-related events
 */

enum {
    PLUGIN_EVENT_LOADED = 0,             /* plugin has been loaded */
    PLUGIN_EVENT_STARTED,                /* plugin has been started */
    PLUGIN_EVENT_FAILED,                 /* plugin failed to start */
    PLUGIN_EVENT_STOPPING,               /* plugin being stopped */
    PLUGIN_EVENT_STOPPED,                /* plugin has been stopped */
    PLUGIN_EVENT_UNLOADED,               /* plugin has been unloaded */
    PLUGIN_EVENT_MAX,
};


MRP_REGISTER_EVENTS(events,
                    { MRP_PLUGIN_EVENT_LOADED  , PLUGIN_EVENT_LOADED   },
                    { MRP_PLUGIN_EVENT_STARTED , PLUGIN_EVENT_STARTED  },
                    { MRP_PLUGIN_EVENT_FAILED  , PLUGIN_EVENT_FAILED   },
                    { MRP_PLUGIN_EVENT_STOPPING, PLUGIN_EVENT_STOPPING },
                    { MRP_PLUGIN_EVENT_STOPPED , PLUGIN_EVENT_STOPPED  },
                    { MRP_PLUGIN_EVENT_UNLOADED, PLUGIN_EVENT_UNLOADED });


static int emit_plugin_event(int idx, mrp_plugin_t *plugin)
{
    uint16_t name = MRP_PLUGIN_TAG_PLUGIN;
    uint16_t inst = MRP_PLUGIN_TAG_INSTANCE;

    return mrp_emit_event(events[idx].id,
                          MRP_MSG_TAG_STRING(name, plugin->descriptor->name),
                          MRP_MSG_TAG_STRING(inst, plugin->instance),
                          MRP_MSG_END);
}


int mrp_register_builtin_plugin(mrp_plugin_descr_t *descriptor)
{
    mrp_plugin_t *plugin;

    if (descriptor->name && descriptor->init && descriptor->exit) {
        if ((plugin = mrp_allocz(sizeof(*plugin))) != NULL) {
            plugin->descriptor = descriptor;
            mrp_list_append(&builtin_plugins, &plugin->hook);

            return TRUE;
        }
        else
            return FALSE;
    }
    else {
        mrp_log_error("Ignoring static plugin '%s' with an invalid or "
                      "incomplete plugin descriptor.", descriptor->path);
        return FALSE;
    }
}


int mrp_plugin_exists(mrp_context_t *ctx, const char *name)
{
    struct stat st;
    char        path[PATH_MAX];

    if (open_builtin(name))
        return TRUE;
    else {
        snprintf(path, sizeof(path), "%s/%s%s.so", ctx->plugin_dir,
                 PLUGIN_PREFIX, name);
        if (stat(path, &st) == 0)
            return TRUE;
        else
            return FALSE;
    }
}


static inline int check_plugin_version(mrp_plugin_descr_t *descr)
{
    int major, minor;

    major = MRP_VERSION_MAJOR(descr->mrp_version);
    minor = MRP_VERSION_MINOR(descr->mrp_version);

    if (major != MRP_PLUGIN_API_MAJOR || minor > MRP_PLUGIN_API_MINOR) {
        mrp_log_error("Plugin '%s' uses incompatible version (%d.%d vs. %d.%d)",
                      descr->name, major, minor,
                      MRP_PLUGIN_API_MAJOR, MRP_PLUGIN_API_MINOR);
        return FALSE;
    }

    return TRUE;
}


static inline int check_plugin_singleton(mrp_plugin_descr_t *descr)
{
    if (descr->singleton && descr->ninstance > 1) {
        mrp_log_error("Singleton plugin '%s' has already been instantiated.",
                      descr->name);
        return FALSE;
    }
    else
        return TRUE;
}


mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
                              const char *instance, mrp_plugin_arg_t *args,
                              int narg)
{
    mrp_plugin_t        *plugin;
    char                 path[PATH_MAX];
    mrp_plugin_descr_t  *dynamic, *builtin, *descr;
    void                *handle;
    mrp_console_group_t *cmds;
    char                 grpbuf[PATH_MAX], *cmdgrp;

    if (name == NULL)
        return NULL;

    if (instance == NULL)
        instance = name;

    if (find_plugin_instance(ctx, instance) != NULL) {
        mrp_log_error("Plugin '%s' has already been loaded.", instance);
        return NULL;
    }

    plugin = NULL;
    handle = NULL;
    snprintf(path, sizeof(path), "%s/%s%s.so", ctx->plugin_dir,
             PLUGIN_PREFIX, name);

    dynamic = open_dynamic(path, &handle);
    builtin = open_builtin(name);

    if (dynamic != NULL) {
        if (builtin != NULL)
            mrp_log_warning("Dynamic plugin '%s' shadows builtin plugin '%s'.",
                            path, builtin->path);
        descr = dynamic;
    }
    else {
        if (builtin == NULL) {
            mrp_log_error("Could not find plugin '%s'.", name);
            return NULL;
        }
        descr = builtin;
    }

    descr->ninstance++;

    if (!check_plugin_version(descr) || !check_plugin_singleton(descr))
        goto fail;

    if ((plugin = mrp_allocz(sizeof(*plugin))) != NULL) {
        mrp_list_init(&plugin->hook);

        plugin->instance = mrp_strdup(instance);
        plugin->path     = mrp_strdup(handle ? path : descr->path);

        if (plugin->instance == NULL || plugin->path == NULL) {
            mrp_log_error("Failed to allocate plugin '%s'.", name);
            goto fail;
        }

        if (descr->cmds != NULL) {
            plugin->cmds = cmds = mrp_allocz(sizeof(*plugin->cmds));

            if (cmds != NULL) {
                mrp_list_init(&cmds->hook);

                if (instance != name) {
                    snprintf(grpbuf, sizeof(grpbuf), "%s-%s", name, instance);
                    cmdgrp = grpbuf;
                }
                else
                    cmdgrp = (char *)name;

                cmds->name = mrp_strdup(cmdgrp);

                if (cmds->name == NULL) {
                    mrp_log_error("Failed to allocate plugin commands.");
                    goto fail;
                }

                cmds->commands = descr->cmds->commands;
                cmds->ncommand = descr->cmds->ncommand;

                if (MRP_UNLIKELY(descr->cmds->user_data != NULL))
                    cmds->user_data = descr->cmds->user_data;
                else
                    cmds->user_data = plugin;
            }
            else {
                mrp_log_error("Failed to allocate plugin commands.");
                goto fail;
            }
        }


        plugin->ctx        = ctx;
        plugin->descriptor = descr;
        plugin->handle     = handle;

        mrp_refcnt_init(&plugin->refcnt);

        if (!parse_plugin_args(plugin, args, narg))
            goto fail;

        if (plugin->cmds != NULL)
            mrp_console_add_group(plugin->ctx, plugin->cmds);

        if (!export_plugin_methods(plugin))
            goto fail;

        mrp_list_append(&ctx->plugins, &plugin->hook);

        emit_plugin_event(PLUGIN_EVENT_LOADED, plugin);

        return plugin;
    }
    else {
        mrp_log_error("Could not allocate plugin '%s'.", name);
    fail:
        mrp_unload_plugin(plugin);

        return NULL;
    }
}


static int load_plugin_cb(const char *file, mrp_dirent_type_t type, void *data)
{
    mrp_context_t *ctx = (mrp_context_t *)data;
    char           name[PATH_MAX], *start, *end;
    int            len;

    MRP_UNUSED(type);

    if ((start = strstr(file, PLUGIN_PREFIX)) != NULL) {
        start += sizeof(PLUGIN_PREFIX) - 1;
        if ((end = strstr(start, ".so")) != NULL) {
            len = end - start;
            strncpy(name, start, len);
            name[len] = '\0';
            mrp_load_plugin(ctx, name, NULL, NULL, 0);
        }
    }

    return TRUE;
}


int mrp_load_all_plugins(mrp_context_t *ctx)
{
    mrp_dirent_type_t  type;
    const char        *pattern;
    mrp_list_hook_t   *p, *n;
    mrp_plugin_t      *plugin;

    type    = MRP_DIRENT_REG;
    pattern = PLUGIN_PREFIX".*\\.so$";
    mrp_scan_dir(ctx->plugin_dir, pattern, type, load_plugin_cb, ctx);


    mrp_list_foreach(&builtin_plugins, p, n) {
        plugin = mrp_list_entry(p, typeof(*plugin), hook);

        mrp_load_plugin(ctx, plugin->descriptor->name, NULL, NULL, 0);
    }

    return TRUE;
}


int mrp_request_plugin(mrp_context_t *ctx, const char *name,
                       const char *instance)
{
    mrp_plugin_t *plugin;

    if (instance == NULL)
        instance = name;

    if ((plugin = find_plugin_instance(ctx, instance)) != NULL) {
        if (instance == name || !strcmp(plugin->descriptor->name, name))
            return TRUE;
    }

    return (mrp_load_plugin(ctx, name, instance, NULL, 0) != NULL);
}


int mrp_unload_plugin(mrp_plugin_t *plugin)
{
    mrp_plugin_arg_t *pa, *da;
    int               i;

    if (plugin != NULL) {
        if (plugin->refcnt == 0) {
            mrp_list_delete(&plugin->hook);

            pa = plugin->args;
            da = plugin->descriptor->args;
            if (pa != da) {
                for (i = 0; i < plugin->descriptor->narg; i++, pa++, da++) {
                    if (pa->type == MRP_PLUGIN_ARG_TYPE_STRING)
                        if (pa->str != da->str)
                            mrp_free(pa->str);
                }
                mrp_free(plugin->args);
            }

            plugin->descriptor->ninstance--;

            emit_plugin_event(PLUGIN_EVENT_UNLOADED, plugin);

            if (plugin->handle != NULL)
                dlclose(plugin->handle);

            if (plugin->cmds != NULL) {
                mrp_console_del_group(plugin->ctx, plugin->cmds);

                mrp_free(plugin->cmds->name);
                mrp_free(plugin->cmds);
            }

            release_plugin_methods(plugin);
            remove_plugin_methods(plugin);

            mrp_free(plugin->instance);
            mrp_free(plugin->path);
            mrp_free(plugin);

            return TRUE;
        }
        else
            return FALSE;
    }
    else
        return TRUE;
}


int mrp_start_plugins(mrp_context_t *ctx)
{
    mrp_list_hook_t *p, *n;
    mrp_plugin_t    *plugin;

    mrp_list_foreach(&ctx->plugins, p, n) {
        plugin = mrp_list_entry(p, typeof(*plugin), hook);

        if (!import_plugin_methods(plugin)) {
            if (!plugin->may_fail)
                return FALSE;
            else
                goto unload;
        }

        if (!mrp_start_plugin(plugin)) {
            mrp_log_error("Failed to start plugin %s (%s).",
                          plugin->instance, plugin->descriptor->name);

            emit_plugin_event(PLUGIN_EVENT_FAILED, plugin);

            if (!plugin->may_fail)
                return FALSE;
            else {
            unload:
                mrp_unload_plugin(plugin);
            }
        }
        else
            emit_plugin_event(PLUGIN_EVENT_STARTED, plugin);
    }

    return TRUE;
}


int mrp_start_plugin(mrp_plugin_t *plugin)
{
    if (plugin != NULL)
        return plugin->descriptor->init(plugin);
    else
        return FALSE;
}


int mrp_stop_plugin(mrp_plugin_t *plugin)
{
    if (plugin != NULL) {
        if (plugin->refcnt <= 1) {
            emit_plugin_event(PLUGIN_EVENT_STOPPING, plugin);
            plugin->descriptor->exit(plugin);
            plugin->refcnt = 0;
            emit_plugin_event(PLUGIN_EVENT_STOPPED, plugin);

            return TRUE;
        }
        else
            return FALSE;
    }
    else
        return TRUE;
}


static mrp_plugin_t *find_plugin_instance(mrp_context_t *ctx,
                                          const char *instance)
{
    mrp_list_hook_t *p, *n;
    mrp_plugin_t    *plg;

    mrp_list_foreach(&ctx->plugins, p, n) {
        plg = mrp_list_entry(p, typeof(*plg), hook);

        if (!strcmp(plg->instance, instance))
            return plg;
    }

    return NULL;
}


static mrp_plugin_t *find_plugin(mrp_context_t *ctx, char *name)
{
    mrp_list_hook_t *p, *n;
    mrp_plugin_t    *plg;

    mrp_list_foreach(&ctx->plugins, p, n) {
        plg = mrp_list_entry(p, typeof(*plg), hook);

        if (!strcmp(plg->descriptor->name, name))
            return plg;
    }

    return NULL;
}


static mrp_plugin_descr_t *open_dynamic(const char *path, void **handle)
{
    mrp_plugin_descr_t *(*describe)(void);
    mrp_plugin_descr_t   *d;
    void                 *h;

    if ((h = dlopen(path, RTLD_LAZY | RTLD_LOCAL)) != NULL) {
        if ((describe = dlsym(h, "mrp_get_plugin_descriptor")) != NULL) {
            if ((d = describe()) != NULL) {
                if (d->init != NULL && d->exit != NULL && d->name != NULL) {
                    if (!d->core)
                        *handle = h;
                    else
                        *handle = dlopen(path,
                                         RTLD_LAZY|RTLD_GLOBAL|RTLD_NOLOAD);

                    return d;
                }
                else
                    mrp_log_error("Ignoring plugin '%s' with invalid "
                                  "plugin descriptor.", path);
            }
            else
                mrp_log_error("Plugin '%s' provided NULL descriptor.", path);
        }
        else
            mrp_log_error("Plugin '%s' does not provide a descriptor.", path);
    }
    else {
        if (access(path, F_OK) == 0) {
            char *err = dlerror();

            mrp_log_error("Failed to dlopen plugin '%s' (%s).", path,
                          err ? err : "unknown error");
        }
    }

    if (h != NULL)
        dlclose(h);

    *handle = NULL;
    return NULL;
}


static mrp_plugin_descr_t *open_builtin(const char *name)
{
    mrp_list_hook_t *p, *n;
    mrp_plugin_t    *plugin;

    mrp_list_foreach(&builtin_plugins, p, n) {
        plugin = mrp_list_entry(p, typeof(*plugin), hook);

        if (!strcmp(plugin->descriptor->name, name))
            return plugin->descriptor;
    }

    return NULL;
}


static int parse_plugin_arg(mrp_plugin_arg_t *arg, mrp_plugin_arg_t *parg)
{
    char *end;

    switch (parg->type) {
    case MRP_PLUGIN_ARG_TYPE_STRING:
        if (arg->str != NULL)
            parg->str = arg->str;
        return TRUE;

    case MRP_PLUGIN_ARG_TYPE_BOOL:
        if (arg->str != NULL) {
            if (!strcasecmp(arg->str, "TRUE"))
                parg->bln = TRUE;
            else if (!strcasecmp(arg->str, "FALSE"))
                parg->bln = FALSE;
            else
                return FALSE;
        }
        else
            parg->bln = TRUE;
        return TRUE;

    case MRP_PLUGIN_ARG_TYPE_UINT32:
        parg->u32 = (uint32_t)strtoul(arg->str, &end, 0);
        if (end && !*end)
            return TRUE;
        else
            return FALSE;

    case MRP_PLUGIN_ARG_TYPE_INT32:
        parg->i32 = (int32_t)strtol(arg->str, &end, 0);
        if (end && !*end)
            return TRUE;
        else
            return FALSE;

    case MRP_PLUGIN_ARG_TYPE_DOUBLE:
        parg->dbl = strtod(arg->str, &end);
        if (end && !*end)
            return TRUE;
        else
            return FALSE;

    default:
        return FALSE;
    }
}


static int parse_plugin_args(mrp_plugin_t *plugin,
                             mrp_plugin_arg_t *argv, int argc)
{
    mrp_plugin_descr_t *descr;
    mrp_plugin_arg_t   *valid, *args, *pa, *a;
    int                 i, j, cnt;

    if (argv == NULL) {
        plugin->args = plugin->descriptor->args;
        return TRUE;
    }

    descr = plugin->descriptor;
    valid = descr->args;

    if (valid == NULL && argv != NULL) {
        mrp_log_error("Plugin '%s' (%s) does not take any arguments.",
                      plugin->instance, descr->name);
        return FALSE;
    }

    if ((args = mrp_allocz_array(typeof(*args), descr->narg)) == NULL) {
        mrp_log_error("Failed to allocate arguments for plugin '%s'.",
                      plugin->instance);
        return FALSE;
    }

    memcpy(args, descr->args, descr->narg * sizeof(*args));
    plugin->args = args;

    j = 0;
    for (i = 0, a = argv; i < argc; i++, a++) {
        for (cnt = 0, pa = NULL; pa == NULL && cnt < descr->narg; cnt++) {
            if (!strcmp(a->key, args[j].key))
                pa = args + j;
            if (++j >= descr->narg)
                j = 0;
        }

        if (pa != NULL) {
            if (!parse_plugin_arg(a, pa)) {
                mrp_log_error("Invalid argument '%s' for plugin '%s'.",
                              a->key, plugin->instance);
                return FALSE;
            }
        }
        else {
            mrp_log_error("Plugin '%s' (%s) does not support argument '%s'",
                          plugin->instance, descr->name, a->key);
            return FALSE;
        }
    }

    return TRUE;
}


static int export_plugin_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t *methods = plugin->descriptor->exports, *m;
    int                 nmethod = plugin->descriptor->nexport, i;

    for (i = 0, m = methods; i < nmethod; i++, m++) {
        m->plugin = plugin;
        if (mrp_export_method(m) != 0) {
            mrp_log_error("Failed to export method %s from plugin %s.",
                          m->name, plugin->instance);
            return FALSE;
        }
    }

    return TRUE;
}


static int remove_plugin_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t *methods = plugin->descriptor->exports, *m;
    int                 nmethod = plugin->descriptor->nexport, i;
    int                 success = TRUE;

    for (i = 0, m = methods; i < nmethod; i++, m++) {
        m->plugin = plugin;
        if (mrp_remove_method(m) != 0) {
            mrp_log_error("Failed to remove exported method %s of plugin %s.",
                          m->name, plugin->instance);
            success = FALSE;
        }
    }

    return success;
}


static int import_plugin_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t *methods = plugin->descriptor->imports, *m;
    int                 nmethod = plugin->descriptor->nimport, i;

    for (i = 0, m = methods; i < nmethod; i++, m++) {
        if (mrp_import_method(m->name, m->signature,
                              (void **)m->native_ptr, NULL, NULL) != 0) {
            mrp_log_error("Failed to import method %s (%s) for plugin %s.",
                          m->name, m->signature, plugin->instance);
            return FALSE;
        }
    }

    return TRUE;
}


static int release_plugin_methods(mrp_plugin_t *plugin)
{
    mrp_method_descr_t *methods = plugin->descriptor->imports, *m;
    int                 nmethod = plugin->descriptor->nimport, i;
    int                 success = TRUE;

    for (i = 0, m = methods; i < nmethod; i++, m++) {
        if (mrp_release_method(m->name, m->signature,
                               (void **)m->native_ptr, NULL) != 0) {
            mrp_log_error("Failed to release imported method %s of plugin %s.",
                          m->name, plugin->instance);
            success = FALSE;
        }
    }

    return success;
}
