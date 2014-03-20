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
#include <ctype.h>
#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>
#include <murphy/common/log.h>

#include <murphy-db/mqi.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>

#include "resource-owner.h"
#include "application-class.h"
#include "resource-set.h"
#include "resource.h"
#include "zone.h"
#include "resource-lua.h"

#define NAME_LENGTH          24

#define ZONE_ID_IDX          0
#define ZONE_NAME_IDX        1
#define CLASS_NAME_IDX       2
#define RSET_ID_IDX          3
#define FIRST_ATTRIBUTE_IDX  4

typedef struct {
    uint32_t          zone_id;
    const char       *zone_name;
    const char       *class_name;
    uint32_t          rset_id;
    mrp_attr_value_t  attrs[MQI_COLUMN_MAX];
} owner_row_t;

static mrp_resource_owner_t  resource_owners[MRP_ZONE_MAX * MRP_RESOURCE_MAX];
static mqi_handle_t          owner_tables[MRP_RESOURCE_MAX];

static mrp_resource_owner_t *get_owner(uint32_t, uint32_t);
static void reset_owners(uint32_t, mrp_resource_owner_t *);
static bool grant_ownership(mrp_resource_owner_t *, mrp_zone_t *,
                            mrp_application_class_t *, mrp_resource_set_t *,
                            mrp_resource_t *);
static bool advice_ownership(mrp_resource_owner_t *, mrp_zone_t *,
                             mrp_application_class_t *, mrp_resource_set_t *,
                             mrp_resource_t *);

static void manager_start_transaction(mrp_zone_t *);
static void manager_end_transaction(mrp_zone_t *);

static void delete_resource_owner(mrp_zone_t *, mrp_resource_t *);
static void insert_resource_owner(mrp_zone_t *, mrp_application_class_t *,
                                  mrp_resource_set_t *, mrp_resource_t *);
static void update_resource_owner(mrp_zone_t *, mrp_application_class_t *,
                                  mrp_resource_set_t *, mrp_resource_t *);
static void set_attr_descriptors(mqi_column_desc_t *, mrp_resource_t *);


int mrp_resource_owner_create_database_table(mrp_resource_def_t *rdef)
{
    MQI_COLUMN_DEFINITION_LIST(base_coldefs,
        MQI_COLUMN_DEFINITION( "zone_id"          , MQI_UNSIGNED             ),
        MQI_COLUMN_DEFINITION( "zone_name"        , MQI_VARCHAR(NAME_LENGTH) ),
        MQI_COLUMN_DEFINITION( "application_class", MQI_VARCHAR(NAME_LENGTH) ),
        MQI_COLUMN_DEFINITION( "resource_set_id"  , MQI_UNSIGNED             )
    );

    MQI_INDEX_DEFINITION(indexdef,
        MQI_INDEX_COLUMN( "zone_id" )
    );

    static bool initialized = false;

    char name[256];
    mqi_column_def_t coldefs[MQI_COLUMN_MAX + 1];
    mqi_column_def_t *col;
    mrp_attr_def_t *atd;
    mqi_handle_t table;
    char c, *p;
    size_t i,j;

    if (!initialized) {
        mqi_open();
        for (i = 0;  i < MRP_RESOURCE_MAX;  i++)
            owner_tables[i] = MQI_HANDLE_INVALID;
        initialized = true;
    }

    MRP_ASSERT(sizeof(base_coldefs) < sizeof(coldefs),"too many base columns");
    MRP_ASSERT(rdef, "invalid argument");
    MRP_ASSERT(rdef->id < MRP_RESOURCE_MAX, "confused with data structures");
    MRP_ASSERT(owner_tables[rdef->id] == MQI_HANDLE_INVALID,
               "owner table already exist");

    snprintf(name, sizeof(name), "%s_owner", rdef->name);
    for (p = name; (c = *p);  p++) {
        if (!isascii(c) || (!isalnum(c) && c != '_'))
            *p = '_';
    }

    j = MQI_DIMENSION(base_coldefs) - 1;
    memcpy(coldefs, base_coldefs, j * sizeof(mqi_column_def_t));

    for (i = 0;  i < rdef->nattr && j < MQI_COLUMN_MAX;  i++, j++) {
        col = coldefs + j;
        atd = rdef->attrdefs + i;

        col->name   = atd->name;
        col->type   = atd->type;
        col->length = (col->type == mqi_string) ? NAME_LENGTH : 0;
        col->flags  = 0;
    }

    memset(coldefs + j, 0, sizeof(mqi_column_def_t));

    table = MQI_CREATE_TABLE(name, MQI_TEMPORARY, coldefs, indexdef);

    if (table == MQI_HANDLE_INVALID) {
        mrp_log_error("Can't create table '%s': %s", name, strerror(errno));
        return -1;
    }

    owner_tables[rdef->id] = table;

    return 0;
}

