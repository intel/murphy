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

#include <resource/resource-api.h>

#include "resource-class.h"
#include "resource-set.h"
#include "zone.h"

/* temporary!!! */
#define mrp_log_warning(fmt, args...) printf(fmt "\n" , ##args) 
#define mrp_log_error(fmt, args...) printf(fmt "\n" , ##args) 


/*
 * sorting key bit layout
 * 
 * +---------+----+----+--------+
 * | 31 - 29 | 28 | 27 | 26 - 0 |  
 * +---------+----+----+--------+
 *      |      |    |       |
 *      |      |    |       +---- 0x07ffffff stamp of the last request
 *      |      |    +------------ 0x08000000 request (set if acquiring)
 *      |      +----------------- 0x10000000 usage (set if shared)
 *      +------------------------ 0xe0000000 priority (0-7)
 */
#define MASK(b)   (((uint32_t)1 << (b)) - (uint32_t)1)

#define STAMP_SHIFT     0
#define REQUEST_SHIFT   (STAMP_SHIFT   + MRP_KEY_STAMP_BITS)
#define USAGE_SHIFT     (REQUEST_SHIFT + MRP_KEY_REQUEST_BITS)
#define PRIORITY_SHIFT  (USAGE_SHIFT   + MRP_KEY_USAGE_BITS)

#define STAMP_MASK      MASK(MRP_KEY_STAMP_BITS)
#define REQUEST_MASK    MASK(MRP_KEY_REQUEST_BITS)
#define USAGE_MASK      MASK(MRP_KEY_USAGE_BITS)
#define PRIORITY_MASK   MASK(MRP_KEY_PRIORITY_BITS)

#define STAMP_KEY(p)    (((uint32_t)(p) & STAMP_MASK)    << STAMP_SHIFT)
#define REQUEST_KEY(p)  (((uint32_t)(p) & REQUEST_MASK)  << REQUEST_SHIFT)
#define USAGE_KEY(p)    (((uint32_t)(p) & USAGE_MASK)    << USAGE_SHIFT)
#define PRIORITY_KEY(p) (((uint32_t)(p) & PRIORITY_MASK) << PRIORITY_SHIFT)


static MRP_LIST_HOOK(class_list);
static mrp_htbl_t *name_hash;

static void init_name_hash(void);
static int  add_to_name_hash(mrp_resource_class_t *);
#if 0
static void remove_from_name_hash(mrp_resource_class_t *);
#endif


mrp_resource_class_t *mrp_resource_class_create(const char *name, uint32_t pri)
{
    mrp_resource_class_t *class;
    mrp_list_hook_t *insert_before, *clhook, *n;
    uint32_t zone;

    MRP_ASSERT(name, "invalid argument");


    /* looping through all classes to check the uniqueness of the
       name & priority of the new class and find the insertion point */
    insert_before = &class_list;

    mrp_list_foreach_back(&class_list, clhook, n) {
        class = mrp_list_entry(clhook, mrp_resource_class_t, list);

        if (!strcasecmp(name, class->name)) {
            mrp_log_warning("Multiple definitions for class '%s'", name);
            return NULL;
        }

        if (pri == class->priority) {
            mrp_log_error("Priority clash. Classes '%s' and '%s' would have "
                          "the same priority", name, class->name);
        }

        if (pri < class->priority)
            insert_before = &class->list;
    }

    if (!(class = mrp_allocz(sizeof(mrp_resource_class_t)))) {
        mrp_log_error("Memory alloc failure. Can't create resource class '%s'",
                      name);
        return NULL;
    }

    class->name = mrp_strdup(name);
    class->priority = pri;

    for (zone = 0;  zone < MRP_ZONE_MAX;  zone++)
        mrp_list_init(&class->resource_sets[zone]);

    /* list do not have insert_before function,
       so don't be mislead by the name */
    mrp_list_append(insert_before, &class->list); 

    add_to_name_hash(class);

    return class;
}


mrp_resource_class_t *mrp_resource_class_find(const char *name)
{
    mrp_resource_class_t *class = NULL;

    if (name_hash && name)
        class = mrp_htbl_lookup(name_hash, (void *)name);

    return class;
}

mrp_resource_class_t *mrp_resource_class_iterate_classes(void **cursor)
{
    mrp_list_hook_t *entry;

    MRP_ASSERT(cursor, "invalid argument");

    entry = (*cursor == NULL) ? class_list.prev : (mrp_list_hook_t *)*cursor;

    if (entry == &class_list)
        return NULL;
 
    *cursor = entry->prev;

    return mrp_list_entry(entry, mrp_resource_class_t, list);
}

mrp_resource_set_t *
mrp_resource_class_iterate_rsets(mrp_resource_class_t *class,
                                 uint32_t              zone,
                                 void                **cursor)
{
    mrp_list_hook_t *list, *entry;

    MRP_ASSERT(class && zone < MRP_ZONE_MAX && cursor, "invalid argument");

    list  = class->resource_sets + zone;
    entry = (*cursor == NULL) ? list->prev : (mrp_list_hook_t *)*cursor;

    if (entry == list)
        return NULL;

    *cursor = entry->prev;
    
    return mrp_list_entry(entry, mrp_resource_set_t, class.list);
}


