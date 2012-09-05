#include <murphy/common/log.h>
#include <murphy/core/method.h>

#include "builtins.h"

static int builtin_echo(mrp_plugin_t *plugin, const char *name,
                        mrp_script_env_t *env)
{
    mrp_script_value_t *arg;
    int                 i;
    char                buf[512], *t;

    MRP_UNUSED(plugin);
    MRP_UNUSED(name);

    for (i = 0, arg = env->args, t = ""; i < env->narg; i++, arg++, t=" ")
        printf("%s%s", t, mrp_print_value(buf, sizeof(buf), arg));

    printf("\n");

    return TRUE;
}


int export_builtins(void)
{
    mrp_method_descr_t methods[] = {
        {
            .name       = "echo",
            .signature  = NULL  ,
            .native_ptr = NULL  ,
            .script_ptr = builtin_echo,
            .plugin     = NULL
        },
        { NULL, NULL, NULL, NULL, NULL }
    }, *m;

    for (m = methods; m->name != NULL; m++) {
        if (mrp_export_method(m) < 0) {
            mrp_log_error("Failed to export function '%s'.", m->name);
            return FALSE;
        }
    }

    return TRUE;
}
