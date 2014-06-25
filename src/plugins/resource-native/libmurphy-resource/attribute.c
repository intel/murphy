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

#include <errno.h>

#include "resource-api.h"

#include "attribute.h"
#include "string_array.h"

void mrp_attribute_array_free(mrp_res_attribute_t *arr,
        uint32_t dim)
{
    uint32_t i;
    mrp_res_attribute_t *attr;

    if (arr) {
        for (i = 0; i < dim; i++) {
            attr = arr + i;

            mrp_free((void *)attr->name);

            if (attr->type == mrp_string)
                mrp_free((void *)attr->string);
        }
        mrp_free(arr);
    }
}


mrp_res_attribute_t *mrp_attribute_array_dup(uint32_t dim,
                                 mrp_res_attribute_t *arr)
{
    size_t size;
    uint32_t i;
    mrp_res_attribute_t *sattr, *dattr;
    mrp_res_attribute_t *dup;
    int err;

    size = (sizeof(mrp_res_attribute_t) * (dim + 1));

    if (!(dup = mrp_allocz(size))) {
        err = ENOMEM;
        goto failed;
    }

    for (i = 0; i < dim; i++) {
        sattr = arr + i;
        dattr = dup + i;

        if (!(dattr->name = mrp_strdup(sattr->name))) {
            err = ENOMEM;
            goto failed;
        }

        switch ((dattr->type = sattr->type)) {
        case mrp_string:
            if (!(dattr->string = mrp_strdup(sattr->string))) {
                err = ENOMEM;
                goto failed;
            }
            break;
        case mrp_int32:
            dattr->integer = sattr->integer;
            break;
        case mrp_uint32:
            dattr->type = mrp_uint32;
            dattr->unsignd = sattr->unsignd;
            break;
        case mrp_double:
            dattr->type = mrp_double;
            dattr->floating = sattr->floating;
            break;
        default:
            err = EINVAL;
            goto failed;
        }
    }

    return dup;

 failed:
    mrp_attribute_array_free(dup, dim);
    errno = err;
    return NULL;
}

/* public API */

mrp_res_string_array_t * mrp_res_list_attribute_names(
        const mrp_res_resource_t *res)
{
    int i;
    mrp_res_string_array_t *ret;
    mrp_res_context_t *cx = NULL;

    if (!res)
        return NULL;

    cx = res->priv->set->priv->cx;

    if (!cx)
        return NULL;

    ret = mrp_allocz(sizeof(mrp_res_string_array_t));

    if (!ret)
        return NULL;

    ret->num_strings = res->priv->num_attributes;
    ret->strings = mrp_allocz_array(const char *, res->priv->num_attributes);

    if (!ret->strings) {
        mrp_free(ret);
        return NULL;
    }

    for (i = 0; i < res->priv->num_attributes; i++) {
        ret->strings[i] = mrp_strdup(res->priv->attrs[i].name);
        if (!ret->strings[i]) {
            ret->num_strings = i;
            mrp_res_free_string_array(ret);
            return NULL;
        }
    }

    return ret;
}


mrp_res_attribute_t * mrp_res_get_attribute_by_name(
        mrp_res_resource_t *res, const char *name)
{
    int i;

    if (!res)
        return NULL;

    for (i = 0; i < res->priv->num_attributes; i++) {
        if (strcmp(name, res->priv->attrs[i].name) == 0) {
            return &res->priv->attrs[i];
        }
    }

    return NULL;
}


int mrp_res_set_attribute_string(mrp_res_attribute_t *attr,
        const char *value)
{
    char *str;

    if (!attr)
        return -1;

    /* check the attribute type */

    if (attr->type != mrp_string)
        return -1;

    str = mrp_strdup(value);

    if (!str)
        return -1;

    mrp_free((void *) attr->string);
    attr->string = str;

    return 0;
}


int mrp_res_set_attribute_uint(mrp_res_attribute_t *attr,
        uint32_t value)
{
    if (!attr || attr->type != mrp_uint32)
        return -1;

    attr->unsignd = value;

    return 0;
}


int mrp_res_set_attribute_int(mrp_res_attribute_t *attr,
        int32_t value)
{
    if (!attr || attr->type != mrp_int32)
        return -1;

    attr->integer = value;

    return 0;
}


int mrp_res_set_attribute_double(mrp_res_attribute_t *attr,
        double value)
{
    if (!attr || attr->type != mrp_double)
        return -1;

    attr->floating = value;

    return 0;
}
