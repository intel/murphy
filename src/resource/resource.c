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

#include <murphy/resource/resource-api.h>

#include "resource.h"


#define RESOURCE_MAX   (sizeof(mrp_resource_mask_t) * 8)
#define ATTRIBUTE_MAX  32

#define VALID_TYPE(t) ((t) == mqi_string  || \
                       (t) == mqi_integer || \
                       (t) == mqi_unsignd || \
                       (t) == mqi_floating  )



static uint32_t            resource_def_count;
static mrp_resource_def_t *resource_def_table[RESOURCE_MAX];
static MRP_LIST_HOOK(manager_list);

static uint32_t add_resource_definition(const char *, bool, uint32_t,
                                        mrp_resource_mgr_ftbl_t *, void *);

#if 0
static uint32_t find_resource_attribute_id(mrp_resource_t *, const char *);

static mqi_data_type_t   get_resource_attribute_value_type(mrp_resource_t *,
                                                           uint32_t);

static mrp_attr_value_t *get_resource_attribute_default_value(mrp_resource_t*,
                                                              uint32_t);
#endif



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


mrp_resource_t *mrp_resource_create(const char *name,
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

            res->def = rdef;
            res->shared = rdef->shareable ?  shared : false;

            sts = mrp_attribute_set_values(attrs, rdef->nattr,
                                           rdef->attrdefs, res->attrs);
            if (sts < 0) {
                mrp_log_error("Memory alloc failure. No '%s' "
                              "resource created", name);
                return NULL;
            }
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

        mrp_list_delete(&res->list);

        for (id = 0;  id < rdef->nattr;  id++) {
            type = rdef->attrdefs[id].type;

            if (type == mqi_string)
                mrp_free((void *)res->attrs[id].string);
        }

        mrp_free(res);
    }
}


uint32_t mrp_resource_get_mask(mrp_resource_t *res)
{
    mrp_resource_def_t *def;
    uint32_t mask = 0;

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
        mrp_log_error("Memory alloc failure. Can't get all"
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


int mrp_resource_print(mrp_resource_t *res, uint32_t mandatory,
                       size_t indent, char *buf, int len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_resource_def_t *rdef;
    char gap[] = "                         ";
    char *p, *e;
    uint32_t m;

    MRP_ASSERT(res && indent < sizeof(gap)-1 && buf && len > 0,
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

    MRP_ASSERT(res && buf && len > 0, "invalid argument");

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
    def->manager.ftbl = mgrftbl;
    def->manager.userdata = mgrdata;
    def->nattr     = nattr;

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



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
