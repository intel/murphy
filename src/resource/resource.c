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
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include <murphy-db/mqi.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/manager-api.h>

#include "resource.h"
#include "resource-owner.h"
#include "resource-set.h"
#include "application-class.h"
#include "zone.h"


#define RESOURCE_MAX        (sizeof(mrp_resource_mask_t) * 8)
#define ATTRIBUTE_MAX       (sizeof(mrp_attribute_mask_t) * 8)
#define NAME_LENGTH          24

#define RSETID_IDX           0
#define AUTOREL_IDX          1
#define STATE_IDX            2
#define GRANT_IDX            3
#define FIRST_ATTRIBUTE_IDX  4


#define VALID_TYPE(t) ((t) == mqi_string  || \
                       (t) == mqi_integer || \
                       (t) == mqi_unsignd || \
                       (t) == mqi_floating  )

typedef struct {
    uint32_t          rsetid;
    int32_t           autorel;
    int32_t           state;
    int32_t           grant;
    mrp_attr_value_t  attrs[MQI_COLUMN_MAX];
} user_row_t;


static uint32_t            resource_def_count;
static mrp_resource_def_t *resource_def_table[RESOURCE_MAX];
static MRP_LIST_HOOK(manager_list);
static mqi_handle_t        resource_user_table[RESOURCE_MAX];

static uint32_t add_resource_definition(const char *, bool, uint32_t,
                                        mrp_resource_mgr_ftbl_t *, void *);

#if 0
static uint32_t find_resource_attribute_id(mrp_resource_t *, const char *);

static mqi_data_type_t   get_resource_attribute_value_type(mrp_resource_t *,
                                                           uint32_t);

static mrp_attr_value_t *get_resource_attribute_default_value(mrp_resource_t*,
                                                              uint32_t);
#endif

static int  resource_user_create_table(mrp_resource_def_t *);
static void resource_user_insert(mrp_resource_t *, bool);
static void resource_user_delete(mrp_resource_t *);

static void set_attr_descriptors(mqi_column_desc_t *, mrp_resource_t *);



uint32_t mrp_resource_definition_create(const char *name, bool shareable,
                                        mrp_attr_def_t *attrdefs,
                                        mrp_resource_mgr_ftbl_t *manager,
                                        void *mgrdata)
{
    uint32_t nattr;
    uint32_t id;
    mrp_resource_def_t *def;

    MRP_ASSERT(name, "invalid argument");

    if (mrp_resource_definition_find_by_name(name)) {
        mrp_log_error("attmpt to redefine resource '%s'", name);
        return MRP_RESOURCE_ID_INVALID;
    }

    for (nattr = 0;  attrdefs && attrdefs[nattr].name;  nattr++)
        ;

    id = add_resource_definition(name, shareable, nattr, manager, mgrdata);

    if (id != MRP_RESOURCE_ID_INVALID) {
        def = mrp_resource_definition_find_by_id(id);

        MRP_ASSERT(def, "got confused with data structures");

        if (mrp_attribute_copy_definitions(attrdefs, def->attrdefs) < 0)
            return MRP_RESOURCE_ID_INVALID;

        resource_user_create_table(def);
        mrp_resource_owner_create_database_table(def);
    }

    return id;
}

uint32_t mrp_resource_definition_count(void)
{
    return resource_def_count;
}

mrp_resource_def_t *mrp_resource_definition_find_by_name(const char *name)
{
    mrp_resource_def_t *def;
    uint32_t            i;

    for (i = 0;  i < resource_def_count;  i++) {
        def = resource_def_table[i];

        if (def && !strcasecmp(name, def->name))
            return def;
    }

    return NULL;
}

uint32_t mrp_resource_definition_get_resource_id_by_name(const char *name)
{
    mrp_resource_def_t *def = mrp_resource_definition_find_by_name(name);

    if (!def) {
        return MRP_RESOURCE_ID_INVALID;
    }

    return def->id;
}

mrp_resource_def_t *mrp_resource_definition_find_by_id(uint32_t id)
{
    if (id < resource_def_count)
        return resource_def_table[id];

    return NULL;
}

