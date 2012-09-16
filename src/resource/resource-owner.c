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

#include <murphy/resource/config-api.h>

#include "resource-owner.h"
#include "resource-class.h"
#include "resource-set.h"
#include "resource.h"
#include "zone.h"


#define RESOURCE_MAX   (sizeof(mrp_resource_mask_t) * 8)

static mrp_resource_owner_t  resource_owners[MRP_ZONE_MAX * RESOURCE_MAX];

static mrp_resource_owner_t *get_owner(uint32_t, uint32_t);
static void reset_owners(uint32_t, mrp_resource_owner_t *);
static bool grant_ownership(mrp_resource_owner_t *, mrp_zone_t *,
                            mrp_resource_class_t *, mrp_resource_set_t *,
                            mrp_resource_t *);
static bool advice_ownership(mrp_resource_owner_t *, mrp_zone_t *,
                             mrp_resource_class_t *, mrp_resource_set_t *,
                             mrp_resource_t *);

static void manager_start_transaction(mrp_zone_t *);
static void manager_end_transaction(mrp_zone_t *);



void mrp_resource_owner_update_zone(uint32_t zoneid)
{
    mrp_resource_owner_t oldowners[RESOURCE_MAX];
    mrp_resource_owner_t backup[RESOURCE_MAX];
    mrp_zone_t *zone;
    mrp_resource_class_t *class;
    mrp_resource_set_t *rset;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;
    mrp_resource_mgr_ftbl_t *ftbl;
    mrp_resource_owner_t *owner, *old;
    mrp_resource_mask_t mask;
    mrp_resource_mask_t mandatory;
    mrp_resource_mask_t grant;
    mrp_resource_mask_t advice;
    void *clc, *rsc, *rc;
    uint32_t rid;
    uint32_t rcnt;

    MRP_ASSERT(zoneid < MRP_ZONE_MAX, "invalid argument");

    zone = mrp_zone_find_by_id(zoneid);

    MRP_ASSERT(zone, "zone is not defined");

    reset_owners(zoneid, oldowners);
    manager_start_transaction(zone);


    rcnt = mrp_resource_definition_count();
    clc  = NULL;

    while ((class = mrp_resource_class_iterate_classes(&clc))) {
        rsc = NULL;

        while ((rset = mrp_resource_class_iterate_rsets(class,zoneid,&rsc))) {
            mandatory = rset->resource.mask.mandatory;
            grant = 0;
            advice = 0;
            rc = NULL;

            switch (rset->request.type) {

            case mrp_resource_acquire:
                while ((res = mrp_resource_set_iterate_resources(rset, &rc))) {
                    rdef  = res->def;
                    rid   = rdef->id;
                    owner = get_owner(zoneid, rid);

                    backup[rid] = *owner;

                    if (grant_ownership(owner, zone, class, rset, res))
                        grant |= ((mrp_resource_mask_t)1 << rid);
                }
                if (mandatory && (grant & mandatory) == mandatory)
                    advice = grant;
                else {
                    /* rollback, ie. restore the backed up state */
                    rc = NULL;
                    while ((res=mrp_resource_set_iterate_resources(rset,&rc))){
                         rdef  = res->def;
                         rid   = rdef->id;
                         mask  = (mrp_resource_mask_t)1 << rid;
                         owner = get_owner(zoneid, rid);
                        *owner = backup[rid];

                        if ((grant & mask)) {
                            if ((ftbl = rdef->manager.ftbl) && ftbl->free)
                                ftbl->free(zone, res, rdef->manager.userdata);
                        }

                        if (advice_ownership(owner, zone, class, rset, res))
                            advice |= mask;
                    }

                    /* nothing is granted */
                    grant = 0;
                }
                break;

            case mrp_resource_release:
                while ((res = mrp_resource_set_iterate_resources(rset, &rc))) {
                    rdef  = res->def;
                    rid   = rdef->id;
                    owner = get_owner(zoneid, rid);

                    if (advice_ownership(owner, zone, class, rset, res))
                        advice |= ((uint32_t)1 << rid);
                }
                if (mandatory && (advice & mandatory) != mandatory)
                    advice = 0;
                break;

            default:
                break;
            }

            if (grant != rset->resource.mask.grant) {
                rset->resource.mask.grant = grant;
            }

            if (advice != rset->resource.mask.advice) {
                rset->resource.mask.advice = advice;
            }

        } /* while rset */
    } /* while class */

    manager_end_transaction(zone);

    for (rid = 0;  rid < rcnt;  rid++) {
        owner = get_owner(zoneid, rid);
        old   = oldowners + rid;

        if (owner->class != old->class ||
            owner->rset  != old->rset  ||
            owner->res   != old->res     )
        {

        }
    }
}

int mrp_resource_owner_print(char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_zone_t *zone;
    mrp_resource_owner_t *owner;
    mrp_resource_class_t *class;
    mrp_resource_set_t *rset;
    mrp_resource_t *res;
    mrp_resource_def_t *rdef;
    uint32_t rcnt, rid;
    uint32_t zcnt, zid;
    char *p, *e;

    MRP_ASSERT(buf && len > 0, "invalid argument");

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
    MRP_ASSERT(zone < MRP_ZONE_MAX && resid < RESOURCE_MAX,"invalid argument");

    return resource_owners + (zone * RESOURCE_MAX + resid);
}

static void reset_owners(uint32_t zone, mrp_resource_owner_t *oldowners)
{
    void   *ptr  = get_owner(zone, 0);
    size_t  size = sizeof(mrp_resource_owner_t) * RESOURCE_MAX;

    if (oldowners)
        memcpy(oldowners, ptr, size);

    memset(ptr, 0, size);
}

static bool grant_ownership(mrp_resource_owner_t *owner,
                            mrp_zone_t           *zone,
                            mrp_resource_class_t *class,
                            mrp_resource_set_t   *rset,
                            mrp_resource_t       *res)
{
    mrp_resource_def_t      *rdef = res->def;
    mrp_resource_mgr_ftbl_t *ftbl = rdef->manager.ftbl;
    bool                     set_owner = false;

    /*
      if (forbid_grant())
        return false;
     */

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

        if (owner->share) {
            /* OK, someone else owns it bu
               the owner is ready to share it with us */
            owner->share = res->shared;
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
        owner->share = res->shared;
    }

    return true;
}

static bool advice_ownership(mrp_resource_owner_t *owner,
                             mrp_zone_t           *zone,
                             mrp_resource_class_t *class,
                             mrp_resource_set_t   *rset,
                             mrp_resource_t       *res)
{
    mrp_resource_def_t      *rdef = res->def;
    mrp_resource_mgr_ftbl_t *ftbl = rdef->manager.ftbl;

    (void)zone;

    /*
      if (forbid_grant())
        return false;
     */

    do { /* not a loop */
        if (!owner->class && !owner->rset)
            /* nobody owns this */
            break;

        if (owner->share)
            /* someone else owns it but it can be shared */
            break;


        if (owner->class == class) {
            if (owner->rset->class.priority == rset->class.priority)
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


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
