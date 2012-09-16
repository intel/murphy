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

#include <murphy/resource/manager-api.h>
#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>

#include "zone.h"


#define ATTRIBUTE_MAX  32

#define VALID_TYPE(t) ((t) == mqi_string  || \
                       (t) == mqi_integer || \
                       (t) == mqi_unsignd || \
                       (t) == mqi_floating  )


static mrp_zone_def_t *zone_def;
static uint32_t        zone_count;
static mrp_zone_t     *zone_table[MRP_ZONE_MAX];


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
    if (!zone | !zone->name)
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
    else if (!buf) {
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
    MRP_ASSERT(zone && buf && len > 0, "invalid argument");

    return mrp_attribute_print(zone_def->nattr, zone_def->attrdefs,
                               zone->attrs, buf,len);
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