mrp_resource_def_t *mrp_resource_definition_iterate_manager(void **cursor)
{
    mrp_list_hook_t *entry;

    MRP_ASSERT(cursor, "invalid argument");

    entry = (*cursor == NULL) ? manager_list.next : (mrp_list_hook_t *)*cursor;

    if (entry == &manager_list)
        return NULL;

    *cursor = entry->next;

    return mrp_list_entry(entry, mrp_resource_def_t, manager.list);
}

const char **mrp_resource_definition_get_all_names(uint32_t buflen,
                                                   const char **buf)
{
    uint32_t i;

    MRP_ASSERT(!buf || (buf && buflen > 1), "invlaid argument");

    if (buf) {
        if (buflen < resource_def_count + 1)
            return NULL;
    }
    else {
        buflen = resource_def_count + 1;
        if (!(buf = mrp_allocz(sizeof(const char *) * buflen))) {
            mrp_log_error("Memory alloc failure. Can't get resource names");
            return NULL;
        }
    }

    for (i = 0;  i < resource_def_count;  i++)
        buf[i] = resource_def_table[i]->name;

    buf[i] = NULL;

    return buf;
}

mrp_attr_t *mrp_resource_definition_read_all_attributes(uint32_t resid,
                                                        uint32_t buflen,
                                                        mrp_attr_t *buf)
{
    mrp_resource_def_t *rdef   = mrp_resource_definition_find_by_id(resid);
    mrp_attr_t         *retval;


    if (!rdef)
        retval = mrp_attribute_get_all_values(buflen, buf, 0, NULL, 0);
    else {
        retval = mrp_attribute_get_all_values(buflen, buf, rdef->nattr,
                                              rdef->attrdefs, 0);
    }

    if (!retval) {
        mrp_log_error("Memory alloc failure. Can't get all "
                      "attributes of resource definition");
    }

    return retval;
}



mrp_resource_t *mrp_resource_create(const char *name,
                                    uint32_t    rsetid,
                                    bool        autorel,
                                    bool        shared,
                                    mrp_attr_t *attrs)
{
    mrp_resource_t *res = NULL;
    mrp_resource_def_t *rdef;
    size_t base_size;
    size_t attr_size;
    size_t total_size;
    int sts;

    MRP_ASSERT(name, "invalid argument");

    if (!(rdef = mrp_resource_definition_find_by_name(name))) {
        mrp_log_warning("Can't find resource definition '%s'. "
                        "No resource created", name);
    }
    else {
        base_size  = sizeof(mrp_resource_t);
        attr_size  = sizeof(mrp_attr_value_t) * rdef->nattr;
        total_size = base_size + attr_size;

        if (!(res = mrp_allocz(total_size))) {
            mrp_log_error("Memory alloc failure. Can't create "
                          "resource '%s'", name);
        }
        else {
            mrp_list_init(&res->list);

            res->rsetid = rsetid;
            res->def = rdef;
            res->shared = rdef->shareable ?  shared : false;

            sts = mrp_attribute_set_values(attrs, rdef->nattr,
                                           rdef->attrdefs, res->attrs);
            if (sts < 0) {
                mrp_log_error("Memory alloc failure. No '%s' "
                              "resource created", name);
                return NULL;
            }

            resource_user_insert(res, autorel);
        }
    }

    return res;
}

void mrp_resource_destroy(mrp_resource_t *res)
{
    mrp_resource_def_t *rdef;
    mqi_data_type_t type;
    uint32_t id;

    if (res) {
        rdef = res->def;

        MRP_ASSERT(rdef, "invalid_argument");

        resource_user_delete(res);

        mrp_list_delete(&res->list);

        for (id = 0;  id < rdef->nattr;  id++) {
            type = rdef->attrdefs[id].type;

            if (type == mqi_string)
                mrp_free((void *)res->attrs[id].string);
        }

        mrp_free(res);
    }
}

uint32_t mrp_resource_get_id(mrp_resource_t *res)
{
    mrp_resource_def_t *def;

    if (res) {
        def = res->def;
        MRP_ASSERT(def, "confused with internal data structures");
        return def->id;
    }

    return MRP_RESOURCE_ID_INVALID;
}

const char *mrp_resource_get_name(mrp_resource_t *res)
{
    mrp_resource_def_t *def;

    if (res) {
        def = res->def;

        MRP_ASSERT(def && def->name, "confused with internal data structures");

        return def->name;
    }

    return "<unknown resource>";
}

