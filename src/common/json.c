#include <stdarg.h>
#include <json/json.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/json.h>


mrp_json_t *mrp_json_create(mrp_json_type_t type, ...)
{
    mrp_json_t *o;
    const char *s;
    bool        b;
    int         i;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        o = json_object_new_string(s);
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


mrp_json_t *mrp_json_string_to_object(const char *s)
{
    return json_object_new_string(s);
}


const char *mrp_json_object_to_string(mrp_json_t *o)
{
    return json_object_to_json_string(o);
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
    int         i;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        m = json_object_new_string(s);
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
    }
    va_end(ap);

    if (m != NULL)
        json_object_object_add(o, key, m);

    return m;
}


mrp_json_t *mrp_json_get(mrp_json_t *o, const char *key)
{
    return json_object_object_get(o, key);
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

    va_start(ap, type);

    m = json_object_object_get(o, key);

    if (m != NULL && json_object_is_type(m, type)) {
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
        success = FALSE;

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
    return json_object_array_add(a, v);
}


mrp_json_t *mrp_json_array_append_item(mrp_json_t *a, mrp_json_type_t type, ...)
{
    mrp_json_t *v;
    const char *s;
    bool        b;
    int         i;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        v = json_object_new_string(s);
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
    }
    va_end(ap);

    if (v != NULL)
        json_object_array_add(a, v);

    return v;
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
    int         i;
    double      d;
    va_list     ap;

    va_start(ap, type);
    switch (type) {
    case MRP_JSON_STRING:
        s = va_arg(ap, const char *);
        v = json_object_new_string(s);
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
    }
    va_end(ap);

    if (v != NULL)
        return json_object_array_put_idx(a, idx, v);
    else
        return FALSE;
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

    va_start(ap, type);

    v = json_object_array_get_idx(a, idx);

    if (v != NULL && json_object_is_type(v, type)) {
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
        }
    }
    else
        success = FALSE;

    va_end(ap);

    return success;
}
