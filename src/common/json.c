/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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
#include <stdarg.h>
#include <string.h>

#include "murphy/config.h"
#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/debug.h>
#include <murphy/common/json.h>

/** Type for a JSON parser. */
typedef struct json_tokener mrp_json_parser_t;

static mrp_json_parser_t *parser;

mrp_json_t *mrp_json_create(mrp_json_type_t type, ...)
{
    mrp_json_t *o;
    const char *s;
    bool        b;
    int         i, l;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        l = va_arg(ap, int);
        if (l < 0)
            o = json_object_new_string(s);
        else
            o = json_object_new_string_len(s, l);
        break;
    case MRP_JSON_BOOLEAN:
        b = va_arg(ap, int);
        o = json_object_new_boolean(b);
        break;
    case MRP_JSON_INTEGER:
        i = va_arg(ap, int);
        o = json_object_new_int(i);
        break;
    case MRP_JSON_DOUBLE:
        d = va_arg(ap, double);
        o = json_object_new_double(d);
        break;
    case MRP_JSON_OBJECT:
        o = json_object_new_object();
        break;
    case MRP_JSON_ARRAY:
        o = json_object_new_array();
        break;
    default:
        o = NULL;
    }
    va_end(ap);

    return o;
}


mrp_json_t *mrp_json_clone(mrp_json_t *o)
{
    if (o != NULL)
        return mrp_json_string_to_object(mrp_json_object_to_string(o), -1);
    else
        return NULL;
}


mrp_json_t *mrp_json_string_to_object(const char *s, int len)
{
    if (parser == NULL) {
        parser = json_tokener_new();

        if (parser == NULL)
            return NULL;
    }
    else
        json_tokener_reset(parser);

    if (len < 0)
        len = strlen(s);

    return json_tokener_parse_ex(parser, s, len);
}


const char *mrp_json_object_to_string(mrp_json_t *o)
{
    if (o != NULL)
        return json_object_to_json_string(o);
    else
        return "{}";
}


mrp_json_t *mrp_json_ref(mrp_json_t *o)
{
    return json_object_get(o);
}


void mrp_json_unref(mrp_json_t *o)
{
    json_object_put(o);
}


mrp_json_type_t mrp_json_get_type(mrp_json_t *o)
{
    return json_object_get_type(o);
}


int mrp_json_is_type(mrp_json_t *o, mrp_json_type_t type)
{
    return json_object_is_type(o, type);
}


void mrp_json_add(mrp_json_t *o, const char *key, mrp_json_t *m)
{
    json_object_object_add(o, key, m);
}


mrp_json_t *mrp_json_add_member(mrp_json_t *o, const char *key,
                                mrp_json_type_t type, ...)
{
    mrp_json_t *m;
    const char *s;
    bool        b;
    int         i, l;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        l = va_arg(ap, int);
        if (l < 0)
            m = json_object_new_string(s);
        else
            m = json_object_new_string_len(s, l);
        break;
    case MRP_JSON_BOOLEAN:
        b = va_arg(ap, int);
        m = json_object_new_boolean(b);
        break;
    case MRP_JSON_INTEGER:
        i = va_arg(ap, int);
        m = json_object_new_int(i);
        break;
    case MRP_JSON_DOUBLE:
        d = va_arg(ap, double);
        m = json_object_new_double(d);
        break;
    case MRP_JSON_OBJECT:
        m = json_object_new_object();
        break;
    case MRP_JSON_ARRAY:
        m = json_object_new_array();
        break;
    default:
        m = NULL;
        errno = EINVAL;
    }
    va_end(ap);

    if (m != NULL)
        json_object_object_add(o, key, m);

    return m;
}


