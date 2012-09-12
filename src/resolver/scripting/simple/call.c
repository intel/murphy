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

#include <murphy/common/mm.h>
#include <murphy/core/plugin.h>       /* XXX TODO, needed for method.h */
#include <murphy/core/method.h>

#include "call.h"

function_call_t *create_call(char *function, arg_t *args, int narg)
{
    function_call_t *c;

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        c->name = mrp_strdup(function);

        if (c->name != NULL) {
            mrp_list_init(&c->hook);
            c->args = args;
            c->narg = narg;

            return c;
        }

        mrp_free(c);
    }

    return NULL;
}


void destroy_call(function_call_t *c)
{
    if (c != NULL) {
        mrp_list_delete(&c->hook);
        mrp_free(c->name);
        destroy_arguments(c->args, c->narg);
        mrp_free(c);
    }
}


int set_constant_value_arg(arg_t *arg, mrp_script_value_t *value)
{
    arg->cst.type  = ARG_CONST_VALUE;
    arg->cst.value = *value;

    if (value->type == MRP_SCRIPT_TYPE_STRING) {
        arg->cst.value.str = mrp_strdup(value->str);

        if (arg->cst.value.str != NULL)
            return TRUE;
        else
            return FALSE;
    }
    else
        return TRUE;
}


int set_context_value_arg(arg_t *arg, char *name)
{
    arg->val.type = ARG_CONTEXT_VAR;
    arg->val.name = mrp_strdup(name);

    if (arg->val.name != NULL)
        return TRUE;
    else
        return FALSE;
}


int set_context_set_arg(arg_t *arg, char *name, mrp_script_value_t *value)
{
    arg->set.type  = ARG_CONTEXT_SET;
    arg->set.name  = mrp_strdup(name);

    if (arg->set.name == NULL)
        return FALSE;

    arg->set.id    = -1;
    arg->set.value = *value;

    if (value->type == MRP_SCRIPT_TYPE_STRING) {
        arg->set.value.str = mrp_strdup(value->str);

        if (arg->set.value.str == NULL)
            return FALSE;
    }

    return TRUE;
}


void destroy_arguments(arg_t *args, int narg)
{
    arg_t *a;
    int    i;

    for (i = 0, a = args; i < narg; i++, a++) {
        if (a->type           == ARG_CONST_VALUE &&
            a->cst.value.type == MRP_SCRIPT_TYPE_STRING)
            mrp_free(a->cst.value.str);
        else if (a->type == ARG_CONTEXT_VAR)
            mrp_free(a->val.name);
    }

    mrp_free(args);
}


int link_call(function_call_t *c)
{
    mrp_plugin_t  *plugin;
    int          (*script_ptr)(mrp_plugin_t *plugin, const char *name,
                               mrp_script_env_t *env);

    if (c->script_ptr == NULL) {
        if (mrp_import_method(c->name, NULL, NULL, &script_ptr, &plugin) < 0) {
            mrp_log_error("Failed to find method '%s'.", c->name);
            return FALSE;
        }
        else {
            c->script_ptr = script_ptr;
            c->plugin     = plugin;
        }
    }

    return TRUE;
}

int execute_call(function_call_t *c, mrp_context_tbl_t *tbl)
{
    mrp_script_env_t   env;
    mrp_script_value_t args[c->narg];
    arg_t             *a;
    int                narg, n, status;

    if (MRP_UNLIKELY(c->script_ptr == NULL)) {
        if (!link_call(c))
            return -ENOENT;
    }

    mrp_push_context_frame(tbl);

    for (n = narg = 0, a = c->args; n < (int)MRP_ARRAY_SIZE(args); n++, a++) {
        switch (a->type) {
        case ARG_CONST_VALUE:
            args[narg++] = a->cst.value;
            break;
        case ARG_CONTEXT_VAR:
            if (a->val.id <= 0)
                a->val.id = mrp_get_context_id(tbl, a->val.name);
            if (mrp_get_context_value(tbl, a->val.id, args + narg) < 0) {
                status = -ENOENT;
                goto pop_frame;
            }
            narg++;
            break;
        case ARG_CONTEXT_SET:
            if (a->set.id <= 0)
                a->set.id = mrp_get_context_id(tbl, a->set.name);
            if (mrp_set_context_value(tbl, a->set.id, &a->set.value) < 0) {
                status = -errno;
                goto pop_frame;
            }
        default:
            status = -EINVAL;
            goto pop_frame;
        }
    }

    env.args = args;
    env.narg = narg;
    env.ctbl = tbl;

    status = c->script_ptr(c->plugin, c->name, &env);

 pop_frame:
    mrp_pop_context_frame(tbl);

    return status;
}


static void dump_arg(FILE *fp, arg_t *arg)
{
    mrp_script_value_t *val;
    char                vbuf[64];

    switch (arg->type) {
    case ARG_CONST_VALUE:
        val = &arg->cst.value;
        fprintf(fp, "%s", mrp_print_value(vbuf, sizeof(vbuf), val));
        break;

    case ARG_CONTEXT_VAR:
        fprintf(fp, "&%s", arg->val.name);
        break;

    case ARG_CONTEXT_SET:
        val = &arg->set.value;
        fprintf(fp, "&%s=%s", arg->set.name,
                mrp_print_value(vbuf, sizeof(vbuf), val));
        break;

    default:
        fprintf(fp, "<unknown/unhandled argument type>");
    }
}


void dump_call(FILE *fp, function_call_t *c)
{
    int   i;
    char *t;

    fprintf(fp, "    %s", c->name);

    fprintf(fp, "(");
    for (i = 0, t = ""; i < c->narg; i++, t = ", ") {
        fprintf(fp, "%s", t);
        dump_arg(fp, c->args + i);
    }
    fprintf(fp, ")\n");
}
