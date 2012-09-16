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
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>
#include <murphy/common/log.h>

#include <murphy/resource/client-api.h>

#include "resource-set.h"
#include "resource-class.h"
#include "resource.h"
#include "resource-owner.h"


#define STAMP_MAX     ((uint32_t)1 << MRP_KEY_STAMP_BITS)
#define PRIORITY_MAX  ((uint32_t)1 << MRP_KEY_PRIORITY_BITS)

static MRP_LIST_HOOK(resource_set_list);


static uint32_t    get_request_stamp(void);
static const char *request_str(mrp_resource_request_t);


mrp_resource_set_t *mrp_resource_set_create(uint32_t client_id,
                                            void *client_data,
                                            uint32_t priority)
{
    static uint32_t our_id;

    mrp_resource_set_t *rset;

    MRP_ASSERT(client_data, "invalid argument");

    if (priority >= PRIORITY_MAX)
        priority = PRIORITY_MAX - 1;

    if (!(rset = mrp_allocz(sizeof(mrp_resource_set_t))))
        mrp_log_error("Memory alloc failure. Can't create resource set");
    else {
        rset->id = our_id++;

        mrp_list_init(&rset->resource.list);
        rset->resource.share = false;

        mrp_list_init(&rset->client.list);
        rset->client.id   = client_id;
        rset->client.data = client_data;

        mrp_list_init(&rset->class.list);
        rset->class.priority = priority;

        mrp_list_append(&resource_set_list, &rset->list);
    }

    return rset;
}

uint32_t mrp_get_resource_set_id(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->id;
}

mrp_resource_t *mrp_resource_set_iterate_resources(mrp_resource_set_t *rset,
                                                   void **cursor)
{
    mrp_list_hook_t *list, *entry;

    MRP_ASSERT(rset && cursor, "invalid argument");

    list  = &rset->resource.list;
    entry = (*cursor == NULL) ? list->next : (mrp_list_hook_t *)*cursor;

    if (entry == list)
        return NULL;

    *cursor = entry->next;

    return mrp_list_entry(entry, mrp_resource_t, list);
}


int mrp_resource_set_add_resource(mrp_resource_set_t *rset,
                                  const char         *name,
                                  bool                shared,
                                  mrp_attr_t         *attrs,
                                  bool                mandatory)
{
    uint32_t mask;
    mrp_resource_t *res;

    MRP_ASSERT(rset && name, "invalid argument");

    if (!(res = mrp_resource_create(name, shared, attrs))) {
        mrp_log_error("Can't add resource '%s' name to resource set %u",
                      name, rset->id);
        return -1;
    }

    mask = mrp_resource_get_mask(res);

    rset->resource.mask.all       |= mask;
    rset->resource.mask.mandatory |= mandatory ? mask : 0;
    rset->resource.share          |= mrp_resource_is_shared(res);


    mrp_list_append(&rset->resource.list, &res->list);

    return 0;
}

void mrp_resource_set_acquire(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    if (rset->request.type != mrp_resource_acquire) {
        rset->request.type  = mrp_resource_acquire;
        rset->request.stamp = get_request_stamp();

        mrp_resource_class_move_resource_set(rset);
        mrp_resource_owner_update_zone(rset->zone);
    }
}

void mrp_resource_set_release(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    if (rset->request.type != mrp_resource_release) {
        rset->request.type  = mrp_resource_release;
        rset->request.stamp = get_request_stamp();

        mrp_resource_class_move_resource_set(rset);
        mrp_resource_owner_update_zone(rset->zone);
    }
}


int mrp_resource_set_print(mrp_resource_set_t *rset, size_t indent,
                           char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_resource_t *res;
    mrp_list_hook_t *resen, *n;
    uint32_t mandatory;
    char gap[] = "                         ";
    char *p, *e;

    MRP_ASSERT(rset && indent < sizeof(gap)-1 && buf && len > 0,
               "invalid argument");

    gap[indent] = '\0';

    e = (p = buf) + len;

    mandatory = rset->resource.mask.mandatory;

    PRINT("%s%3u - 0x%02x/0x%02x 0x%02x/0x%02x 0x%08x %d %s %s\n",
          gap, rset->id,
          rset->resource.mask.all, mandatory,
          rset->resource.mask.grant, rset->resource.mask.advice,
          mrp_resource_class_get_sorting_key(rset), rset->class.priority,
          rset->resource.share ? "shared   ":"exclusive",
          request_str(rset->request.type));

    mrp_list_foreach(&rset->resource.list, resen, n) {
        res = mrp_list_entry(resen, mrp_resource_t, list);
        p  += mrp_resource_print(res, mandatory, indent+6, p, e-p);
    }

    return p - buf;

#undef PRINT
}

static uint32_t get_request_stamp(void)
{
    static uint32_t  stamp;

    mrp_list_hook_t *entry, *n;
    mrp_resource_set_t *rset;
    uint32_t min;

    if ((min = stamp) >= STAMP_MAX) {
        mrp_log_info("rebasing resource set stamps");

        mrp_list_foreach(&resource_set_list, entry, n) {
            rset = mrp_list_entry(entry, mrp_resource_set_t, list);
            if (rset->request.stamp < min)
                min = rset->request.stamp;
        }

        stamp -= min;

        mrp_list_foreach(&resource_set_list, entry, n) {
            rset = mrp_list_entry(entry, mrp_resource_set_t, list);
            rset->request.stamp -= min;
        }
    }

    MRP_ASSERT(stamp < STAMP_MAX, "Request stamp overflow");

    return stamp++;
}

static const char *request_str(mrp_resource_request_t request)
{
    switch(request) {
    case mrp_resource_no_request:     return "no-request";
    case mrp_resource_release:        return "release";
    case mrp_resource_acquire:        return "acquire";
    default:                          return "< ??? >";
    }
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
