/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include <murphy/core/context.h>
#include <murphy/core/domain.h>

typedef struct {
    mrp_list_hook_t         hook;        /* to list of registered methods */
    char                   *name;        /* method name */
    int                     max_out;     /* max returned arguments */
    mrp_domain_invoke_cb_t  cb;          /* actual callback */
    void                   *user_data;   /* callback user data */
} method_t;


void domain_setup(mrp_context_t *ctx)
{
    mrp_list_init(&ctx->domain_methods);
}


int mrp_set_domain_invoke_handler(mrp_context_t *ctx,
                                  mrp_domain_invoke_handler_t handler,
                                  void *handler_data)
{
    if (ctx->domain_invoke != NULL)
        return FALSE;

    ctx->domain_invoke = handler;
    ctx->domain_data   = handler_data;

    return TRUE;
}


int mrp_register_domain_methods(mrp_context_t *ctx,
                                mrp_domain_method_def_t *defs, size_t ndef)
{
    mrp_domain_method_def_t *def;
    method_t                *m;
    size_t                   i;

    for (i = 0, def = defs; i < ndef; i++, def++) {
        m = mrp_allocz(sizeof(*m));

        if (m == NULL)
            return FALSE;

        mrp_list_init(&m->hook);

        m->name      = mrp_strdup(def->name);
        m->max_out   = def->max_out;
        m->cb        = def->cb;
        m->user_data = def->user_data;

        if (m->name == NULL) {
            mrp_free(m);
            return FALSE;
        }

        mrp_list_append(&ctx->domain_methods, &m->hook);
    }

    return TRUE;
}


static method_t *find_method(mrp_context_t *ctx, const char *name)
{
    mrp_list_hook_t *p, *n;
    method_t        *m;

    mrp_list_foreach(&ctx->domain_methods, p, n) {
        m = mrp_list_entry(p, typeof(*m), hook);

        if (!strcmp(m->name, name))
            return m;
    }

    return NULL;
}


int mrp_lookup_domain_method(mrp_context_t *ctx, const char *name,
                             mrp_domain_invoke_cb_t *cb, int *max_out,
                             void **user_data)
{
    method_t *m;

    m = find_method(ctx, name);

    if (m == NULL)
        return FALSE;

    *cb        = m->cb;
    *max_out   = m->max_out;
    *user_data = m->user_data;

    return TRUE;
}


int mrp_invoke_domain(mrp_context_t *ctx, const char *domain,
                      const char *method, int narg, mrp_domctl_arg_t *args,
                      mrp_domain_return_cb_t return_cb, void *user_data)
{
    mrp_domain_invoke_handler_t  handler      = ctx->domain_invoke;
    void                        *handler_data = ctx->domain_data;

    if (handler == NULL)
        return FALSE;

    return handler(handler_data, domain,
                   method, narg, args, return_cb, user_data);
}
