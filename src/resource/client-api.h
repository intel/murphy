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

#ifndef __MURPHY_RESOURCE_CLIENT_API_H__
#define __MURPHY_RESOURCE_CLIENT_API_H__

#include <murphy/resource/common-api.h>

mrp_resource_client_t *mrp_resource_client_create(const char *name,
                                                  void *user_data);
void mrp_resource_client_destroy(mrp_resource_client_t *client);

mrp_resource_set_t *mrp_resource_client_find_set(mrp_resource_client_t *client,
                                                 uint32_t resource_set_id);


const char **mrp_zone_get_all_names(uint32_t buflen, const char **buf);

const char **mrp_resource_definition_get_all_names(uint32_t buflen,
                                                   const char **buf);

uint32_t mrp_resource_definition_get_resource_id_by_name(const char *name);

mrp_attr_t *
mrp_resource_definition_read_all_attributes(uint32_t resource_id,
                                            uint32_t buflen,
                                            mrp_attr_t *buf);

const char **mrp_application_class_get_all_names(uint32_t buflen,
                                                 const char **buf);

int mrp_application_class_add_resource_set(const char *class_name,
                                           const char *zone_name,
                                           mrp_resource_set_t *resource_set,
                                           uint32_t request_id);

mrp_resource_set_t *mrp_resource_set_create(mrp_resource_client_t *client,
                                            bool auto_release,
                                            bool dont_wait,
                                            uint32_t priority,
                                            mrp_resource_event_cb_t event_cb,
                                            void *user_data);
void mrp_resource_set_destroy(mrp_resource_set_t *resource_set);

uint32_t mrp_get_resource_set_id(mrp_resource_set_t *resource_set);

mrp_resource_state_t
mrp_get_resource_set_state(mrp_resource_set_t *resource_set);

mrp_resource_mask_t
mrp_get_resource_set_grant(mrp_resource_set_t *resource_set);

mrp_resource_mask_t
mrp_get_resource_set_advice(mrp_resource_set_t *resource_set);

mrp_resource_client_t *
mrp_get_resource_set_client(mrp_resource_set_t *resource_set);


int mrp_resource_set_add_resource(mrp_resource_set_t *resource_set,
                                  const char *resource_name,
                                  bool shared,
                                  mrp_attr_t *attribute_list,
                                  bool mandatory);

mrp_attr_t *
mrp_resource_set_read_attribute(mrp_resource_set_t *resource_set,
                                const char *resource_name,
                                uint32_t attribute_index,
                                mrp_attr_t *buf);

mrp_attr_t *
mrp_resource_set_read_all_attributes(mrp_resource_set_t *resource_set,
                                     const char *resource_name,
                                     uint32_t buflen,
                                     mrp_attr_t *buf);

int mrp_resource_set_write_attributes(mrp_resource_set_t *resource_set,
                                      const char *resource_name,
                                      mrp_attr_t *attribute_list);

void mrp_resource_set_acquire(mrp_resource_set_t *resource_set,
                              uint32_t request_id);

void mrp_resource_set_release(mrp_resource_set_t *resource_set,
                              uint32_t request_id);

mrp_resource_t *
mrp_resource_set_iterate_resources(mrp_resource_set_t *resource_set,void **it);

uint32_t mrp_resource_get_id(mrp_resource_t *resource);
const char *mrp_resource_get_name(mrp_resource_t *resource);
mrp_resource_mask_t mrp_resource_get_mask(mrp_resource_t *resource);
bool mrp_resource_is_shared(mrp_resource_t *resource);

/* Find a resource set given the resource set id. */
mrp_resource_set_t *mrp_resource_set_find_by_id(uint32_t id);

/* Get a single attribute object that contains the current value. */
mrp_attr_t *mrp_resource_set_get_attribute_by_name(
        mrp_resource_set_t *resource_set, const char *resource_name,
        const char *attribute_name);

/* Free the object obtained with mrp_resource_set_get_attribute_by_name. */
void mrp_resource_set_free_attribute(mrp_attr_t *attr);

#endif  /* __MURPHY_RESOURCE_CLIENT_API_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
