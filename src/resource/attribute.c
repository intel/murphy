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

#include "attribute.h"

/* temporary!!! */
#define mrp_log_warning(fmt, args...) printf(fmt "\n", ## args) 
#define mrp_log_error(fmt, args...) printf(fmt "\n", ## args) 

static mrp_attr_value_t *get_attr_value_from_list(mrp_attr_def_t *,
                                                  const char*,mqi_data_type_t);


int mrp_attribute_copy_definitions(mrp_attr_def_t *from, mrp_attr_def_t *to)
{
    mrp_attr_def_t *s, *d;

    MRP_ASSERT(to,"invalid argument");

    if (from) {
        for (s = from, d = to;   s->name;   s++, d++) {

            if (!(d->name = mrp_strdup(s->name)))
                goto no_memory;
            
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


int mrp_attribute_set_values(mrp_attr_def_t   *values,
                             uint32_t          nattr,
                             mrp_attr_def_t   *defs,
                             mrp_attr_value_t *attrs)
{
    mrp_attr_def_t *adef;
    mrp_attr_value_t *vsrc;
    mrp_attr_value_t *vdst;
    uint32_t i;

    
    for (i = 0;  i < nattr;  i++) {
        adef = defs  + i;
        vdst = attrs + i;

        if (!(vsrc = get_attr_value_from_list(values, adef->name, adef->type)))
            vsrc = &adef->value; /* default value */

        if (adef->type !=  mqi_string)
            *vdst = *vsrc;
        else {
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

    MRP_ASSERT(adefs && avals && buf && len > 0, "invalid argument");

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


static mrp_attr_value_t *get_attr_value_from_list(mrp_attr_def_t *list,
                                                  const char     *name,
                                                  mqi_data_type_t type)
{
    mrp_attr_def_t *adef;

    MRP_ASSERT(name, "invalid argument");

    if (list) {
        for (adef = list;   adef->name;   adef++) {
            if (!strcasecmp(name, adef->name) && type == adef->type)
                return &adef->value;
        }
    }
        
    return NULL;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
