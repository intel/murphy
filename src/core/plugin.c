#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <murphy/common/list.h>
#include <murphy/core/plugin.h>

#define PLUGIN_PREFIX "plugin-"

static mrp_plugin_descr_t *open_builtin(const char *name);
static mrp_plugin_descr_t *open_dynamic(const char *path, void **handle);
static mrp_plugin_t *find_plugin(mrp_context_t *ctx, char *name);
static mrp_plugin_t *find_plugin_instance(mrp_context_t *ctx,
					  const char *instance);
static int parse_plugin_args(mrp_plugin_t *plugin,
			     mrp_plugin_arg_t *argv, int argc);


static MRP_LIST_HOOK(builtin_plugins);    /* list of built-in plugins */



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
	snprintf(path, sizeof(path), "%s/plugin-%s.so", ctx->plugin_dir, name);
	if (stat(path, &st) == 0)
	    return TRUE;
	else
	    return FALSE;
    }
}


mrp_plugin_t *mrp_load_plugin(mrp_context_t *ctx, const char *name,
			      const char *instance, mrp_plugin_arg_t *args,
			      int narg)
{
    mrp_plugin_t       *plugin;
    char                path[PATH_MAX];
    mrp_plugin_descr_t *dynamic, *builtin;
    void               *handle;
    
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
    snprintf(path, sizeof(path), "%s/plugin-%s.so", ctx->plugin_dir, name);
    
    dynamic = open_dynamic(path, &handle);
    builtin = open_builtin(name);
    
    if (dynamic != NULL) {
	if (builtin != NULL)
    	    mrp_log_warning("Dynamic plugin '%s' shadows builtin plugin '%s'.",
			    path, builtin->path);
    }
    else {
	if (builtin == NULL) {
	    mrp_log_error("Could not find plugin '%s'.", name);
	    return NULL;
	}
    }
	
    if ((plugin = mrp_allocz(sizeof(*plugin))) != NULL) {
	mrp_list_init(&plugin->hook);
	
	plugin->instance = mrp_strdup(instance);
	plugin->path     = mrp_strdup(handle ? path : builtin->path);

	if (plugin->instance == NULL || plugin->path == NULL) {
	    mrp_log_error("Failed to allocate plugin '%s'.", name);
	    goto fail;
	}
	
	plugin->ctx        = ctx;
	plugin->descriptor = handle ? dynamic : builtin;
	plugin->refcnt     = 1;
	plugin->handle     = handle;
	
	if (!parse_plugin_args(plugin, args, narg))
	    goto fail;
	
	mrp_list_append(&ctx->plugins, &plugin->hook);

	return plugin;
    }
    else {
	mrp_log_error("Could not allocate plugin '%s'.", name);
    fail:
	mrp_unload_plugin(plugin);
	
	return NULL;
    }
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

	    if (plugin->handle != NULL)
		dlclose(plugin->handle);
	
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
	
	if (!mrp_start_plugin(plugin)) {
	    mrp_log_error("Failed to start plugin %s (%s).",
			  plugin->instance, plugin->descriptor->name);
	    return FALSE;
	}
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
	    plugin->descriptor->exit(plugin);
	    plugin->refcnt = 0;
	    
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