mrp_json_t *mrp_json_add_array(mrp_json_t *o, const char *key,
                               mrp_json_type_t type, ...)
{
    va_list      ap;
    void        *arr;
    size_t       cnt, i;
    mrp_json_t  *a;

    va_start(ap, type);
    arr = va_arg(ap, void *);
    cnt = va_arg(ap, size_t);
    a   = mrp_json_create(MRP_JSON_ARRAY);

    if (a == NULL)
        goto fail;

    switch (type) {
    case MRP_JSON_STRING:
        for (i = 0; i < cnt; i++) {
            if (!mrp_json_array_append_string(a, ((char **)arr)[i]))
                goto fail;
        }
        break;

    case MRP_JSON_INTEGER:
        for (i = 0; i < cnt; i++) {
            if (!mrp_json_array_append_integer(a, ((int *)arr)[i]))
                goto fail;
        }
        break;

    case MRP_JSON_DOUBLE:
        for (i = 0; i < cnt; i++) {
            if (!mrp_json_array_append_double(a, ((double *)arr)[i]))
                goto fail;
        }
        break;

    case MRP_JSON_BOOLEAN:
        for (i = 0; i < cnt; i++) {
            if (!mrp_json_array_append_boolean(a, ((bool *)arr)[i]))
                goto fail;
        }
        break;

    default:
        goto fail;

    }

    va_end(ap);

    mrp_json_add(o, key, a);
    return a;

 fail:
    va_end(ap);
    mrp_json_unref(a);

    return NULL;
}


mrp_json_t *mrp_json_get(mrp_json_t *o, const char *key)
{
    mrp_json_iter_t  it;
    const char      *k;
    mrp_json_t      *v;

    mrp_json_foreach_member(o, k, v, it) {
        if (!strcmp(k, key))
            return v;
    }

    return NULL;
}


int mrp_json_get_member(mrp_json_t *o, const char *key,
                        mrp_json_type_t type, ...)
{
    const char **s;
    bool        *b;
    int         *i;
    double      *d;
    mrp_json_t  *m, **mp;
    int          success;
    va_list      ap;

    success = FALSE;
    va_start(ap, type);

    m = mrp_json_get(o, key);

    if (m != NULL) {
        if (json_object_is_type(m, type)) {
            success = TRUE;
            switch (type) {
            case MRP_JSON_STRING:
                s  = va_arg(ap, const char **);
                *s = json_object_get_string(m);
                break;
            case MRP_JSON_BOOLEAN:
                b  = va_arg(ap, bool *);
                *b = json_object_get_boolean(m);
                break;
            case MRP_JSON_INTEGER:
                i  = va_arg(ap, int *);
                *i = json_object_get_int(m);
                break;
            case MRP_JSON_DOUBLE:
                d  = va_arg(ap, double *);
                *d = json_object_get_double(m);
                break;
            case MRP_JSON_OBJECT:
                mp  = va_arg(ap, mrp_json_t **);
                *mp = m;
                break;
            case MRP_JSON_ARRAY:
                mp  = va_arg(ap, mrp_json_t **);
                *mp = m;
                break;
            default:
                success = FALSE;
            }
        }
        else
            errno = EINVAL;
    }
    else {
        errno = ENOENT;
        success = FALSE;
    }

    va_end(ap);

    return success;
}


void mrp_json_del_member(mrp_json_t *o, const char *key)
{
    json_object_object_del(o, key);
}


int mrp_json_array_length(mrp_json_t *a)
{
    return json_object_array_length(a);
}


int mrp_json_array_append(mrp_json_t *a, mrp_json_t *v)
{
    return json_object_array_add(a, v) == 0;
}


mrp_json_t *mrp_json_array_append_item(mrp_json_t *a, mrp_json_type_t type, ...)
{
    mrp_json_t *v;
    const char *s;
    bool        b;
    int         i, l;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        l = va_arg(ap, int);
        if (l < 0)
            v = json_object_new_string(s);
        else
            v = json_object_new_string_len(s, l);
        break;
    case MRP_JSON_BOOLEAN:
        b = va_arg(ap, int);
        v = json_object_new_boolean(b);
        break;
    case MRP_JSON_INTEGER:
        i = va_arg(ap, int);
        v = json_object_new_int(i);
        break;
    case MRP_JSON_DOUBLE:
        d = va_arg(ap, double);
        v = json_object_new_double(d);
        break;
    case MRP_JSON_OBJECT:
        v = va_arg(ap, mrp_json_t *);
        break;
    case MRP_JSON_ARRAY:
        v = va_arg(ap, mrp_json_t *);
        break;
    default:
        v = NULL;
        errno = EINVAL;
    }
    va_end(ap);

    if (v != NULL) {
        if (json_object_array_add(a, v) == 0)
            return v;
        else {
            mrp_json_unref(v);
            errno = ENOMEM;
        }
    }

    return NULL;
}


