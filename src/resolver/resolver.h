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

#ifndef __MURPHY_RESOLVER_H__
#define __MURPHY_RESOLVER_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct mrp_resolver_s mrp_resolver_t;

#include <murphy/common/macros.h>
#include <murphy/core/context.h>
#include <murphy/core/scripting.h>

MRP_CDECL_BEGIN


/*
 * tags and names of resolver-related events we emit
 */

#define MRP_RESOLVER_BUS           "resolver-bus"
#define MRP_RESOLVER_EVENT_STARTED "resolver-update-start"
#define MRP_RESOLVER_EVENT_FAILED  "resolver-update-failed"
#define MRP_RESOLVER_EVENT_DONE    "resolver-update-done"

#define MRP_RESOLVER_TAG_TARGET ((uint16_t)1)
#define MRP_RESOLVER_TAG_LEVEL  ((uint16_t)2)

/** Just create a resolver context without parsing any input. */
mrp_resolver_t *mrp_resolver_create(mrp_context_t *ctx);

/** Parse the given resolver input file into a resolver context. */
mrp_resolver_t *mrp_resolver_parse(mrp_resolver_t *r, mrp_context_t *ctx,
                                   const char *path);

/** Add a new target with the given parameters to the resolver context. */
int mrp_resolver_add_target(mrp_resolver_t *r, const char *target,
                            const char **depend, int ndepend,
                            const char *script_type,
                            const char *script_source);

/** Add a precompiled target to the resolver context. */
int mrp_resolver_add_prepared_target(mrp_resolver_t *r, const char *target,
                                     const char **depend, int ndepend,
                                     mrp_interpreter_t *interpreter,
                                     void *compiled_data, void *target_data);

/** Add an alias for the given target. */
int mrp_resolver_add_alias(mrp_resolver_t *r, const char *target,
                           const char *alias);

/** Enable autoupdate, generate autoupdate target if needed. */
int mrp_resolver_enable_autoupdate(mrp_resolver_t *r, const char *name);

/** Destroy the given resolver context, freeing all associated resources. */
void mrp_resolver_destroy(mrp_resolver_t *r);

/** Prepare the targets for resolution (link scriptlets, etc.). */
int mrp_resolver_prepare(mrp_resolver_t *r);

/** Update the given target. The NULL-terminated variable argument list
    after the target name sepcifies the resolver context variables to
    set during the update. Use a single NULL to omit variables. */
int mrp_resolver_update_targetl(mrp_resolver_t *r,
                                const char *target, ...) MRP_NULLTERM;

#define mrp_resolver_update_target mrp_resolver_update_targetl

/** Update the given target. The variable name and type/value arrays
    specify the resolver context variables to set during the update. */
int mrp_resolver_update_targetv(mrp_resolver_t *r, const char *target,
                                const char **variables,
                                mrp_script_value_t *values,
                                int nvariable);

/** Declare a context variable with a given type. */
int mrp_resolver_declare_variable(mrp_resolver_t *r, const char *name,
                                  mrp_script_type_t type);


/** Get the value of a context variable by id. */
int mrp_resolver_get_value(mrp_resolver_t *r, int id, mrp_script_value_t *v);
#define mrp_resolver_get_value_by_id mrp_resolver_get_value

/** Get the value of a context variable by name. */
int mrp_resolver_get_value_by_name(mrp_resolver_t *r, const char *name,
                                   mrp_script_value_t *v);

/** Print the given value to the given buffer. */
char *mrp_print_value(char *buf, size_t size, mrp_script_value_t *value);

/** Produce a debug dump of all targets. */
void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp);

/** Produce a debug dump of all tracked facts. */
void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp);

/** Register a script interpreter. */
int mrp_resolver_register_interpreter(mrp_interpreter_t *i);

/** Unregister a script interpreter. */
int mrp_resolver_unregister_interpreter(const char *name);

MRP_CDECL_END

#endif /* __MURPHY_RESOLVER_H__ */
