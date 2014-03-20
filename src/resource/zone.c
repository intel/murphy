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
#include <murphy/common/log.h>

#include <murphy/resource/manager-api.h>
#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>

#include <murphy-db/mqi.h>

#include "zone.h"


#define ATTRIBUTE_MAX  32
#define NAME_LENGTH    24

#define VALID_TYPE(t) ((t) == mqi_string  || \
                       (t) == mqi_integer || \
                       (t) == mqi_unsignd || \
                       (t) == mqi_floating  )


#define ZONE_ID_IDX          0
#define ZONE_NAME_IDX        1
#define FIRST_ATTRIBUTE_IDX  2

typedef struct {
    uint32_t          zone_id;
    const char       *zone_name;
    mrp_attr_value_t  attrs[MQI_COLUMN_MAX];
} zone_row_t;

static mrp_zone_def_t *zone_def;
static uint32_t        zone_count;
static mrp_zone_t     *zone_table[MRP_ZONE_MAX];
static mqi_handle_t    db_table = MQI_HANDLE_INVALID;

static mqi_handle_t create_zone_table(mrp_zone_def_t *);
static void insert_into_zone_table(mrp_zone_t *);
static void set_attr_descriptors(mqi_column_desc_t *);


int mrp_zone_definition_create(mrp_attr_def_t *attrdefs)
{
    uint32_t nattr;
    size_t size;
    mrp_zone_def_t *def;

    for (nattr = 0;  attrdefs && attrdefs[nattr].name;  nattr++)
        ;

    size = sizeof(mrp_zone_def_t) + sizeof(mrp_attr_def_t) * nattr;

    if (!(def = mrp_allocz(size))) {
        mrp_log_error("Memory alloc failure. Can't create zone definition");
        return -1;
    }

    def->nattr = nattr;
    zone_def = def;

    if (mrp_attribute_copy_definitions(attrdefs, def->attrdefs) < 0)
        return -1;

    db_table = create_zone_table(def);

    return 0;
}

uint32_t mrp_zone_count(void)
{
    return zone_count;
}

uint32_t mrp_zone_create(const char *name, mrp_attr_t *attrs)
{
    size_t size;
    mrp_zone_t *zone;
    const char *dup_name;
    int sts;

    MRP_ASSERT(name, "invalid argument");

    if (!zone_def) {
        mrp_log_error("Zone definition must preceed zone creation. "
                      "can't create zone '%s'", name);
        return MRP_ZONE_ID_INVALID;
    }

    if (zone_count >= MRP_ZONE_MAX) {
        mrp_log_error("Zone table overflow. Can't create zone '%s'", name);
        return MRP_ZONE_ID_INVALID;
    }

    size = sizeof(mrp_zone_t) + sizeof(mrp_attr_def_t) * zone_def->nattr;

    if (!(zone = mrp_allocz(size)) || !(dup_name = mrp_strdup(name))) {
        mrp_log_error("Memory alloc failure. Can't create zone '%s'", name);
        return MRP_ZONE_ID_INVALID;
    }


    zone->id   = zone_count++;
    zone->name = dup_name;

    sts = mrp_attribute_set_values(attrs, zone_def->nattr,
                                   zone_def->attrdefs, zone->attrs);
    if (sts < 0) {
        mrp_log_error("Memory alloc failure. Can't create zone '%s'", name);
        return MRP_ZONE_ID_INVALID;
    }

    insert_into_zone_table(zone);

    zone_table[zone->id] = zone;

    return zone->id;
}

mrp_zone_t *mrp_zone_find_by_id(uint32_t id)
{
    if (id < zone_count)
        return zone_table[id];

    return NULL;
}

mrp_zone_t *mrp_zone_find_by_name(const char *name)
{
    mrp_zone_t *zone;
    uint32_t id;

    for (id = 0;  id < zone_count;  id++) {
        zone = zone_table[id];

        if (!strcasecmp(name, zone->name))
            return zone;
    }

    return NULL;
}

uint32_t mrp_zone_get_id(mrp_zone_t *zone)
{
    if (!zone)
        return MRP_ZONE_ID_INVALID;

    return zone->id;
}

const char *mrp_zone_get_name(mrp_zone_t *zone)
{
    if (!zone || !zone->name)
        return "<unknown zone>";

    return zone->name;
}

const char **mrp_zone_get_all_names(uint32_t buflen, const char **buf)
{
    uint32_t i;

    MRP_ASSERT(!buf || (buf && buflen > 1), "invlaid argument");

    if (buf) {
        if (buflen < zone_count + 1)
            return NULL;
    }
    else {
        buflen = zone_count + 1;
        if (!(buf = mrp_allocz(sizeof(const char *) * buflen))) {
            mrp_log_error("Memory alloc failure. Can't get all zone names");
            return NULL;
        }
    }

    for (i = 0;  i < zone_count;  i++)
        buf[i] = zone_table[i]->name;

    buf[i] = NULL;

    return buf;
}


