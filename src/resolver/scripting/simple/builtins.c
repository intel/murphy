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
