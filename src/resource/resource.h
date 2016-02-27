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

#ifndef __MURPHY_RESOURCE_H__
#define __MURPHY_RESOURCE_H__

#include <murphy/common/list.h>

#include "attribute.h"


struct mrp_resource_def_s {
    uint32_t            id;
    const char         *name;
    bool                shareable;
    bool                sync_release;
    struct {
        mrp_list_hook_t list;
        mrp_resource_mgr_ftbl_t *ftbl;
        void *userdata;
    }                   manager;
    uint32_t            nattr;
    mrp_attr_def_t      attrdefs[0];
};

struct mrp_resource_s {
    mrp_list_hook_t     list;
    uint32_t            rsetid;
    mrp_resource_def_t *def;
    bool                shared;
    mrp_attr_value_t    attrs[0];
};



uint32_t            mrp_resource_definition_count(void);
mrp_resource_def_t *mrp_resource_definition_find_by_name(const char *);
mrp_resource_def_t *mrp_resource_definition_find_by_id(uint32_t);
mrp_resource_def_t *mrp_resource_definition_iterate_manager(void **);


mrp_resource_t     *mrp_resource_create(const char *, uint32_t, bool,
                                        bool, mrp_attr_t *);
void                mrp_resource_destroy(mrp_resource_t *);

void                mrp_resource_notify(mrp_resource_t *, mrp_resource_set_t *,
                                        mrp_resource_event_t);

int                 mrp_resource_print(mrp_resource_t*, uint32_t,
                                       size_t, char *, int);
int                 mrp_resource_attribute_print(mrp_resource_t *, char *,int);

void                mrp_resource_user_update(mrp_resource_t *, int, bool);

#endif  /* __MURPHY_RESOURCE_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