int mrp_json_array_set(mrp_json_t *a, int idx, mrp_json_t *v)
{
    return json_object_array_put_idx(a, idx, v);
}


int mrp_json_array_set_item(mrp_json_t *a, int idx, mrp_json_type_t type, ...)
{
    mrp_json_t *v;
    const char *s;
    bool        b;
    int         i, l;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        l = va_arg(ap, int);
        if (l < 0)
            v = json_object_new_string(s);
        else
            v = json_object_new_string_len(s, l);
        break;
    case MRP_JSON_BOOLEAN:
        b = va_arg(ap, int);
        v = json_object_new_boolean(b);
        break;
    case MRP_JSON_INTEGER:
        i = va_arg(ap, int);
        v = json_object_new_int(i);
        break;
    case MRP_JSON_DOUBLE:
        d = va_arg(ap, double);
        v = json_object_new_double(d);
        break;
    case MRP_JSON_OBJECT:
        v = va_arg(ap, mrp_json_t *);
        break;
    case MRP_JSON_ARRAY:
        v = va_arg(ap, mrp_json_t *);
        break;
    default:
        v = NULL;
        errno = EINVAL;
    }
    va_end(ap);

    if (v != NULL)
        return json_object_array_put_idx(a, idx, v);
    else {
        errno = ENOMEM;
        return FALSE;
    }
}


mrp_json_t *mrp_json_array_get(mrp_json_t *a, int idx)
{
    return json_object_array_get_idx(a, idx);
}


int mrp_json_array_get_item(mrp_json_t *a, int idx, mrp_json_type_t type, ...)
{
    const char **s;
    bool        *b;
    int         *i;
    double      *d;
    mrp_json_t  *v, **vp;
    int          success;
    va_list      ap;

    success = FALSE;
    va_start(ap, type);

    v = json_object_array_get_idx(a, idx);

    if (v != NULL) {
        if (json_object_is_type(v, type)) {
            success = TRUE;
            switch (type) {
            case MRP_JSON_STRING:
                s  = va_arg(ap, const char **);
                *s = json_object_get_string(v);
                break;
            case MRP_JSON_BOOLEAN:
                b  = va_arg(ap, bool *);
                *b = json_object_get_boolean(v);
                break;
            case MRP_JSON_INTEGER:
                i  = va_arg(ap, int *);
                *i = json_object_get_int(v);
                break;
            case MRP_JSON_DOUBLE:
                d  = va_arg(ap, double *);
                *d = json_object_get_double(v);
                break;
            case MRP_JSON_OBJECT:
                vp  = va_arg(ap, mrp_json_t **);
                *vp = v;
                break;
            case MRP_JSON_ARRAY:
                vp  = va_arg(ap, mrp_json_t **);
                *vp = v;
                break;
            default:
                success = FALSE;
                errno = EINVAL;
            }
        }
        else
            errno = EINVAL;
    }
    else
        errno = ENOENT;

    va_end(ap);

    return success;
}


int mrp_json_parse_object(char **strp, int *lenp, mrp_json_t **op)
{
    char         *str;
    int           len;
    mrp_json_t   *o   = NULL;
    json_tokener *tok = NULL;
    int           res = -1;

    if (strp == NULL || *strp == NULL) {
        *op = NULL;
        if (lenp != NULL)
            *lenp = 0;

        return 0;
    }

    str = *strp;
    len = lenp ? *lenp : 0;

    if (len <= 0)
        len = strlen(str);

    tok = json_tokener_new();

    if (tok != NULL) {
        o = json_tokener_parse_ex(tok, str, len);

        if (o != NULL) {
            *strp += tok->char_offset;
            if (lenp != NULL)
                *lenp -= tok->char_offset;

            res = 0;
        }
        else {
#ifdef HAVE_JSON_TOKENER_GET_ERROR
            if (json_tokener_get_error(tok) != json_tokener_success)
                errno = EINVAL;
#else
            if (tok->err != json_tokener_success)
                errno = EINVAL;
#endif
            else
                res = 0;
        }

        json_tokener_free(tok);
    }
    else
        errno = ENOMEM;

    *op = o;
    return res;
}
