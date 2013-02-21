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

#define DEFAULT_EXTADDR MRP_DEFAULT_DOMCTL_ADDRESS
#define NO_ADDR         NULL

#ifdef MURPHY_DATADIR
#    define DEFAULT_HTTPDIR MURPHY_DATADIR"/domain-control"
#else
#    define DEFAULT_HTTPDIR "/usr/share/murphy/domain-control"
#endif

enum {
    ARG_EXTADDR,                         /* external transport address */
    ARG_INTADDR,                         /* internal transport address */
    ARG_WRTADDR,                         /* WRT transport address */
    ARG_HTTPDIR                          /* content directory for HTTP */
};


static int plugin_init(mrp_plugin_t *plugin)
{
    const char *extaddr = plugin->args[ARG_EXTADDR].str;
    const char *intaddr = plugin->args[ARG_INTADDR].str;
    const char *wrtaddr = plugin->args[ARG_WRTADDR].str;
    const char *httpdir = plugin->args[ARG_HTTPDIR].str;

    plugin->data = create_domain_control(plugin->ctx,
                                         extaddr && *extaddr ? extaddr : NULL,
                                         intaddr && *intaddr ? intaddr : NULL,
                                         wrtaddr && *wrtaddr ? wrtaddr : NULL,
                                         httpdir);

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

    printf("domctl:%s() called...\n", __FUNCTION__);
}


#define DOMCTL_DESCRIPTION "Murphy domain-control plugin."
#define DOMCTL_HELP                                                         \
    "The domain-control plugin provides a control interface for Murphy\n"    \
    "domain controllers. A domain controller is an entity capable of\n"      \
    "enforcing domain-specific policies in a certain resource domain, eg.\n" \
    "audio, video, CPU-scheduling, etc. The domain-control plugin allows\n"  \
    "such entities to export and import domain-specific data to and from\n"  \
    "Murphy. Domain controllers typically import either ready decisions\n"   \
    "for their domain or data necessary for local decision making in\n"      \
    "the controller itself. The controllers typically export also some\n"    \
    "domain-specific data to Murphy which can then be used for decision\n"   \
    "making in other domains other domains.\n"

#define DOMCTL_VERSION MRP_VERSION_INT(0, 0, 2)
#define DOMCTL_AUTHORS "Krisztian Litkey <krisztian.litkey@intel.com>"

MRP_CONSOLE_GROUP(domctl_commands, "domain-control", NULL, NULL, {
        MRP_TOKENIZED_CMD("cmd", cmd_cb, TRUE,
                          "cmd [args]", "a command", "A command..."),
});

static mrp_plugin_arg_t domctl_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_EXTADDR, STRING, "external_address", DEFAULT_EXTADDR),
    MRP_PLUGIN_ARGIDX(ARG_INTADDR, STRING, "internal_address", NO_ADDR        ),
    MRP_PLUGIN_ARGIDX(ARG_WRTADDR, STRING, "wrt_address"     , NO_ADDR        ),
    MRP_PLUGIN_ARGIDX(ARG_HTTPDIR, STRING, "httpdir", DEFAULT_HTTPDIR)
};

MURPHY_REGISTER_PLUGIN("domain-control",
                       DOMCTL_VERSION, DOMCTL_DESCRIPTION,
                       DOMCTL_AUTHORS, DOMCTL_HELP, MRP_MULTIPLE,
                       plugin_init, plugin_exit,
                       domctl_args, MRP_ARRAY_SIZE(domctl_args),
                       NULL, 0, NULL, 0, &domctl_commands);
