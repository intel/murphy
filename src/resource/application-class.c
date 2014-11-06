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
#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>
#include <murphy/common/log.h>

#include <murphy/resource/manager-api.h>
#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>

#include <murphy-db/mqi.h>

#include "application-class.h"
#include "resource-set.h"
#include "resource-owner.h"
#include "zone.h"

#define CLASS_MAX        64
#define NAME_LENGTH      24

#define CLASS_NAME_IDX   0
#define PRIORITY_IDX     1


/*
 * sorting key bit layout
 *
 * +---------+----+----+--------+
 * | 31 - 29 | 28 | 27 | 26 - 0 |
 * +---------+----+----+--------+
 *      |      |    |       |
 *      |      |    |       +---- 0x07ffffff stamp of the last request
 *      |      |    +------------ 0x08000000 state (set if acquiring)
 *      |      +----------------- 0x10000000 usage (set if shared)
 *      +------------------------ 0xe0000000 priority (0-7)
 */
#define MASK(b)   (((uint32_t)1 << (b)) - (uint32_t)1)

#define STAMP_SHIFT     0
#define STATE_SHIFT     (STAMP_SHIFT + MRP_KEY_STAMP_BITS)
#define USAGE_SHIFT     (STATE_SHIFT + MRP_KEY_STATE_BITS)
#define PRIORITY_SHIFT  (USAGE_SHIFT + MRP_KEY_USAGE_BITS)

#define STAMP_MASK      MASK(MRP_KEY_STAMP_BITS)
#define STATE_MASK      MASK(MRP_KEY_STATE_BITS)
#define USAGE_MASK      MASK(MRP_KEY_USAGE_BITS)
#define PRIORITY_MASK   MASK(MRP_KEY_PRIORITY_BITS)

#define STAMP_KEY(p)    (((uint32_t)(p) & STAMP_MASK)    << STAMP_SHIFT)
#define STATE_KEY(p)    (((uint32_t)(p) & STATE_MASK)    << STATE_SHIFT)
#define USAGE_KEY(p)    (((uint32_t)(p) & USAGE_MASK)    << USAGE_SHIFT)
#define PRIORITY_KEY(p) (((uint32_t)(p) & PRIORITY_MASK) << PRIORITY_SHIFT)

#define STAMP_MAX       STAMP_MASK

typedef struct {
    const char *class_name;
    uint32_t    priority;
} class_row_t;


static MRP_LIST_HOOK(class_list);
static mrp_htbl_t *name_hash;

static void init_name_hash(void);
static int  add_to_name_hash(mrp_application_class_t *);
#if 0
static void remove_from_name_hash(mrp_application_class_t *);
#endif

static mqi_handle_t get_database_table(void);
static void insert_into_application_class_table(const char *, uint32_t);


