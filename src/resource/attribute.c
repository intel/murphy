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
#include <stdlib.h>
#include <string.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include "attribute.h"
#include "client-api.h"

static mrp_attr_value_t *get_attr_value_from_list(mrp_attr_t *, const char *,
                                                  mqi_data_type_t);


int mrp_attribute_copy_definitions(mrp_attr_def_t *from, mrp_attr_def_t *to)
{
    mrp_attr_def_t *s, *d;

    MRP_ASSERT(to,"invalid argument");

    if (from) {
        for (s = from, d = to;   s->name;   s++, d++) {

            if (!(d->name = mrp_strdup(s->name)))
                goto no_memory;

            d->access = s->access;

            if ((d->type = s->type) != mqi_string)
                d->value = s->value;
            else {
                if (!(d->value.string = mrp_strdup(s->value.string))) {
                    mrp_free((void *)d->name);
                    memset(d, 0, sizeof(*d));
                    goto no_memory;
                }
            }
        }
    }

    return 0;

 no_memory:
    mrp_log_error("Memory alloc failure. Can't copy attribute definition");
    return -1;
}


mrp_attr_t *mrp_attribute_get_value(uint32_t          idx,
                                    mrp_attr_t       *value,
                                    uint32_t          nattr,
                                    mrp_attr_def_t   *defs,
                                    mrp_attr_value_t *attrs)
{
    mrp_attr_t *vdst;
    mrp_attr_def_t *adef;

    MRP_ASSERT(!nattr || (nattr > 0 && defs && attrs), "invalid argument");
    MRP_ASSERT(idx < nattr, "invalid argument");

    if ((vdst = value) || (vdst = mrp_alloc(sizeof(mrp_attr_t)))) {
        adef = defs + idx;

        if (!(adef->access & MRP_RESOURCE_READ))
            memset(vdst, 0, sizeof(mrp_attr_t));
        else {
            vdst->name  = adef->name;
            vdst->type  = adef->type;
            vdst->value = attrs[idx];
        }
    }

    return vdst;
}


mrp_attr_t *mrp_attribute_get_all_values(uint32_t          nvalue,
                                         mrp_attr_t       *values,
                                         uint32_t          nattr,
                                         mrp_attr_def_t   *defs,
                                         mrp_attr_value_t *attrs)
{
    mrp_attr_def_t *adef;
    mrp_attr_t *vdst, *vend;
    uint32_t i;

    MRP_ASSERT((!nvalue || (nvalue > 0 && values)) &&
               (!nattr  || (nattr  > 0 && defs)),
               "invalid argument");

    if (nvalue)
        nvalue--;
    else {
        for (i = 0;  i < nattr;  i++) {
            if (!attrs || (attrs && (defs[i].access & MRP_RESOURCE_READ)))
                nvalue++;
        }

        if (!(values = mrp_allocz(sizeof(mrp_attr_t) * (nvalue + 1)))) {
            mrp_log_error("Memory alloc failure. Can't get attributes");
            return NULL;
        }
    }

    vend = (vdst = values) + nvalue;

    for (i = 0;     i < nattr && vdst < vend;    i++) {
        adef = defs  + i;

        if (!(adef->access && MRP_RESOURCE_READ))
            continue;

        vdst->name   =  adef->name;
        vdst->type   =  adef->type;
        vdst->value  =  attrs ? attrs[i] : adef->value;

        vdst++;
    }

    memset(vdst, 0, sizeof(*vdst));

    return values;
}

int mrp_attribute_set_values(mrp_attr_t      *values,
                             uint32_t          nattr,
                             mrp_attr_def_t   *defs,
                             mrp_attr_value_t *attrs)
{
    mrp_attr_def_t *adef;
    mrp_attr_value_t *vsrc;
    mrp_attr_value_t *vdst;
    uint32_t i;


    MRP_ASSERT(!nattr || (nattr > 0 && defs && attrs),
               "invalid arguments");

    for (i = 0;  i < nattr;  i++) {
        adef = defs  + i;
        vdst = attrs + i;

        if (!(adef->access & MRP_RESOURCE_WRITE) ||
            !(vsrc = get_attr_value_from_list(values, adef->name, adef->type)))
            vsrc = &adef->value; /* default value */

        if (adef->type !=  mqi_string)
            *vdst = *vsrc;
        else if (vdst->string != vsrc->string) {
            /* if the string is not the same, change it */
            mrp_free((void *)vdst->string);
            if (!(vdst->string = mrp_strdup(vsrc->string)))
                return -1;
        }
    }

    return 0;
}


int mrp_attribute_print(uint32_t          nattr,
                        mrp_attr_def_t   *adefs,
                        mrp_attr_value_t *avals,
                        char             *buf,
                        int               len)
{
#define PRINT(fmt, args...)  if (p<e) { p += snprintf(p, e-p, fmt , ##args); }

    mrp_attr_def_t *adef;
    mrp_attr_value_t *aval;
    uint32_t i;
    char *p, *e;

    if (len <= 0)
        return 0;

    MRP_ASSERT(adefs && avals && buf, "invalid argument");

    e = (p = buf) + len;

    for (i = 0;  i < nattr;  i++) {
        adef = adefs + i;
        aval = avals + i;

        PRINT(" %s:", adef->name);

        switch (adef->type) {
        case mqi_string:    PRINT("'%s'", aval->string  );   break;
        case mqi_integer:   PRINT("%d"  , aval->integer );   break;
        case mqi_unsignd:   PRINT("%u"  , aval->unsignd );   break;
        case mqi_floating:  PRINT("%lf" , aval->floating);   break;
        default:            PRINT(" <unsupported type>" );   break;
        }

    }

    return p - buf;

#undef PRINT
}


static mrp_attr_value_t *get_attr_value_from_list(mrp_attr_t     *list,
                                                  const char     *name,
                                                  mqi_data_type_t type)
{
    mrp_attr_t *attr;

    MRP_ASSERT(name, "invalid argument");

    if (list) {
        for (attr = list;   attr->name;   attr++) {
            if (!strcasecmp(name, attr->name) && type == attr->type)
                return &attr->value;
        }
    }

    return NULL;
}

void mrp_resource_set_free_attribute(mrp_attr_t *attr)
{
    if (!attr)
        return;

    mrp_free(attr);
}

mrp_attr_t *mrp_resource_set_get_attribute_by_name(
        mrp_resource_set_t *resource_set, const char *resource_name,
        const char *attribute_name)
{
    mrp_attr_t *attr = NULL, *attrs;
    uint32_t res_id;
    mrp_attr_t attr_buf[128];
    uint32_t attr_idx = 0;

    memset(attr_buf, 0, sizeof(attr_buf));

    res_id = mrp_resource_definition_get_resource_id_by_name(resource_name);
    attrs = mrp_resource_definition_read_all_attributes(res_id, 128, attr_buf);

    if (!attrs)
        return NULL;

    while (attrs->name != NULL) {
        if (strcmp(attrs->name, attribute_name) == 0) {

            mrp_attr_t *buf = mrp_allocz(sizeof(mrp_attr_t));
            mrp_resource_set_read_attribute(resource_set, resource_name,
                    attr_idx, buf);

            attr = buf;

            break;
        }
        attr_idx++;
        attrs++;
    }

    return attr;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
