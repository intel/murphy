#include <murphy/common/macros.h>

#include <murphy/core/plugin.h>
#include <murphy/core/console.h>

#include "domain-control-types.h"
#include "domain-control.h"
#include "client.h"


static int plugin_init(mrp_plugin_t *plugin)
{
    const char *address = MRP_DEFAULT_DOMCTL_ADDRESS;

    plugin->data = create_domain_control(plugin->ctx, address);

    return (plugin->data != NULL);
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    pdp_t *pdp = (pdp_t *)plugin->data;

    destroy_domain_control(pdp);
}


static void cmd_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    printf("domctrl:%s() called...\n", __FUNCTION__);
}


#define PLUGIN_DESCRIPTION "Murphy domain control plugin."
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)
#define PLUGIN_HELP        "TODO..."
#define PLUGIN_AUTHORS     "Krisztian Litkey <krisztian.litkey@intel.com>"

MRP_CONSOLE_GROUP(plugin_commands, "domain-control", NULL, NULL, {
        MRP_TOKENIZED_CMD("cmd", cmd_cb, TRUE,
                          "cmd [args]", "a command", "A command..."),
});

MURPHY_REGISTER_PLUGIN("domain-control",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION,
                       PLUGIN_AUTHORS, PLUGIN_HELP, MRP_SINGLETON,
                       plugin_init, plugin_exit,
                       NULL, 0, /* plugin argument table */
                       NULL, 0, /* exported methods */
                       NULL, 0, /* imported methods */
                       &plugin_commands);