mrp_attr_t *mrp_zone_read_attribute(mrp_zone_t *zone,
                                    uint32_t    idx,
                                    mrp_attr_t *value)
{
    mrp_attr_t *retval;

    MRP_ASSERT(zone, "invalid argument");
    MRP_ASSERT(zone_def, "no zone definition");

    retval = mrp_attribute_get_value(idx, value, zone_def->nattr,
                                     zone_def->attrdefs, zone->attrs);

    if (!retval) {
        mrp_log_error("Memory alloc failure. Can't get "
                      "zone '%s' attribute %u", zone->name, idx);
    }

    return retval;
}

mrp_attr_t *mrp_zone_read_all_attributes(mrp_zone_t *zone,
                                         uint32_t nvalue,
                                         mrp_attr_t *values)
{
    mrp_attr_t *retval;

    MRP_ASSERT(zone, "invalid argument");

    retval = mrp_attribute_get_all_values(nvalue, values, zone_def->nattr,
                                          zone_def->attrdefs, zone->attrs);

    if (!retval) {
        mrp_log_error("Memory alloc failure. Can't get all"
                      "attributes of zone '%s'", zone->name);
    }

    return retval;
}

int mrp_zone_attribute_print(mrp_zone_t *zone, char *buf, int len)
{
    if (len <= 0)
        return 0;

    MRP_ASSERT(zone && buf, "invalid argument");

    return mrp_attribute_print(zone_def->nattr, zone_def->attrdefs,
                               zone->attrs, buf,len);
}


static mqi_handle_t create_zone_table(mrp_zone_def_t *zdef)
{
    MQI_COLUMN_DEFINITION_LIST(base_coldefs,
        MQI_COLUMN_DEFINITION( "zone_id"       , MQI_UNSIGNED             ),
        MQI_COLUMN_DEFINITION( "zone_name"     , MQI_VARCHAR(NAME_LENGTH) )
    );

    MQI_INDEX_DEFINITION(indexdef,
        MQI_INDEX_COLUMN("zone_id")
    );

    char *name;
    mqi_column_def_t coldefs[MQI_COLUMN_MAX + 1];
    mqi_column_def_t *col;
    mrp_attr_def_t *atd;
    mqi_handle_t table;
    size_t i,j;

    MRP_ASSERT(zdef, "invalid argument");
    MRP_ASSERT(zdef->nattr < MQI_COLUMN_MAX, "too many zone attributes");
    MRP_ASSERT(db_table == MQI_HANDLE_INVALID,
               "multiple definition of zone data table");

    mqi_open();

    name = "zones";

    j = MQI_DIMENSION(base_coldefs) - 1;
    memcpy(coldefs, base_coldefs, j * sizeof(mqi_column_def_t));

    for (i = 0;  i < zdef->nattr && j < MQI_COLUMN_MAX;  i++, j++) {
        col = coldefs + j;
        atd = zdef->attrdefs + i;

        col->name   = atd->name;
        col->type   = atd->type;
        col->length = (col->type == mqi_string) ? NAME_LENGTH : 0;
        col->flags  = 0;
    }

    memset(coldefs + j, 0, sizeof(mqi_column_def_t));

    table = MQI_CREATE_TABLE(name, MQI_TEMPORARY, coldefs, indexdef);

    if (table == MQI_HANDLE_INVALID)
        mrp_log_error("Can't create table '%s': %s", name, strerror(errno));

    return table;
}

static void insert_into_zone_table(mrp_zone_t *zone)
{
    uint32_t i;
    int n;
    zone_row_t row;
    zone_row_t *rows[2];
    mqi_column_desc_t cdsc[FIRST_ATTRIBUTE_IDX + MQI_COLUMN_MAX + 1];

    MRP_ASSERT(zone_def, "no zone definition");
    MRP_ASSERT(db_table != MQI_HANDLE_INVALID, "no zone table");
    MRP_ASSERT(FIRST_ATTRIBUTE_IDX + zone_def->nattr <= MQI_COLUMN_MAX,
               "too many attributes for a table");

    row.zone_id    = zone->id;
    row.zone_name  = zone->name;
    memcpy(row.attrs, zone->attrs, zone_def->nattr * sizeof(mrp_attr_value_t));

    i = 0;
    cdsc[i].cindex = ZONE_ID_IDX;
    cdsc[i].offset = MQI_OFFSET(zone_row_t, zone_id);

    i++;
    cdsc[i].cindex = ZONE_NAME_IDX;
    cdsc[i].offset = MQI_OFFSET(zone_row_t, zone_name);

    set_attr_descriptors(cdsc + (i+1));

    rows[0] = &row;
    rows[1] = NULL;

    if ((n = MQI_INSERT_INTO(db_table, cdsc, rows)) != 1)
        mrp_log_error("can't insert row into zone table");
}

static void set_attr_descriptors(mqi_column_desc_t *cdsc)
{
    uint32_t i,j;
    int o;

    for (i = j = 0;  j < zone_def->nattr;  j++) {
        switch (zone_def->attrdefs[j].type) {
        case mqi_string:   o = MQI_OFFSET(zone_row_t,attrs[j].string);   break;
        case mqi_integer:  o = MQI_OFFSET(zone_row_t,attrs[j].integer);  break;
        case mqi_unsignd:  o = MQI_OFFSET(zone_row_t,attrs[j].unsignd);  break;
        case mqi_floating: o = MQI_OFFSET(zone_row_t,attrs[j].floating); break;
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
