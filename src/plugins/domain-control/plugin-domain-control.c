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