void mrp_resource_class_add_resource_set(mrp_resource_class_t *class,
                                         uint32_t zone,
                                         mrp_resource_set_t *rset)
{
    MRP_ASSERT(class && rset && zone < MRP_ZONE_MAX, "invalid argument");
    MRP_ASSERT(!rset->class.ptr || !mrp_list_empty(&rset->class.list),
               "attempt to add multiple times the same resource set");

    rset->class.ptr = class;
    rset->zone = zone;

    mrp_resource_class_move_resource_set(rset);
}

void mrp_resource_class_move_resource_set(mrp_resource_set_t *rset)
{
    mrp_resource_class_t *class;
    mrp_list_hook_t *list, *lentry, *n, *insert_before;
    mrp_resource_set_t *rentry;
    uint32_t key;
    uint32_t zone;

    MRP_ASSERT(rset, "invalid argument");

    mrp_list_delete(&rset->class.list);

    class = rset->class.ptr;
    zone  = rset->zone;

    list = insert_before = class->resource_sets + zone;
    key  = mrp_resource_class_get_sorting_key(rset);

    mrp_list_foreach_back(list, lentry, n) {
        rentry = mrp_list_entry(lentry, mrp_resource_set_t, class.list);

         if (key >= mrp_resource_class_get_sorting_key(rentry))
             break;

         insert_before = lentry;
    }

    mrp_list_append(insert_before, &rset->class.list);
}

uint32_t mrp_resource_class_get_sorting_key(mrp_resource_set_t *rset)
{
    uint32_t priority;
    uint32_t usage;
    uint32_t request;
    uint32_t stamp;
    uint32_t key;

    MRP_ASSERT(rset, "invalid argument");

    priority = PRIORITY_KEY(rset->class.priority);
    usage    = USAGE_KEY(rset->resource.share ? 1 : 0);
    request  = REQUEST_KEY(rset->request.type == mrp_resource_acquire ? 1 : 0);
    stamp    = STAMP_KEY(rset->request.stamp);

    key = priority | usage | request | stamp;

    return key;
}


int mrp_resource_class_print(char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_zone_t *zone;
    mrp_resource_class_t *class;
    mrp_resource_set_t *rset;
    mrp_list_hook_t *clen, *n;
    mrp_list_hook_t *list, *rsen, *m;
    uint32_t zid;
    char *p, *e;
    int clcnt, rscnt;

    MRP_ASSERT(buf && len > 0, "invalid argument");

    e = (p = buf) + len;
    clcnt = rscnt = 0;

    PRINT("Resource classes:\n");

    mrp_list_foreach_back(&class_list, clen, n) {
        class = mrp_list_entry(clen, mrp_resource_class_t, list);
        PRINT("  %3u - %s\n", class->priority, class->name);

        for (zid = 0;   zid < MRP_ZONE_MAX;   zid++) {
            zone = mrp_zone_find_by_id(zid);
            list = class->resource_sets + zid;

            if (!mrp_list_empty(list)) {
                if (!zone) {
                    PRINT("           Resource-sets in zone %u:\n", zid);
                }
                else {
                    PRINT("           Resource-sets in %s zone:", zone->name);
                    p += mrp_zone_attribute_print(zone, p, e-p);
                    PRINT("\n");
                }

                mrp_list_foreach_back(list, rsen, m) {
                    rset = mrp_list_entry(rsen, mrp_resource_set_t,class.list);
                    p += mrp_resource_set_print(rset, 13, p, e-p);
                }
            }
        }

        clcnt++;
    }

    if (!clcnt)
        PRINT("   <none>\n");

    return p - buf;

#undef PRINT
}


static void init_name_hash(void)
{
    mrp_htbl_config_t  cfg;

    if (!name_hash) {
        cfg.nentry  = 32;
        cfg.comp    = mrp_string_comp;
        cfg.hash    = mrp_string_hash;
        cfg.free    = NULL;
        cfg.nbucket = cfg.nentry / 2;

        name_hash = mrp_htbl_create(&cfg);

        MRP_ASSERT(name_hash, "failed to make name_hash for resource classes");
    }
}


static int add_to_name_hash(mrp_resource_class_t *class)
{
    MRP_ASSERT(class && class->name, "invalid argument");
    
    init_name_hash();

    if (!mrp_htbl_insert(name_hash, (void *)class->name, class))
        return -1;

    return 0;
}

#if 0
static void remove_from_name_hash(mrp_resource_class_t *class)
{
    mrp_resource_class_t *deleted;

    if (class && class->name && name_hash) {
        deleted = mrp_htbl_remove(name_hash, (void *)class->name, false);

        MRP_ASSERT(deleted == class, "confused with data structures when "
                   "deleting resource-class from name hash");

        /* in case we were not compiled with debug enabled */
        if (deleted != class) {
            mrp_log_error("confused with data structures when deleting "
                          "resource-class '%s' from name hash", class->name);
        }
    }
}
#endif

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