void mrp_resource_owner_recalc(uint32_t zoneid)
{
    mrp_resource_owner_update_zone(zoneid, NULL, 0);
}

void mrp_resource_owner_update_zone(uint32_t zoneid,
                                    mrp_resource_set_t *reqset,
                                    uint32_t reqid)
{
    typedef struct {
        uint32_t replyid;
        mrp_resource_set_t *rset;
        bool move;
    } event_t;

    mrp_resource_owner_t oldowners[MRP_RESOURCE_MAX];
    mrp_resource_owner_t backup[MRP_RESOURCE_MAX];
    mrp_zone_t *zone;
    mrp_application_class_t *class;
    mrp_resource_set_t *rset;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;
    mrp_resource_mgr_ftbl_t *ftbl;
    mrp_resource_owner_t *owner, *old, *owners;
    mrp_resource_mask_t mask;
    mrp_resource_mask_t mandatory;
    mrp_resource_mask_t grant;
    mrp_resource_mask_t advice;
    void *clc, *rsc, *rc;
    uint32_t rid;
    uint32_t rcnt;
    bool force_release;
    bool changed;
    bool move;
    mrp_resource_event_t notify;
    uint32_t replyid;
    uint32_t nevent, maxev;
    event_t *events, *ev, *lastev;

    MRP_ASSERT(zoneid < MRP_ZONE_MAX, "invalid argument");

    zone = mrp_zone_find_by_id(zoneid);

    MRP_ASSERT(zone, "zone is not defined");

    if (!(maxev = mrp_get_resource_set_count()))
        return;

    nevent = 0;
    events = mrp_alloc(sizeof(event_t) * maxev);

    MRP_ASSERT(events, "Memory alloc failure. Can't update zone");

    reset_owners(zoneid, oldowners);
    manager_start_transaction(zone);

    rcnt = mrp_resource_definition_count();
    clc  = NULL;

    while ((class = mrp_application_class_iterate_classes(&clc))) {
        rsc = NULL;

        while ((rset=mrp_application_class_iterate_rsets(class,zoneid,&rsc))) {
            force_release = false;
            mandatory = rset->resource.mask.mandatory;
            grant = 0;
            advice = 0;
            rc = NULL;

            switch (rset->state) {

            case mrp_resource_acquire:
                while ((res = mrp_resource_set_iterate_resources(rset, &rc))) {
                    rdef  = res->def;
                    rid   = rdef->id;
                    owner = get_owner(zoneid, rid);

                    backup[rid] = *owner;

                    if (grant_ownership(owner, zone, class, rset, res))
                        grant |= ((mrp_resource_mask_t)1 << rid);
                    else {
                        if (owner->rset != rset)
                            force_release |= owner->modal;
                    }
                }
                owners = get_owner(zoneid, 0);
                if ((grant & mandatory) == mandatory &&
                    mrp_resource_lua_veto(zone, rset, owners, grant, reqset))
                {
                    advice = grant;
                }
                else {
                    /* rollback, ie. restore the backed up state */
                    rc = NULL;
                    while ((res=mrp_resource_set_iterate_resources(rset,&rc))){
                        rdef = res->def;
                        rid = rdef->id;
                        mask = (mrp_resource_mask_t)1 << rid;
                        owner = get_owner(zoneid, rid);
                        *owner = backup[rid];

                        if ((grant & mask)) {
                            if ((ftbl = rdef->manager.ftbl) && ftbl->free)
                                ftbl->free(zone, res, rdef->manager.userdata);
                        }

                        if (advice_ownership(owner, zone, class, rset, res))
                            advice |= mask;
                    }

                    grant = 0;

                    if ((advice & mandatory) != mandatory)
                        advice = 0;

                    mrp_resource_lua_set_owners(zone, owners);
                }
                break;

            case mrp_resource_release:
                while ((res = mrp_resource_set_iterate_resources(rset, &rc))) {
                    rdef  = res->def;
                    rid   = rdef->id;
                    owner = get_owner(zoneid, rid);

                    if (advice_ownership(owner, zone, class, rset, res))
                        advice |= ((mrp_resource_mask_t)1 << rid);
                }
                if ((advice & mandatory) != mandatory)
                    advice = 0;
                break;

            default:
                break;
            }

            changed = false;
            move    = false;
            notify  = 0;
            replyid = (reqset == rset && reqid == rset->request.id) ? reqid:0;


            if (force_release) {
                move = (rset->state != mrp_resource_release);
                notify = move ? MRP_RESOURCE_EVENT_RELEASE : 0;
                changed = move || rset->resource.mask.grant;
                rset->state = mrp_resource_release;
                rset->resource.mask.grant = 0;
            }
            else {
                if (grant == rset->resource.mask.grant) {
                    if (rset->state == mrp_resource_acquire &&
                        !grant && rset->dont_wait.current)
                    {
                        rset->state = mrp_resource_release;
                        rset->dont_wait.current = rset->dont_wait.client;

                        notify = MRP_RESOURCE_EVENT_RELEASE;
                        move = true;
                    }
                }
                else {
                    rset->resource.mask.grant = grant;
                    changed = true;

                    if (rset->state != mrp_resource_release &&
                        !grant && rset->auto_release.current)
                    {
                        rset->state = mrp_resource_release;
                        rset->auto_release.current = rset->auto_release.client;

                        notify = MRP_RESOURCE_EVENT_RELEASE;
                        move = true;
                    }
                }
            }

            if (notify) {
                mrp_resource_set_notify(rset, notify);
            }

            if (advice != rset->resource.mask.advice) {
                rset->resource.mask.advice = advice;
                changed = true;
            }

            if (replyid || changed) {
                ev = events + nevent++;

                ev->replyid = replyid;
                ev->rset    = rset;
                ev->move    = move;
            }
        } /* while rset */
    } /* while class */

    manager_end_transaction(zone);

    for (lastev = (ev = events) + nevent;     ev < lastev;     ev++) {
        rset = ev->rset;

        if (ev->move)
            mrp_application_class_move_resource_set(rset);

        mrp_resource_set_updated(rset);

        /* first we send out the revoke/deny events
         * followed by the grants (in the next for loop)
         */
        if (rset->event && !rset->resource.mask.grant)
            rset->event(ev->replyid, rset, rset->user_data);
    }

    for (lastev = (ev = events) + nevent;     ev < lastev;     ev++) {
        rset = ev->rset;

        if (rset->event && rset->resource.mask.grant)
            rset->event(ev->replyid, rset, rset->user_data);
    }

    mrp_free(events);

    for (rid = 0;  rid < rcnt;  rid++) {
        owner = get_owner(zoneid, rid);
        old   = oldowners + rid;

        if (owner->class != old->class ||
            owner->rset  != old->rset  ||
            owner->res   != old->res     )
        {
            if (!owner->res)
               delete_resource_owner(zone,old->res);
            else if (!old->res)
               insert_resource_owner(zone,owner->class,owner->rset,owner->res);
            else
               update_resource_owner(zone,owner->class,owner->rset,owner->res);
        }
    }
}

