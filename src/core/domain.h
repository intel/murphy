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

#ifndef __MURPHY_CORE_DOMAIN__
#define __MURPHY_CORE_DOMAIN__

#include <murphy/common/macros.h>
#include <murphy/common/msg.h>

#include <murphy/core/context.h>
#include <murphy/core/domain-types.h>

MRP_CDECL_BEGIN

/* Type for a proxied invocation handler. */
typedef int (*mrp_domain_invoke_cb_t)(uint32_t narg, mrp_domctl_arg_t *args,
                                      uint32_t *nout, mrp_domctl_arg_t *outs,
                                      void *user_data);

/* Type for a proxied invocation return/reply handler. */
typedef void (*mrp_domain_return_cb_t)(int error, int retval, int narg,
                                       mrp_domctl_arg_t *args, void *user_data);

typedef struct {
    char                   *name;        /* method name */
    int                     max_out;     /* max. number of return arguments */
    mrp_domain_invoke_cb_t  cb;          /* handler callback */
    void                   *user_data;   /* opaque callback user data */
} mrp_domain_method_def_t;



/* Type for handling proxied invocation to domain controllers. */
typedef int (*mrp_domain_invoke_handler_t)(void *handler_data, const char *id,
                                           const char *method, int narg,
                                           mrp_domctl_arg_t *args,
                                           mrp_domain_return_cb_t return_cb,
                                           void *user_data);

/* Initialize domain-specific context parts. */
void domain_setup(mrp_context_t *ctx);

/* Register a domain method. */
int mrp_register_domain_methods(mrp_context_t *ctx,
                                mrp_domain_method_def_t *defs, size_t ndef);

/* Find a registered domain method. */
int mrp_lookup_domain_method(mrp_context_t *ctx, const char *method,
                             mrp_domain_invoke_cb_t *cb, int *max_out,
                             void **user_data);

/* Invoke the named method of the specified domain. */
int mrp_invoke_domain(mrp_context_t *ctx, const char *domain, const char *method,
                      int narg, mrp_domctl_arg_t *args,
                      mrp_domain_return_cb_t return_cb, void *user_data);

/* Set the domain invoke handler. */
int mrp_set_domain_invoke_handler(mrp_context_t *ctx,
                                  mrp_domain_invoke_handler_t handler,
                                  void *handler_data);

MRP_CDECL_END

#endif /* __MURPHY_CORE_DOMAIN__ */
