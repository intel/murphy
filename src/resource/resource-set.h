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

#ifndef __MURPHY_RESOURCE_SET_H__
#define __MURPHY_RESOURCE_SET_H__

#include <murphy/common/list.h>

#include "data-types.h"

/* event protocol */

#define MURPHY_RESOURCE_EVENT_CREATED   "resource_set_created"
#define MURPHY_RESOURCE_EVENT_DESTROYED "resource_set_destroyed"
#define MURPHY_RESOURCE_EVENT_ACQUIRE   "resource_set_acquire"
#define MURPHY_RESOURCE_EVENT_RELEASE   "resource_set_release"

#define MRP_RESOURCE_TAG_RSET_ID ((uint16_t) 1)

struct mrp_resource_set_s {
    mrp_list_hook_t                 list;
    uint32_t                        id;
    mrp_resource_state_t            state;
    struct {
        bool current;
        bool client;
    }                               auto_release;
    struct {
        bool current;
        bool client;
    }                               dont_wait;
    struct {
        struct {
            mrp_resource_mask_t all;
            mrp_resource_mask_t mandatory;
            mrp_resource_mask_t grant;
            mrp_resource_mask_t advice;
            struct {
                mrp_resource_mask_t acquire;
                mrp_resource_mask_t release;
            } pending;
        } mask;
        mrp_list_hook_t list;
        bool share;
    }                               resource;
    struct {
        mrp_list_hook_t list;
        mrp_resource_client_t *ptr;
        uint32_t reqno;
    }                               client;
    struct {
        mrp_list_hook_t list;
        mrp_application_class_t *ptr;
        uint32_t priority;
    }                               class;
    uint32_t                        zone;
    struct {
        uint32_t id;
        uint32_t stamp;
    }                               request;
    mrp_resource_event_cb_t         event;
    void                           *user_data;
};


mrp_resource_set_t *mrp_resource_set_find_by_id(uint32_t);
mrp_resource_t     *mrp_resource_set_find_resource(uint32_t, const char *);
uint32_t            mrp_get_resource_set_count(void);
void                mrp_resource_set_updated(mrp_resource_set_t *);
void                mrp_resource_set_notify(mrp_resource_set_t *,
                                            mrp_resource_event_t);
void                mrp_resource_set_request_auto_release(mrp_resource_set_t *,
                                                          bool);
void                mrp_resource_set_request_dont_wait(mrp_resource_set_t *,
                                                       bool);
int                 mrp_resource_set_print(mrp_resource_set_t *, size_t,
                                           char *, int);


#endif  /* __MURPHY_RESOURCE_SET_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
