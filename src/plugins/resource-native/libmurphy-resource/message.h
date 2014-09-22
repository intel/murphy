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

#ifndef __MURPHY_RESOURCE_API_MESSAGE_H__
#define __MURPHY_RESOURCE_API_MESSAGE_H__

#include <murphy/resource/protocol.h>

#include "resource-private.h"
#include "string_array.h"

/* parsing of the message */

bool fetch_resource_set_state(mrp_msg_t *msg, void **pcursor,
                                     mrp_resproto_state_t *pstate);

bool fetch_resource_set_mask(mrp_msg_t *msg, void **pcursor,
                                    int mask_type, uint32_t *pmask);

bool fetch_resource_set_id(mrp_msg_t *msg, void **pcursor, uint32_t *pid);

bool fetch_mrp_str_array(mrp_msg_t *msg, void **pcursor,
                   uint16_t expected_tag, mrp_res_string_array_t **parr);

bool fetch_seqno(mrp_msg_t *msg, void **pcursor, uint32_t *pseqno);

bool fetch_request(mrp_msg_t *msg, void **pcursor, uint16_t *preqtype);

bool fetch_status(mrp_msg_t *msg, void **pcursor, int *pstatus);

bool fetch_attribute_array(mrp_msg_t *msg, void **pcursor,
                                 size_t dim, mrp_res_attribute_t *arr,
                                 int *n_arr);

bool fetch_resource_name(mrp_msg_t *msg, void **pcursor,
                                const char **pname);

/* handling of the message responses */

mrp_res_resource_set_t *resource_query_response(mrp_res_context_t *cx,
        mrp_msg_t *msg, void **pcursor);

mrp_res_string_array_t *class_query_response(mrp_msg_t *msg, void **pcursor);

bool create_resource_set_response(mrp_msg_t *msg,
        mrp_res_resource_set_t *rset, void **pcursor);

mrp_res_resource_set_t *acquire_resource_set_response(mrp_msg_t *msg,
            mrp_res_context_t *cx, void **pcursor);

/* requests to the server */

int acquire_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset);

int release_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset);

int create_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset);

int get_application_classes_request(mrp_res_context_t *cx);

int get_available_resources_request(mrp_res_context_t *cx);

#endif
