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
#include <murphy/common/mainloop.h>

#include <murphy-db/mqi.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/common-api.h>

#include "resource-set.h"
#include "application-class.h"
#include "resource.h"
#include "resource-client.h"
#include "resource-owner.h"
#include "resource-lua.h"


#define STAMP_MAX     ((uint32_t)1 << MRP_KEY_STAMP_BITS)
#define PRIORITY_MAX  ((uint32_t)1 << MRP_KEY_PRIORITY_BITS)


static MRP_LIST_HOOK(resource_set_list);
static uint32_t resource_set_count;
static mrp_htbl_t *id_hash;

static int add_to_id_hash(mrp_resource_set_t *);
static void remove_from_id_hash(mrp_resource_set_t *);

static mrp_resource_t *find_resource_by_name(mrp_resource_set_t *,const char*);
#if 0
static mrp_resource_t *find_resource_by_id(mrp_resource_set_t *, uint32_t);
#endif

static uint32_t get_request_stamp(void);
static const char *state_str(mrp_resource_state_t);
static void send_rset_event(mrp_resource_set_t *rset,
        mrp_resource_event_t ev);

uint32_t mrp_get_resource_set_count(void)
{
    return resource_set_count;
}

mrp_resource_set_t *mrp_resource_set_create(mrp_resource_client_t *client,
                                            bool auto_release,
                                            bool dont_wait,
                                            uint32_t priority,
                                            mrp_resource_event_cb_t event_cb,
                                            void *user_data)
{
    static uint32_t our_id;

    mrp_resource_set_t *rset;

    MRP_ASSERT(client, "invalid argument");

    if (priority >= PRIORITY_MAX)
        priority = PRIORITY_MAX - 1;

    if (!(rset = mrp_allocz(sizeof(mrp_resource_set_t))))
        mrp_log_error("Memory alloc failure. Can't create resource set");
    else {
        rset->id = ++our_id;

        rset->dont_wait.current = dont_wait;
        rset->dont_wait.client  = dont_wait;

        rset->auto_release.current = auto_release;
        rset->auto_release.client  = auto_release;

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

        add_to_id_hash(rset);
        mrp_resource_lua_register_resource_set(rset);

        send_rset_event(rset, MRP_RESOURCE_EVENT_CREATED);
    }

    return rset;
}

void mrp_resource_set_destroy(mrp_resource_set_t *rset)
{
    mrp_resource_state_t state;
    mrp_list_hook_t *entry, *n;
    mrp_resource_t *res;

    if (rset) {
        state = rset->state;

        rset->event = NULL; /* make sure nothing is sent any more */

        send_rset_event(rset, MRP_RESOURCE_EVENT_DESTROYED);

        mrp_resource_lua_unregister_resource_set(rset);
        remove_from_id_hash(rset);

        if (state == mrp_resource_acquire)
            mrp_resource_set_release(rset, MRP_RESOURCE_REQNO_INVALID);

        mrp_list_foreach(&rset->resource.list, entry, n) {
            res = mrp_list_entry(entry, mrp_resource_t, list);
            mrp_resource_notify(res, rset, MRP_RESOURCE_EVENT_DESTROYED);
            mrp_resource_destroy(res);
        }

        mrp_list_delete(&rset->list);
        mrp_list_delete(&rset->client.list);
        mrp_list_delete(&rset->class.list);

        mrp_free(rset);

        if (resource_set_count > 0)
            resource_set_count--;
    }
}

