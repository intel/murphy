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

#ifndef __MURPHY_RESOLVER_TARGET_H__
#define __MURPHY_RESOLVER_TARGET_H__

#include "resolver-types.h"
#include "resolver.h"
#include "parser-api.h"

int create_targets(mrp_resolver_t *r, yy_res_parser_t *parser);
void destroy_targets(mrp_resolver_t *r);
target_t *create_target(mrp_resolver_t *r, const char *target,
                        const char **depends, int ndepend,
                        const char *script_type, const char *script_source);
int generate_autoupdate_target(mrp_resolver_t *r, const char *name);
int compile_target_scripts(mrp_resolver_t *r);
int prepare_target_scripts(mrp_resolver_t *r);

int update_target_by_name(mrp_resolver_t *r, const char *name);
int update_target_by_id(mrp_resolver_t *r, int id);
int schedule_target_autoupdate(mrp_resolver_t *r);

target_t *lookup_target(mrp_resolver_t *s, const char *name);
void dump_targets(mrp_resolver_t *r, FILE *fp);

/** Dump the resolver dependency graph in DOT format. */
void mrp_resolver_dump_dot_graph(mrp_resolver_t *r, FILE *fp);


#endif /* __MURPHY_RESOLVER_TARGET_H__ */