mrp_resource_mask_t mrp_resource_get_mask(mrp_resource_t *res)
{
    mrp_resource_def_t *def;
    mrp_resource_mask_t mask = 0;

    if (res) {
        def = res->def;

        MRP_ASSERT(def, "confused with internal data structures");

        mask = (mrp_resource_mask_t)1 << def->id;
    }

    return mask;
}

bool mrp_resource_is_shared(mrp_resource_t *res)
{
    if (res)
        return res->shared;

    return false;
}

mrp_attr_t *mrp_resource_read_attribute(mrp_resource_t *res,
                                        uint32_t        idx,
                                        mrp_attr_t     *value)
{
    mrp_attr_t *retval;
    mrp_resource_def_t *rdef;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");

    retval = mrp_attribute_get_value(idx, value, rdef->nattr,
                                     rdef->attrdefs, res->attrs);

    if (!retval) {
        mrp_log_error("Memory alloc failure. Can't get "
                      "resource '%s' attribute %u", rdef->name, idx);
    }

    return retval;
}


mrp_attr_t *mrp_resource_read_all_attributes(mrp_resource_t *res,
                                             uint32_t nvalue,
                                             mrp_attr_t *values)
{
    mrp_attr_t *retval;
    mrp_resource_def_t *rdef;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");

    retval = mrp_attribute_get_all_values(nvalue, values, rdef->nattr,
                                          rdef->attrdefs, res->attrs);

    if (!retval) {
        mrp_log_error("Memory alloc failure. Can't get all "
                      "attributes of resource '%s'", rdef->name);
    }

    return retval;
}