int mrp_resource_owner_print(char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_zone_t *zone;
    mrp_resource_owner_t *owner;
    mrp_application_class_t *class;
    mrp_resource_set_t *rset;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;
    uint32_t rcnt, rid;
    uint32_t zcnt, zid;
    char *p, *e;

    if (len <= 0)
        return 0;

    MRP_ASSERT(buf, "invalid argument");

    rcnt = mrp_resource_definition_count();
    zcnt = mrp_zone_count();

    e = (p = buf) + len;

    PRINT("Resource owners:\n");

    for (zid = 0;  zid < zcnt;  zid++) {
        zone = mrp_zone_find_by_id(zid);

        if (!zone) {
            PRINT("   Zone %u:\n", zid);
        }
        else {
            PRINT("   Zone %s:", zone->name);
            p += mrp_zone_attribute_print(zone, p, e-p);
            PRINT("\n");
        }

        for (rid = 0;   rid < rcnt;   rid++) {
            if (!(rdef = mrp_resource_definition_find_by_id(rid)))
                continue;

            PRINT("      %-15s: ", rdef->name);

            owner = get_owner(zid, rid);

            if (!(class = owner->class) ||
                !(rset  = owner->rset ) ||
                !(res   = owner->res  )    )
            {
                PRINT("<nobody>");
            }
            else {
                MRP_ASSERT(rdef == res->def, "confused with data structures");

                PRINT("%-15s", class->name);

                p += mrp_resource_attribute_print(res, p, e-p);
            }

            PRINT("\n");
        }
    }

    return p - buf;

#undef PRINT
}


