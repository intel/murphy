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

#include <stdio.h>
#include <string.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include <resource/client-api.h>

#include "resource-client.h"
#include "resource-set.h"



static MRP_LIST_HOOK(client_list);


mrp_resource_client_t *mrp_resource_client_create(const char *name,
                                                  void *user_data)
{
    mrp_resource_client_t *client;
    const char *dup_name;

    MRP_ASSERT(name, "invalid argument");

    if (!(client = mrp_allocz(sizeof(*client))) ||
        !(dup_name = mrp_strdup(name)))
    {
        mrp_log_error("Memory alloc failure. Can't create client '%s'", name);
        return NULL;
    }

    client->name = dup_name;
    client->user_data = user_data;
    mrp_list_init(&client->resource_sets);

    mrp_list_append(&client_list, &client->list);

    return client;
}

void mrp_resource_client_destroy(mrp_resource_client_t *client)
{
    mrp_list_hook_t *entry, *n;
    mrp_resource_set_t *rset;

    if (client) {
        mrp_list_delete(&client->list);

        mrp_list_foreach(&client->resource_sets, entry, n) {
            rset = mrp_list_entry(entry, mrp_resource_set_t, client.list);
            mrp_resource_set_destroy(rset);
        }

        mrp_free((void *) client->name);
        mrp_free(client);
    }
}


mrp_resource_set_t *mrp_resource_client_find_set(mrp_resource_client_t *client,
                                                 uint32_t resource_set_id)
{
    mrp_list_hook_t *entry, *n;
    mrp_resource_set_t *rset;

    if (client) {
        mrp_list_foreach(&client->resource_sets, entry, n) {
            rset = mrp_list_entry(entry, mrp_resource_set_t, client.list);

            if (resource_set_id == rset->id)
                return rset;
        }
    }

    return NULL;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