int mrp_resource_write_attributes(mrp_resource_t *res, mrp_attr_t *values)
{
    int sts;
    mrp_resource_def_t *rdef;

    MRP_ASSERT(res && values, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");

    sts = mrp_attribute_set_values(values, rdef->nattr,
                                   rdef->attrdefs, res->attrs);

    if (sts < 0) {
        mrp_log_error("Memory alloc failure. Can't set attributes "
                      "of resource '%s'", rdef->name);
    }

    return sts;
}

const char *mrp_resource_get_application_class(mrp_resource_t *res)
{
    mrp_resource_set_t *rset;
    mrp_application_class_t *class;

    MRP_ASSERT(res, "invalid argument");

    if (!(rset = mrp_resource_set_find_by_id(res->rsetid)))
        return NULL;

    if (!(class = rset->class.ptr))
        return NULL;

    return class->name;
}

void mrp_resource_notify(mrp_resource_t *res,
                         mrp_resource_set_t *rset,
                         mrp_resource_event_t event)
{
    mrp_resource_def_t *rdef;
    mrp_resource_mgr_ftbl_t *ftbl;
    mrp_manager_notify_func_t notify;
    mrp_application_class_t *class;
    mrp_zone_t *zone;

    MRP_ASSERT(res && rset, "inavlid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");

    if ((ftbl = rdef->manager.ftbl) &&
        (notify = ftbl->notify) &&
        (zone = mrp_zone_find_by_id(rset->zone)) &&
        (class = rset->class.ptr))
    {
        notify(event, zone, class, res, rdef->manager.userdata);
    }
}

int mrp_resource_print(mrp_resource_t *res, uint32_t mandatory,
                       size_t indent, char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_resource_def_t *rdef;
    char gap[] = "                         ";
    char *p, *e;
    uint32_t m;

    if (len <= 0)
        return 0;

    MRP_ASSERT(res && indent < sizeof(gap)-1 && buf,
               "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "Confused with data structures");

    gap[indent] = '\0';

    e = (p = buf) + len;
    m = ((mrp_resource_mask_t)1 << rdef->id);

    PRINT("%s%s: 0x%02x %s %s", gap, rdef->name, m,
          (m & mandatory) ? "mandatory":"optional ",
          res->shared ? "shared  ":"exlusive");

    p += mrp_resource_attribute_print(res, p, e-p);

    PRINT("\n");


    return p - buf;

#undef PRINT
}

int mrp_resource_attribute_print(mrp_resource_t *res, char *buf, int len)
{
    mrp_resource_def_t *rdef;

    if (len <= 0)
        return 0;

    MRP_ASSERT(res && buf, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "Confused with data structures");

    return mrp_attribute_print(rdef->nattr,rdef->attrdefs,res->attrs, buf,len);
}


static uint32_t add_resource_definition(const char *name,
                                        bool        shareable,
                                        uint32_t    nattr,
                                        mrp_resource_mgr_ftbl_t *mgrftbl,
                                        void       *mgrdata)
{
    mrp_resource_def_t *def;
    const char         *dup_name;
    size_t              size;
    uint32_t            id;

    MRP_ASSERT(name && nattr < ATTRIBUTE_MAX, "invalid argument");

    if (resource_def_count >= RESOURCE_MAX) {
        mrp_log_error("Resource table overflow. Can't add resource '%s'",name);
        return MRP_RESOURCE_ID_INVALID;
    }

    size = sizeof(mrp_resource_def_t) + sizeof(mrp_attr_def_t) * nattr;

    if (!(def = mrp_allocz(size)) || !(dup_name = mrp_strdup(name))) {
        mrp_log_error("Memory alloc failure. Can't add resource '%s'", name);
        return MRP_RESOURCE_ID_INVALID;
    }

    id = resource_def_count++;

    def->id        = id;
    def->name      = dup_name;
    def->shareable = shareable;
    def->nattr     = nattr;

    if (mgrftbl) {
        def->manager.ftbl = mrp_alloc(sizeof(mrp_resource_mgr_ftbl_t));
        def->manager.userdata = mgrdata;

        if (def->manager.ftbl)
            memcpy(def->manager.ftbl, mgrftbl,sizeof(mrp_resource_mgr_ftbl_t));
        else {
            mrp_log_error("Memory alloc failure. No manager for resource '%s'",
                          name);
        }
    }

    resource_def_table[id] = def;

    if (!mgrftbl)
        mrp_list_init(&def->manager.list);
    else
        mrp_list_append(&manager_list, &def->manager.list);

    return id;
}


#if 0
static uint32_t find_resource_attribute_id(mrp_resource_t *res,
                                           const char *attrnam)
{
    mrp_resource_def_t *rdef;
    mrp_attr_def_t *adef;
    uint32_t id;

    if (res && (rdef = res->def) && attrnam) {
        for (id = 0;  id < rdef->nattr;  id++) {
            adef = rdef->attrdefs + id;

            if (!strcasecmp(attrnam, adef->name))
                return id;
        }
    }

    return MRP_RESOURCE_ID_INVALID;
}

static mqi_data_type_t
get_resource_attribute_value_type(mrp_resource_t *res, uint32_t id)
{
    mrp_resource_def_t *rdef;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");
    MRP_ASSERT(id < rdef->nattr, "invalid argument");

    return rdef->attrdefs[id].type;
}

static mrp_attr_value_t *
get_resource_attribute_default_value(mrp_resource_t *res, uint32_t id)
{
    mrp_resource_def_t *rdef;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;

    MRP_ASSERT(rdef, "confused with data structures");
    MRP_ASSERT(id < rdef->nattr, "invalid argument");

    return &rdef->attrdefs[id].value;
}
#endif


static int resource_user_create_table(mrp_resource_def_t *rdef)
{
    MQI_COLUMN_DEFINITION_LIST(base_coldefs,
        MQI_COLUMN_DEFINITION( "rsetid" , MQI_UNSIGNED ),
        MQI_COLUMN_DEFINITION( "autorel", MQI_INTEGER  ),
        MQI_COLUMN_DEFINITION( "state"  , MQI_INTEGER  ),
        MQI_COLUMN_DEFINITION( "grant"  , MQI_INTEGER  )
    );

    MQI_INDEX_DEFINITION(indexdef,
        MQI_INDEX_COLUMN( "rsetid" )
    );

    static bool initialized = false;

    char name[256];
    mqi_column_def_t  coldefs[MQI_COLUMN_MAX + 1];
    mqi_column_def_t *col;
    mrp_attr_def_t *atd;
    mqi_handle_t table;
    char c, *p;
    size_t i,j;

    if (!initialized) {
        mqi_open();
        for (i = 0;  i < RESOURCE_MAX;  i++)
            resource_user_table[i] = MQI_HANDLE_INVALID;
        initialized = true;
    }

    MRP_ASSERT(sizeof(base_coldefs) < sizeof(coldefs),"too many base columns");
    MRP_ASSERT(rdef, "invalid argument");
    MRP_ASSERT(rdef->id < RESOURCE_MAX, "confused with data structures");
    MRP_ASSERT(resource_user_table[rdef->id] == MQI_HANDLE_INVALID,
               "resource user table already exist");

    snprintf(name, sizeof(name), "%s_users", rdef->name);
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

    resource_user_table[rdef->id] = table;

    return 0;
}

static void resource_user_insert(mrp_resource_t *res, bool autorel)
{
    mrp_resource_def_t *rdef = res->def;
    uint32_t i;
    int n;
    user_row_t row;
    user_row_t *rows[2];
    mqi_column_desc_t cdsc[FIRST_ATTRIBUTE_IDX + MQI_COLUMN_MAX + 1];

    MRP_ASSERT(FIRST_ATTRIBUTE_IDX + rdef->nattr <= MQI_COLUMN_MAX,
               "too many attributes for a table");

    row.rsetid   = res->rsetid;
    row.autorel  = autorel;
    row.grant    = 0;
    row.state    = mrp_resource_no_request;
    memcpy(row.attrs, res->attrs, rdef->nattr * sizeof(mrp_attr_value_t));

    i = 0;
    cdsc[i].cindex = RSETID_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, rsetid);

    i++;
    cdsc[i].cindex = AUTOREL_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, autorel);

    i++;
    cdsc[i].cindex = STATE_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, state);

    i++;
    cdsc[i].cindex = GRANT_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, grant);

    set_attr_descriptors(cdsc + (i+1), res);

    rows[0] = &row;
    rows[1] = NULL;

    if ((n = MQI_INSERT_INTO(resource_user_table[rdef->id], cdsc, rows)) != 1)
        mrp_log_error("can't insert row into resource user table");
}