mrp_resource_set_t *mrp_resource_set_find_by_id(uint32_t id)
{
    return id_hash ? mrp_htbl_lookup(id_hash, NULL + id) : NULL;
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

mrp_resource_t *mrp_resource_set_find_resource(uint32_t rsetid,
                                               const char *resnam)
{
    mrp_resource_set_t *rset;
    mrp_resource_t *res;

    MRP_ASSERT(resnam, "invalid argument");

    if (!(rset = mrp_resource_set_find_by_id(rsetid)))
        res = NULL;
    else
        res = find_resource_by_name(rset, resnam);

    return res;
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
    uint32_t rsetid;
    bool autorel;

    MRP_ASSERT(rset && name, "invalid argument");

    rsetid  = rset->id;
    autorel = rset->auto_release.client;

    if (!(res = mrp_resource_create(name, rsetid, autorel, shared, attrs))) {
        mrp_log_error("Can't add resource '%s' name to resource set %u",
                      name, rset->id);
        return -1;
    }

    mask = mrp_resource_get_mask(res);

    rset->resource.mask.all       |= mask;
    rset->resource.mask.mandatory |= mandatory ? mask : 0;
    rset->resource.share          |= mrp_resource_is_shared(res);


    mrp_list_append(&rset->resource.list, &res->list);

    mrp_resource_lua_add_resource_to_resource_set(rset, res);

    return 0;
}

mrp_attr_t *mrp_resource_set_read_attribute(mrp_resource_set_t *rset,
                                            const char *resnam,
                                            uint32_t attridx,
                                            mrp_attr_t *buf)
{
    mrp_resource_t *res;

    MRP_ASSERT(rset && resnam, "invalid argument");

    if (!(res = find_resource_by_name(rset, resnam)))
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

    if (!(res = find_resource_by_name(rset, resnam)))
        return NULL;

    return mrp_resource_read_all_attributes(res, buflen, buf);
}

int mrp_resource_set_write_attributes(mrp_resource_set_t *rset,
                                      const char *resnam,
                                      mrp_attr_t *attrs)
{
    mrp_resource_t *res;

    MRP_ASSERT(rset && resnam && attrs, "invalid argument");

    if (!(res = find_resource_by_name(rset, resnam)))
        return -1;

    if (mrp_resource_write_attributes(res, attrs) < 0)
        return -1;

    return 0;
}

void mrp_resource_set_acquire(mrp_resource_set_t *rset, uint32_t reqid)
{
    mrp_resource_state_t old_state;
    mqi_handle_t trh;

    MRP_ASSERT(rset, "invalid argument");

    mrp_debug("acquiring resource set #%d", rset->id);

    old_state = rset->state;
    rset->state = mrp_resource_acquire;

    if (rset->class.ptr) {
        rset->request.id = reqid;
        rset->request.stamp = get_request_stamp();

        mrp_application_class_move_resource_set(rset);

        if (old_state != mrp_resource_acquire)
            mrp_resource_set_notify(rset, MRP_RESOURCE_EVENT_ACQUIRE);

        trh = mqi_begin_transaction();
        mrp_resource_owner_update_zone(rset->zone, rset, reqid);
        mqi_commit_transaction(trh);
    }
}

void mrp_resource_set_release(mrp_resource_set_t *rset, uint32_t reqid)
{
    mqi_handle_t trh;

    MRP_ASSERT(rset, "invalid argument");

    mrp_debug("releasing resource set #%d", rset->id);

    if (!rset->class.ptr)
        rset->state = mrp_resource_release;
    else {
        if (rset->state == mrp_resource_release) {
            if (rset->event)
                rset->event(reqid, rset, rset->user_data);
        }
        else {
            rset->state = mrp_resource_release;
            rset->request.id = reqid;
            rset->request.stamp = get_request_stamp();

            mrp_application_class_move_resource_set(rset);

            mrp_resource_set_notify(rset, MRP_RESOURCE_EVENT_RELEASE);

            trh = mqi_begin_transaction();
            mrp_resource_owner_update_zone(rset->zone, rset, reqid);
            mqi_commit_transaction(trh);
        }
    }
}

void mrp_resource_set_updated(mrp_resource_set_t *rset)
{
    mrp_resource_t *res;
    mrp_resource_def_t *def;
    mrp_list_hook_t *resen, *n;
    mrp_resource_mask_t mask;
    bool grant;

    MRP_ASSERT(rset, "invalid argument");

    mrp_debug("resource set got #%d updated", rset->id);

    mrp_list_foreach(&rset->resource.list, resen, n) {
        res = mrp_list_entry(resen, mrp_resource_t, list);
        def = res->def;

        mask = ((mrp_resource_mask_t)1) << def->id;
        grant =  (mask & rset->resource.mask.grant) ? true : false;

        mrp_debug("    %s now %sgranted", def->name, grant ? "" : "not ");

        mrp_resource_user_update(res, rset->state, grant);
    }
}


MRP_REGISTER_EVENTS(resource_events,
       MRP_EVENT(MURPHY_RESOURCE_EVENT_CREATED  , MRP_RESOURCE_EVENT_CREATED  ),
       MRP_EVENT(MURPHY_RESOURCE_EVENT_DESTROYED, MRP_RESOURCE_EVENT_DESTROYED),
       MRP_EVENT(MURPHY_RESOURCE_EVENT_ACQUIRE  , MRP_RESOURCE_EVENT_ACQUIRE  ),
       MRP_EVENT(MURPHY_RESOURCE_EVENT_RELEASE  , MRP_RESOURCE_EVENT_RELEASE  ));



static void send_rset_event(mrp_resource_set_t *rset, mrp_resource_event_t ev)
{
    mrp_event_bus_t *bus   = MRP_GLOBAL_BUS;
    uint32_t         id    = resource_events[ev].id;
    int              flags = MRP_EVENT_SYNCHRONOUS;;
    uint16_t          tag  = MRP_RESOURCE_TAG_RSET_ID;

    MRP_ASSERT(rset, "invalid argument");

    /* The resource set id is enough information, because the full resource
     * set can be found by making a query with the id. */


    mrp_debug("emit event %d for rset %u", id, rset->id);

    switch (ev) {
    case MRP_RESOURCE_EVENT_CREATED:
        mrp_event_emit_msg(bus, id, flags, MRP_MSG_TAG_UINT32(tag, rset->id));
        break;
    case MRP_RESOURCE_EVENT_ACQUIRE:
        mrp_event_emit_msg(bus, id, flags, MRP_MSG_TAG_UINT32(tag, rset->id));
        break;
    case MRP_RESOURCE_EVENT_RELEASE:
        mrp_event_emit_msg(bus, id, flags, MRP_MSG_TAG_UINT32(tag, rset->id));
        break;
    case MRP_RESOURCE_EVENT_DESTROYED:
        mrp_event_emit_msg(bus, id, flags, MRP_MSG_TAG_UINT32(tag, rset->id));
        break;
    default:
        break;
    }
}

void mrp_resource_set_notify(mrp_resource_set_t *rset, mrp_resource_event_t ev)
{
    mrp_resource_t *res;
    void *cursor = NULL;

    MRP_ASSERT(rset, "invalid argument");

    send_rset_event(rset, ev);

    while ((res = mrp_resource_set_iterate_resources(rset, &cursor)))
        mrp_resource_notify(res, rset, ev);
}

void mrp_resource_set_request_auto_release(mrp_resource_set_t *rset,
                                           bool auto_release)
{
    MRP_ASSERT(rset, "invalid argument");

    rset->auto_release.current = auto_release;
}

void mrp_resource_set_request_dont_wait(mrp_resource_set_t *rset,
                                        bool dont_wait)
{
    MRP_ASSERT(rset, "invalid argument");

    rset->dont_wait.current = dont_wait;
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

    if (len <= 0)
        return 0;

    MRP_ASSERT(rset && indent < sizeof(gap)-1 && buf,
               "invalid argument");

    gap[indent] = '\0';

    e = (p = buf) + len;

    mandatory = rset->resource.mask.mandatory;

    PRINT("%s%3u - 0x%02x/0x%02x 0x%02x/0x%02x 0x%08x %d %s%s%s %s\n",
          gap, rset->id,
          rset->resource.mask.all, mandatory,
          rset->resource.mask.grant, rset->resource.mask.advice,
          mrp_application_class_get_sorting_key(rset), rset->class.priority,
          rset->resource.share ? "shared   ":"exclusive",
          rset->auto_release.client ? ",autorelease" : "",
          rset->dont_wait.client ? ",dontwait" : "",
          state_str(rset->state));

    mrp_list_foreach(&rset->resource.list, resen, n) {
        res = mrp_list_entry(resen, mrp_resource_t, list);
        p  += mrp_resource_print(res, mandatory, indent+6, p, e-p);
    }

    if (p >= e) {
        if (len >= 5) {
            e[-5] = e[-4] = e[-3] = '.';
            e[-2] = '\n';
            e[-1] = '\0';
        }
    }

    return p - buf;

#undef PRINT
}

static uint32_t rset_hash(const void *key)
{
    return (uint32_t)(key - NULL);
}

static int rset_comp(const void *key1, const void *key2)
{
    uint32_t k1 = key1 - NULL;
    uint32_t k2 = key2 - NULL;

    return (k1 == k2) ? 0 : ((k1 > k2) ? 1 : -1);
}

static void init_id_hash(void)
{
    mrp_htbl_config_t cfg;

    if (!id_hash) {
        cfg.nentry  = 32;
        cfg.comp    = rset_comp;
        cfg.hash    = rset_hash;
        cfg.free    = NULL;
        cfg.nbucket = cfg.nentry;

        id_hash = mrp_htbl_create(&cfg);

        MRP_ASSERT(id_hash, "failed to make id_hash for resource sets");
    }
}

static int add_to_id_hash(mrp_resource_set_t *rset)
{
    MRP_ASSERT(rset, "invalid argument");

    init_id_hash();

    if (!mrp_htbl_insert(id_hash, NULL + rset->id, rset))
        return -1;

    return 0;
}

static void remove_from_id_hash(mrp_resource_set_t *rset)
{
    mrp_resource_set_t *deleted;

    if (id_hash && rset) {
        deleted = mrp_htbl_remove(id_hash, NULL + rset->id, false);

        MRP_ASSERT(!deleted || deleted == rset, "confused with data "
                   "structures when deleting resource-set from id hash");

        /* in case we were not compiled with debug enabled */
        if (deleted != rset) {
            mrp_log_error("confused with data structures when deleting "
                          "resource-set '%u' from id hash", rset->id);
        }
    }
}

static mrp_resource_t *find_resource_by_name(mrp_resource_set_t *rset,
                                             const char *name)
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

#if 0
static mrp_resource_t *find_resource_by_id(mrp_resource_set_t *rset,
                                           uint32_t id)
{
    mrp_list_hook_t *entry, *n;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;

    MRP_ASSERT(rset, "invalid_argument");

    mrp_list_foreach(&rset->resource.list, entry, n) {
        res = mrp_list_entry(entry, mrp_resource_t, list);
        rdef = res->def;

        MRP_ASSERT(rdef, "confused with data structures");

        if (id == rdef->id)
            return res;
    }

    return NULL;
}
#endif

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
