/*
 * Copyright (c) 2012, Intel Corporation
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

#ifndef __MURPHY_RESOURCE_API_RSET_H__
#define __MURPHY_RESOURCE_API_RSET_H__

#include <errno.h>

#include <murphy/resource/protocol.h>
#include "resource-api.h"
#include "resource-private.h"

#define RESOURCE_MAX   32

void print_resource(mrp_res_resource_t *res);

void print_resource_set(mrp_res_resource_set_t *rset);


void increase_ref(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset);

void decrease_ref(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset);


void free_resource_set(mrp_res_resource_set_t *rset);

void delete_resource_set(mrp_res_resource_set_t *rs);

mrp_res_resource_set_t *resource_set_copy(
        const mrp_res_resource_set_t *original);

mrp_res_resource_t *get_resource_by_name(mrp_res_resource_set_t *rset,
        const char *name);

#endif
