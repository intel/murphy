#include <murphy/common/macros.h>

#include <murphy/core/plugin.h>
#include <murphy/core/console.h>

#include "decision-types.h"
#include "decision.h"
#include "client.h"


static int plugin_init(mrp_plugin_t *plugin)
{
    plugin->data = create_decision(plugin->ctx, MRP_DEFAULT_PEP_ADDRESS);

    return (plugin->data != NULL);
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    pdp_t *pdp = (pdp_t *)plugin->data;

    destroy_decision(pdp);
}


static void cmd_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_console_printf(c, "decision:%s() called...\n", __FUNCTION__);
}


#define PLUGIN_DESCRIPTION "Murphy decision making plugin prototype."
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)
#define PLUGIN_HELP        "TODO..."
#define PLUGIN_AUTHORS     "Aku Ankka <aku.ankka@ankkalinna.org>"

MRP_CONSOLE_GROUP(plugin_commands, "decision", NULL, NULL, {
        MRP_TOKENIZED_CMD("cmd", cmd_cb, TRUE,
                          "cmd [args]", "a command", "A command..."),
});

MURPHY_REGISTER_PLUGIN("decision-proto",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION,
                       PLUGIN_AUTHORS, PLUGIN_HELP, MRP_SINGLETON,
                       plugin_init, plugin_exit,
                       NULL, 0, /* plugin argument table */
                       NULL, 0, /* exported methods */
                       NULL, 0, /* imported methods */
                       &plugin_commands);