static void resource_user_delete(mrp_resource_t *res)
{
    static uint32_t rsetid;

    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(RSETID_IDX), MQI_UNSIGNED_VAR(rsetid) )
    );

    mrp_resource_def_t *rdef;
    int n;

    MRP_ASSERT(res, "invalid argument");

    rdef = res->def;
    rsetid = res->rsetid;

    if ((n = MQI_DELETE(resource_user_table[rdef->id], where)) != 1)
        mrp_log_error("Could not delete resource user");
}

void mrp_resource_user_update(mrp_resource_t *res, int state, bool grant)
{
    static uint32_t rsetid;

    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(RSETID_IDX), MQI_UNSIGNED_VAR(rsetid) )
    );

    mrp_resource_def_t *rdef = res->def;
    uint32_t i;
    int n;
    user_row_t row;
    mqi_column_desc_t cdsc[FIRST_ATTRIBUTE_IDX + MQI_COLUMN_MAX + 1];

    rsetid = res->rsetid;

    MRP_ASSERT(1 + rdef->nattr <= MQI_COLUMN_MAX,
               "too many attributes for a table");

    row.state = state;
    row.grant = grant;
    memcpy(row.attrs, res->attrs, rdef->nattr * sizeof(mrp_attr_value_t));

    i = 0;
    cdsc[i].cindex = STATE_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, state);

    i++;
    cdsc[i].cindex = GRANT_IDX;
    cdsc[i].offset = MQI_OFFSET(user_row_t, grant);

    set_attr_descriptors(cdsc + (i+1), res);

    if ((n = MQI_UPDATE(resource_user_table[rdef->id], cdsc,&row, where)) != 1)
        mrp_log_error("can't update row in resource user table");
}

static void set_attr_descriptors(mqi_column_desc_t *cdsc, mrp_resource_t *res)
{
    mrp_resource_def_t *rdef = res->def;
    uint32_t i,j;
    int o;

    for (i = j = 0;  j < rdef->nattr;  j++) {
        switch (rdef->attrdefs[j].type) {
        case mqi_string:   o = MQI_OFFSET(user_row_t,attrs[j].string);  break;
        case mqi_integer:  o = MQI_OFFSET(user_row_t,attrs[j].integer); break;
        case mqi_unsignd:  o = MQI_OFFSET(user_row_t,attrs[j].unsignd); break;
        case mqi_floating: o = MQI_OFFSET(user_row_t,attrs[j].floating);break;
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