static mrp_resource_owner_t *get_owner(uint32_t zone, uint32_t resid)
{
    MRP_ASSERT(zone < MRP_ZONE_MAX && resid < MRP_RESOURCE_MAX,
               "invalid argument");

    return resource_owners + (zone * MRP_RESOURCE_MAX + resid);
}

static void reset_owners(uint32_t zone, mrp_resource_owner_t *oldowners)
{
    mrp_resource_owner_t *owners = get_owner(zone, 0);
    size_t size = sizeof(mrp_resource_owner_t) * MRP_RESOURCE_MAX;
    size_t i;

    if (oldowners)
        memcpy(oldowners, owners, size);

    memset(owners, 0, size);

    for (i = 0;   i < MRP_RESOURCE_MAX;   i++)
        owners[i].share = true;
}

static bool grant_ownership(mrp_resource_owner_t    *owner,
                            mrp_zone_t              *zone,
                            mrp_application_class_t *class,
                            mrp_resource_set_t      *rset,
                            mrp_resource_t          *res)
{
    mrp_resource_def_t      *rdef = res->def;
    mrp_resource_mgr_ftbl_t *ftbl = rdef->manager.ftbl;
    bool                     set_owner = false;

    /*
      if (forbid_grant())
        return false;
     */

    if (owner->modal)
        return false;

    do { /* not a loop */
        if (!owner->class && !owner->rset) {
            /* nobody owns this, so grab it */
            set_owner = true;
            break;
        }

        if (owner->class == class && owner->rset == rset) {
            /* we happen to already own it */
            break;
        }

        if (rdef->shareable && owner->share) {
            /* OK, someone else owns it but
               the owner is ready to share it with us */
            break;
        }

        return false;

    } while(0);

    if (ftbl && ftbl->allocate) {
        if (!ftbl->allocate(zone, res, rdef->manager.userdata))
            return false;
    }

    if (set_owner) {
        owner->class = class;
        owner->rset  = rset;
        owner->res   = res;
        owner->modal = class->modal;
    }

    owner->share = class->share && res->shared;

    return true;
}

static bool advice_ownership(mrp_resource_owner_t    *owner,
                             mrp_zone_t              *zone,
                             mrp_application_class_t *class,
                             mrp_resource_set_t      *rset,
                             mrp_resource_t          *res)
{
    mrp_resource_def_t      *rdef = res->def;
    mrp_resource_mgr_ftbl_t *ftbl = rdef->manager.ftbl;

    (void)zone;

    /*
      if (forbid_grant())
        return false;
     */

    if (owner->modal)
        return false;

    do { /* not a loop */
        if (!owner->class && !owner->rset)
            /* nobody owns this */
            break;

        if (owner->share)
            /* someone else owns it but it can be shared */
            break;

        if (owner->class == class) {
            if (owner->rset->class.priority == rset->class.priority &&
                    class->order == MRP_RESOURCE_ORDER_LIFO)
                /* same class and resource goes to the last one who asks it */
                break;
        }

        return false;

    } while(0);

    if (ftbl && ftbl->advice) {
        if (!ftbl->advice(zone, res, rdef->manager.userdata))
            return false;
    }

    return true;
}

static void manager_start_transaction(mrp_zone_t *zone)
{
    mrp_resource_def_t *rdef;
    mrp_resource_mgr_ftbl_t *ftbl;
    void *cursor = NULL;

    while ((rdef = mrp_resource_definition_iterate_manager(&cursor))) {
        ftbl = rdef->manager.ftbl;

        MRP_ASSERT(ftbl, "confused with data structures");

        if (ftbl->init)
            ftbl->init(zone, rdef->manager.userdata);
    }
}

static void manager_end_transaction(mrp_zone_t *zone)
{
    mrp_resource_def_t *rdef;
    mrp_resource_mgr_ftbl_t *ftbl;
    void *cursor = NULL;

    while ((rdef = mrp_resource_definition_iterate_manager(&cursor))) {
        ftbl = rdef->manager.ftbl;

        MRP_ASSERT(ftbl, "confused with data structures");

        if (ftbl->commit)
            ftbl->commit(zone, rdef->manager.userdata);
    }
}