mrp_application_class_t *mrp_application_class_create(const char *name,
                                                    uint32_t pri,
                                                    bool modal,
                                                    bool share,
                                                    mrp_resource_order_t order)
{
    mrp_application_class_t *class;
    mrp_list_hook_t *insert_before, *clhook, *n;
    uint32_t zone;

    MRP_ASSERT(name, "invalid argument");

    if (modal && share) {
        mrp_log_error("Class '%s' is both modal and shared. "
                      "Sharing will be disabled", name);
        share = false;
    }

    /* looping through all classes to check the uniqueness of the
       name & priority of the new class and find the insertion point */
    insert_before = &class_list;

    mrp_list_foreach_back(&class_list, clhook, n) {
        class = mrp_list_entry(clhook, mrp_application_class_t, list);

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

    if (!(class = mrp_allocz(sizeof(mrp_application_class_t)))) {
        mrp_log_error("Memory alloc failure. Can't create resource class '%s'",
                      name);
        return NULL;
    }

    class->name = mrp_strdup(name);
    class->priority = pri;
    class->modal = modal;
    class->share = share;
    class->order = order;

    for (zone = 0;  zone < MRP_ZONE_MAX;  zone++)
        mrp_list_init(&class->resource_sets[zone]);

    /* list do not have insert_before function,
       so don't be mislead by the name */
    mrp_list_append(insert_before, &class->list);

    add_to_name_hash(class);

    insert_into_application_class_table(class->name, class->priority);

    return class;
}


mrp_application_class_t *mrp_application_class_find(const char *name)
{
    mrp_application_class_t *class = NULL;

    if (name_hash && name)
        class = mrp_htbl_lookup(name_hash, (void *)name);

    return class;
}

mrp_application_class_t *mrp_application_class_iterate_classes(void **cursor)
{
    mrp_list_hook_t *entry;

    MRP_ASSERT(cursor, "invalid argument");

    entry = (*cursor == NULL) ? class_list.prev : (mrp_list_hook_t *)*cursor;

    if (entry == &class_list)
        return NULL;

    *cursor = entry->prev;

    return mrp_list_entry(entry, mrp_application_class_t, list);
}

mrp_resource_set_t *
mrp_application_class_iterate_rsets(mrp_application_class_t *class,
                                    uint32_t                 zone,
                                    void                   **cursor)
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

const char *mrp_application_class_get_name(mrp_application_class_t *class)
{
    if (!class || !class->name)
        return "<unknown class>";

    return class->name;
}


const char **mrp_application_class_get_all_names(uint32_t buflen,
                                                 const char **buf)
{
    mrp_list_hook_t *entry, *n;
    mrp_application_class_t *class;
    bool freeit = false;
    uint32_t i  = 0;

    MRP_ASSERT(!buf || (buf && buflen > 1), "invalid argument");

    if (!buf) {
        freeit = true;
        buflen = CLASS_MAX;
        if (!(buf = mrp_allocz(sizeof(const char *) * buflen))) {
            mrp_log_error("Memory alloc failure. Can't get class names");
            return NULL;
        }
    }

    mrp_list_foreach(&class_list, entry, n) {
        class = mrp_list_entry(entry, mrp_application_class_t, list);

        if (i >= buflen-1) {
            if (freeit)
                mrp_free(buf);
            return NULL;
        }

        buf[i++] = class->name;
    }

    buf[i] = NULL;

    return buf;
}

uint32_t mrp_application_class_get_priority(mrp_application_class_t *class)
{
    return class ? class->priority : 0;
}

int mrp_application_class_add_resource_set(const char *class_name,
                                           const char *zone_name,
                                           mrp_resource_set_t *rset,
                                           uint32_t reqid)
{
    mrp_application_class_t *class;
    mrp_zone_t *zone;


    MRP_ASSERT(class_name && rset && zone_name, "invalid argument");
    MRP_ASSERT(!rset->class.ptr || !mrp_list_empty(&rset->class.list),
               "attempt to add multiple times the same resource set");

    if (!(class = mrp_application_class_find(class_name)))
        return -1;

    if (!(zone = mrp_zone_find_by_name(zone_name)))
        return -1;

    rset->class.ptr = class;
    rset->zone = mrp_zone_get_id(zone);

    if (rset->state == mrp_resource_acquire)
        mrp_resource_set_acquire(rset, reqid);
    else {
        rset->request.id = reqid;

        if (rset->state == mrp_resource_no_request)
            rset->state = mrp_resource_release;

        mrp_application_class_move_resource_set(rset);

        mrp_resource_set_notify(rset, MRP_RESOURCE_EVENT_CREATED);

        mrp_resource_owner_update_zone(rset->zone, rset, reqid);
    }
    
    return 0;
}

void mrp_application_class_move_resource_set(mrp_resource_set_t *rset)
{
    mrp_application_class_t *class;
    mrp_list_hook_t *list, *lentry, *n, *insert_before;
    mrp_resource_set_t *rentry;
    uint32_t key;
    uint32_t zone;

    MRP_ASSERT(rset, "invalid argument");

    mrp_list_delete(&rset->class.list);

    class = rset->class.ptr;
    zone  = rset->zone;

    list = insert_before = class->resource_sets + zone;
    key  = mrp_application_class_get_sorting_key(rset);

    mrp_list_foreach_back(list, lentry, n) {
        rentry = mrp_list_entry(lentry, mrp_resource_set_t, class.list);

         if (key >= mrp_application_class_get_sorting_key(rentry))
             break;

         insert_before = lentry;
    }

    mrp_list_append(insert_before, &rset->class.list);
}

uint32_t mrp_application_class_get_sorting_key(mrp_resource_set_t *rset)
{
    mrp_application_class_t *class;
    bool     lifo;
    uint32_t rqstamp;
    uint32_t priority;
    uint32_t usage;
    uint32_t state;
    uint32_t stamp;
    uint32_t key;

    MRP_ASSERT(rset, "invalid argument");

    class = rset->class.ptr;
    lifo  = (class->order == MRP_RESOURCE_ORDER_LIFO);

    rqstamp  = rset->request.stamp;

    priority = PRIORITY_KEY(rset->class.priority);
    usage    = USAGE_KEY(rset->resource.share ? 1 : 0);
    state    = STATE_KEY(rset->state == mrp_resource_acquire ? 1 : 0);
    stamp    = STAMP_KEY(lifo ? rqstamp : STAMP_MAX - rqstamp);

    key = priority | usage | state | stamp;

    return key;
}


int mrp_application_class_print(char *buf, int len, bool with_rsets)
{
#define PRINT(fmt, args...) \
    do { if (p<e) { p += snprintf(p, e-p, fmt , ##args); } } while (0)

    mrp_zone_t *zone;
    mrp_application_class_t *class;
    mrp_resource_set_t *rset;
    mrp_list_hook_t *clen, *n;
    mrp_list_hook_t *list, *rsen, *m;
    uint32_t zid;
    char *p, *e;
    int width, l;
    int clcnt, rscnt;

    if (len <= 0)
        return 0;

    MRP_ASSERT(buf, "invalid argument");

    e = (p = buf) + len;
    clcnt = rscnt = 0;
    width = 0;

    if (!with_rsets) {
        mrp_list_foreach(&class_list, clen, n) {
            class = mrp_list_entry(clen, mrp_application_class_t, list);
            if ((l = strlen(class->name)) > width)
                width = l;
        }
    }

    PRINT("Application classes:\n");

    mrp_list_foreach_back(&class_list, clen, n) {
        class = mrp_list_entry(clen, mrp_application_class_t, list);
        clcnt++;

        if (with_rsets)
            PRINT("  %3u - %s ", class->priority, class->name);
        else
            PRINT("   %-*s ", width, class->name);

        if (class->modal)
            PRINT(" modal");
        if (class->share)
            PRINT(" share");

        PRINT("\n");

        if (!with_rsets)
            continue;

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
        cfg.nentry  = CLASS_MAX;
        cfg.comp    = mrp_string_comp;
        cfg.hash    = mrp_string_hash;
        cfg.free    = NULL;
        cfg.nbucket = cfg.nentry / 2;

        name_hash = mrp_htbl_create(&cfg);

        MRP_ASSERT(name_hash, "failed to make name_hash for resource classes");
    }
}


static int add_to_name_hash(mrp_application_class_t *class)
{
    MRP_ASSERT(class && class->name, "invalid argument");

    init_name_hash();

    if (!mrp_htbl_insert(name_hash, (void *)class->name, class))
        return -1;

    return 0;
}

#if 0
static void remove_from_name_hash(mrp_application_class_t *class)
{
    mrp_application_class_t *deleted;

    if (class && class->name && name_hash) {
        deleted = mrp_htbl_remove(name_hash, (void *)class->name, false);

        MRP_ASSERT(!deleted || deleted == class, "confused with data "
                   "structures when deleting resource-class from name hash");

        /* in case we were not compiled with debug enabled */
        if (deleted != class) {
            mrp_log_error("confused with data structures when deleting "
                          "resource-class '%s' from name hash", class->name);
        }
    }
}
#endif


static mqi_handle_t get_database_table(void)
{
    MQI_COLUMN_DEFINITION_LIST(coldefs,
        MQI_COLUMN_DEFINITION( "name"     , MQI_VARCHAR(NAME_LENGTH) ),
        MQI_COLUMN_DEFINITION( "priority" , MQI_UNSIGNED             )
    );

    MQI_INDEX_DEFINITION(indexdef,
        MQI_INDEX_COLUMN("priority")
    );

    static mqi_handle_t  table = MQI_HANDLE_INVALID;
    static char         *name  = "application_classes";

    if (table == MQI_HANDLE_INVALID) {
        mqi_open();

        table = MQI_CREATE_TABLE(name, MQI_TEMPORARY, coldefs, indexdef);

        if (table == MQI_HANDLE_INVALID)
            mrp_log_error("Can't create table '%s': %s", name,strerror(errno));
    }

    return table;
}

static void insert_into_application_class_table(const char *name, uint32_t pri)
{
    MQI_COLUMN_SELECTION_LIST(cols,
        MQI_COLUMN_SELECTOR(CLASS_NAME_IDX, class_row_t, class_name),
        MQI_COLUMN_SELECTOR(PRIORITY_IDX  , class_row_t, priority  )
    );

    class_row_t   row;
    mqi_handle_t  table   = get_database_table();
    class_row_t  *rows[2] = {&row, NULL};

    MRP_ASSERT(name, "invalid argument");
    MRP_ASSERT(table != MQI_HANDLE_INVALID, "database problem");

    row.class_name = name;
    row.priority = pri;

    if (MQI_INSERT_INTO(table, cols, rows) != 1)
        mrp_log_error("Failed to add application class '%s' to database",name);
}



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
