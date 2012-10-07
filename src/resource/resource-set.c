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
#include <murphy/resource/common-api.h>

#include "resource-set.h"
#include "application-class.h"
#include "resource.h"
#include "resource-client.h"
#include "resource-owner.h"


#define STAMP_MAX     ((uint32_t)1 << MRP_KEY_STAMP_BITS)
#define PRIORITY_MAX  ((uint32_t)1 << MRP_KEY_PRIORITY_BITS)

static MRP_LIST_HOOK(resource_set_list);
static uint32_t resource_set_count;

static mrp_resource_t *find_resource(mrp_resource_set_t *, const char *);
static uint32_t get_request_stamp(void);
static const char *state_str(mrp_resource_state_t);


uint32_t mrp_get_resource_set_count(void)
{
    return resource_set_count;
}

mrp_resource_set_t *mrp_resource_set_create(mrp_resource_client_t *client,
                                            bool auto_release,
                                            uint32_t priority,
                                            mrp_resource_event_cb_t event_cb,
                                            void *user_data)
{
    static uint32_t our_id;

    mrp_resource_set_t *rset;

    MRP_ASSERT(client, "invlaid argument");

    if (priority >= PRIORITY_MAX)
        priority = PRIORITY_MAX - 1;

    if (!(rset = mrp_allocz(sizeof(mrp_resource_set_t))))
        mrp_log_error("Memory alloc failure. Can't create resource set");
    else {
        rset->id = ++our_id;
        rset->auto_release = auto_release;

        mrp_list_init(&rset->resource.list);
        rset->resource.share = false;

        mrp_list_append(&client->resource_sets, &rset->client.list);
        rset->client.ptr = client;
        rset->client.reqno = MRP_RESOURCE_REQNO_INVALID;

        mrp_list_init(&rset->class.list);
        rset->class.priority = priority;

        mrp_list_append(&resource_set_list, &rset->list);

        rset->event = event_cb;
        rset->user_data = user_data;

        resource_set_count++;
    }

    return rset;
}

void mrp_resource_set_destroy(mrp_resource_set_t *rset)
{
    mrp_resource_state_t state;
    uint32_t zoneid;
    mrp_list_hook_t *entry, *n;
    mrp_resource_t *res;

    if (rset) {
        state  = rset->state;
        zoneid = rset->zone;

        mrp_list_foreach(&rset->resource.list, entry, n) {
            res = mrp_list_entry(entry, mrp_resource_t, list);
            mrp_resource_destroy(res);
        }

        mrp_list_delete(&rset->list);
        mrp_list_delete(&rset->client.list);
        mrp_list_delete(&rset->class.list);

        mrp_free(rset);

        if (resource_set_count > 0)
            resource_set_count--;

        if (state == mrp_resource_acquire)
            mrp_resource_owner_update_zone(zoneid, MRP_RESOURCE_REQNO_INVALID);
    }
}



uint32_t mrp_get_resource_set_id(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->id;
}

mrp_resource_state_t mrp_get_resource_set_state(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->state;
}

mrp_resource_mask_t mrp_get_resource_set_grant(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->resource.mask.grant;
}

mrp_resource_mask_t mrp_get_resource_set_advice(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->resource.mask.advice;
}

mrp_resource_client_t *mrp_get_resource_set_client(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    return rset->client.ptr;
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

mrp_attr_t *mrp_resource_set_read_attribute(mrp_resource_set_t *rset,
                                            const char *resnam,
                                            uint32_t attridx,
                                            mrp_attr_t *buf)
{
    mrp_resource_t *res;

    MRP_ASSERT(rset && resnam, "invalid argument");

    if (!(res = find_resource(rset, resnam)))
        return NULL;

    return mrp_resource_read_attribute(res, attridx, buf);
}

mrp_attr_t *mrp_resource_set_read_all_attributes(mrp_resource_set_t *rset,
                                                 const char *resnam,
                                                 uint32_t buflen,
                                                 mrp_attr_t *buf)
{
    mrp_resource_t *res;

    MRP_ASSERT(rset && resnam, "invalid argument");

    if (!(res = find_resource(rset, resnam)))
        return NULL;

    return mrp_resource_read_all_attributes(res, buflen, buf);
}

int mrp_resource_set_write_attributes(mrp_resource_set_t *rset,
                                      const char *resnam,
                                      mrp_attr_t *attrs)
{
    mrp_resource_t *res;

    MRP_ASSERT(rset && resnam && attrs, "invalid argument");

    if (!(res = find_resource(rset, resnam)))
        return -1;

    if (mrp_resource_write_attributes(res, attrs) < 0)
        return -1;

    return 0;
}

void mrp_resource_set_acquire(mrp_resource_set_t *rset, uint32_t reqid)
{
    MRP_ASSERT(rset, "invalid argument");

    if (rset->state != mrp_resource_acquire) {
        rset->state = mrp_resource_acquire;
        rset->request.id = reqid;
        rset->request.stamp = get_request_stamp();

        mrp_application_class_move_resource_set(rset);
        mrp_resource_owner_update_zone(rset->zone, reqid);
    }
}

void mrp_resource_set_release(mrp_resource_set_t *rset, uint32_t reqid)
{
    MRP_ASSERT(rset, "invalid argument");

    if (rset->state != mrp_resource_release) {
        rset->state = mrp_resource_release;
        rset->request.id = reqid;
        rset->request.stamp = get_request_stamp();

        mrp_application_class_move_resource_set(rset);
        mrp_resource_owner_update_zone(rset->zone, reqid);
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
          mrp_application_class_get_sorting_key(rset), rset->class.priority,
          rset->resource.share ? "shared   ":"exclusive",
          state_str(rset->state));

    mrp_list_foreach(&rset->resource.list, resen, n) {
        res = mrp_list_entry(resen, mrp_resource_t, list);
        p  += mrp_resource_print(res, mandatory, indent+6, p, e-p);
    }

    return p - buf;

#undef PRINT
}

static mrp_resource_t *find_resource(mrp_resource_set_t *rset,const char *name)
{
    mrp_list_hook_t *entry, *n;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;

    MRP_ASSERT(rset && name, "invalid_argument");

    mrp_list_foreach(&rset->resource.list, entry, n) {
        res = mrp_list_entry(entry, mrp_resource_t, list);
        rdef = res->def;

        MRP_ASSERT(rdef, "confused with data structures");

        if (!strcasecmp(name, rdef->name))
            return res;
    }

    return NULL;
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

static const char *state_str(mrp_resource_state_t state)
{
    switch(state) {
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