static void delete_resource_owner(mrp_zone_t *zone, mrp_resource_t *res)
{
    static uint32_t zone_id;

    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(0), MQI_UNSIGNED_VAR(zone_id) )
    );

    mrp_resource_def_t *rdef;
    int n;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;
    zone_id = zone->id;

    if ((n = MQI_DELETE(owner_tables[rdef->id], where)) != 1)
        mrp_log_error("Could not delete resource owner");
}

static void insert_resource_owner(mrp_zone_t *zone,
                                  mrp_application_class_t *class,
                                  mrp_resource_set_t *rset,
                                  mrp_resource_t *res)
{
    mrp_resource_def_t *rdef = res->def;
    uint32_t i;
    int n;
    owner_row_t row;
    owner_row_t *rows[2];
    mqi_column_desc_t cdsc[FIRST_ATTRIBUTE_IDX + MQI_COLUMN_MAX + 1];

    MRP_ASSERT(FIRST_ATTRIBUTE_IDX + rdef->nattr <= MQI_COLUMN_MAX,
               "too many attributes for a table");

    row.zone_id    = zone->id;
    row.zone_name  = zone->name;
    row.class_name = class->name;
    row.rset_id    = rset->id;
    memcpy(row.attrs, res->attrs, rdef->nattr * sizeof(mrp_attr_value_t));

    i = 0;
    cdsc[i].cindex = ZONE_ID_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, zone_id);

    i++;
    cdsc[i].cindex = ZONE_NAME_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, zone_name);

    i++;
    cdsc[i].cindex = CLASS_NAME_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, class_name);

    i++;
    cdsc[i].cindex = RSET_ID_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, rset_id);
    
    set_attr_descriptors(cdsc + (i+1), res);

    rows[0] = &row;
    rows[1] = NULL;

    if ((n = MQI_INSERT_INTO(owner_tables[rdef->id], cdsc, rows)) != 1)
        mrp_log_error("can't insert row into owner table");
}

static void update_resource_owner(mrp_zone_t *zone,
                                  mrp_application_class_t *class,
                                  mrp_resource_set_t *rset,
                                  mrp_resource_t *res)
{
    static uint32_t zone_id;

    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(0), MQI_UNSIGNED_VAR(zone_id) )
    );

    mrp_resource_def_t *rdef = res->def;
    uint32_t i;
    int n;
    owner_row_t row;
    mqi_column_desc_t cdsc[FIRST_ATTRIBUTE_IDX + MQI_COLUMN_MAX + 1];

    zone_id = zone->id;

    MRP_ASSERT(1 + rdef->nattr <= MQI_COLUMN_MAX,
               "too many attributes for a table");

    row.class_name = class->name;
    row.rset_id    = rset->id;
    memcpy(row.attrs, res->attrs, rdef->nattr * sizeof(mrp_attr_value_t));

    i = 0;
    cdsc[i].cindex = CLASS_NAME_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, class_name);

    i++;
    cdsc[i].cindex = RSET_ID_IDX;
    cdsc[i].offset = MQI_OFFSET(owner_row_t, rset_id);

    set_attr_descriptors(cdsc + (i+1), res);


    if ((n = MQI_UPDATE(owner_tables[rdef->id], cdsc, &row, where)) != 1)
        mrp_log_error("can't update row in owner table");
}


static void set_attr_descriptors(mqi_column_desc_t *cdsc, mrp_resource_t *res)
{
    mrp_resource_def_t *rdef = res->def;
    uint32_t i,j;
    int o;

    for (i = j = 0;  j < rdef->nattr;  j++) {
        switch (rdef->attrdefs[j].type) {
        case mqi_string:   o = MQI_OFFSET(owner_row_t,attrs[j].string);  break;
        case mqi_integer:  o = MQI_OFFSET(owner_row_t,attrs[j].integer); break;
        case mqi_unsignd:  o = MQI_OFFSET(owner_row_t,attrs[j].unsignd); break;
        case mqi_floating: o = MQI_OFFSET(owner_row_t,attrs[j].floating);break;
        default:           /* skip this */                            continue;
        }

        cdsc[i].cindex = FIRST_ATTRIBUTE_IDX + j;
        cdsc[i].offset = o;
        i++;
    }

    cdsc[i].cindex = -1;
    cdsc[i].offset =  1;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
