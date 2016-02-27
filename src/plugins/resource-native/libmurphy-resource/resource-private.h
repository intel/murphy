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

#ifndef __MURPHY_RESOURCE_API_PRIVATE_H__
#define __MURPHY_RESOURCE_API_PRIVATE_H__

#include <stdarg.h>

#include <murphy/common/log.h>

#include "resource-api.h"

MRP_CDECL_BEGIN

typedef enum {
    MRP_RES_PENDING_OPERATION_NONE = 0,
    MRP_RES_PENDING_OPERATION_ACQUIRE,
    MRP_RES_PENDING_OPERATION_RELEASE,
} pending_operation_t;

typedef struct {
    const char *name;
    mrp_res_attribute_type_t type; /* s:char *, i:int32_t, u:uint32_t, f:double */
    union {
        const char *string;
        int32_t integer;
        uint32_t unsignd;
        double floating;
    };
} attribute_t;

typedef struct {
    uint32_t dim;
    mrp_res_attribute_t elems[0];
} attribute_array_t;

typedef struct {
    const char *name;
    bool sync_release;
    int num_attrs;
    mrp_res_attribute_t *attrs;
} resource_def_t;

typedef struct {
    uint32_t dim;
    resource_def_t defs[0];
} resource_def_array_t;

struct mrp_res_resource_private_s {
    mrp_res_resource_t *pub; /* composition */
    mrp_res_resource_set_t *set; /* owning set */

    bool sync_release;
    bool mandatory;
    bool shared;
    int num_attributes;
    mrp_res_attribute_t *attrs;
    uint32_t server_id;
};

struct mrp_res_resource_set_private_s {
    mrp_res_resource_set_t *pub; /* composition */
    mrp_res_context_t *cx; /* the context of this resource set */
    uint32_t id; /* id given by the server */
    uint32_t internal_id; /* id for checking identity */
    uint32_t internal_ref_count;
    uint32_t seqno;

    bool autorelease;

    mrp_res_resource_callback_t cb;
    mrp_res_resource_release_callback_t release_cb;
    void *user_data;
    void *release_cb_user_data;

    uint32_t num_resources;
    mrp_res_resource_t **resources;

    pending_operation_t waiting_for;

    mrp_list_hook_t hook;
};

struct mrp_res_context_private_s {
    int connection_id;

    /* mapping of server-side resource set numbers to library resource sets */
    mrp_htbl_t *rset_mapping;

    /* mapping of library resource sets to client resource sets */
    mrp_htbl_t *internal_rset_mapping;

    mrp_res_state_callback_t cb;
    void *user_data;

    mrp_mainloop_t *ml;
    mrp_sockaddr_t saddr;
    mrp_transport_t *transp;
    bool connected;

    mrp_res_string_array_t *master_classes;
    mrp_res_resource_set_t *master_resource_set;

    /* sometimes we need to know which query was answered */
    uint32_t next_seqno;

    /* running number for identifying resource sets */
    uint32_t next_internal_id;

    mrp_list_hook_t pending_sets;
};

uint32_t p_to_u(const void *p);
void *u_to_p(uint32_t u);

/*
 * logging macros
 */

#define __LOCATION__ __FILE__,__LINE__,__FUNCTION__

#define mrp_res_info(format, args...) \
    mrp_res_log_msg(MRP_LOG_INFO, __LOCATION__, format, ## args)

#define mrp_res_warning(format, args...) \
    mrp_res_log_msg(MRP_LOG_WARNING, __LOCATION__, format, ## args)

#define mrp_res_error(format, args...) \
    mrp_res_log_msg(MRP_LOG_ERROR, __LOCATION__, format, ## args)

void mrp_res_log_msg(mrp_log_level_t level, const char *file, int line,
                     const char *func, const char *format, ...);

MRP_CDECL_END

#endif /* __MURPHY_RESOURCE_API_PRIVATE_H__ */
