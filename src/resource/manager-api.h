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

#ifndef __MURPHY_RESOURCE_MANAGER_API_H__
#define __MURPHY_RESOURCE_MANAGER_API_H__

#include <murphy/resource/common-api.h>

uint32_t mrp_zone_get_id(mrp_zone_t *zone);
const char *mrp_zone_get_name(mrp_zone_t *zone);
mrp_attr_t *mrp_zone_read_attribute(mrp_zone_t *zone,
                                    uint32_t attribute_index,
                                    mrp_attr_t *buf);
mrp_attr_t *mrp_zone_read_all_attributes(mrp_zone_t *zone,
                                         uint32_t buflen,
                                         mrp_attr_t *buf);


const char *mrp_application_class_get_name(mrp_application_class_t *class);
uint32_t mrp_application_class_get_priority(mrp_application_class_t *class);


uint32_t mrp_resource_definition_create(const char *name,
                                        bool shareable,
                                        mrp_attr_def_t *attrdefs,
                                        mrp_resource_mgr_ftbl_t *manager,
                                        void *manager_data);
void mrp_lua_resclass_create_from_c(uint32_t id);


const char *mrp_resource_get_application_class(mrp_resource_t *resource);

void mrp_resource_owner_recalc(uint32_t zoneid);

#endif  /* __MURPHY_RESOURCE_MANAGER_API_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
