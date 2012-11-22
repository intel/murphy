#include <errno.h>

#include <murphy/resource/protocol.h>
#include "resource-api.h"
#include "resource-private.h"

#define RES_ADDRESS "unxs:/tmp/murphy/resource-native"

#define ARRAY_MAX      1024
#define RESOURCE_MAX   32
#define ATTRIBUTE_MAX  32

#ifndef NO_DEBUG
static void print_resource(murphy_resource *res)
{
    printf("   resource '%s' : %smandatory, %sshared\n",
            res->name, res->priv->mandatory ? " " : "not ", res->priv->shared ? "" : "not ");
}

static void print_resource_set(murphy_resource_set *rset)
{
    int i;
    murphy_resource *res;

    printf("Resource set %i (%s):\n", rset->priv->id, rset->application_class);

    for (i = 0; i < rset->priv->num_resources; i++) {
    res = rset->priv->resources[i];
        print_resource(res);
    }
}
#endif

void *u_to_p(uint32_t u)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint64_t o = u;
#else
    uint32_t o = u;
#endif
#else
    uint32_t o = o;
#endif
    return (void *) o;
}

uint32_t p_to_u(const void *p)
{
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
    uint32_t o = 0;
    uint64_t big = (uint64_t) p;
    o = big & 0xffffffff;
#else
    uint32_t o = (uint32_t) p;
#endif
#else
    uint32_t o = p;
#endif
    return o;
}


int int_comp(const void *key1, const void *key2)
{
    return key1 != key2;
}


uint32_t int_hash(const void *key)
{
    return p_to_u(key);
}


#if 0
static void str_array_free(string_array_t *arr)
{
    uint32_t i;

    if (arr) {
        for (i = 0;  i < arr->dim;  i++)
            mrp_free((void *)arr->elems[i]);

        mrp_free(arr);
    }
}
#endif


static string_array_t *str_array_dup(uint32_t dim, const char **arr)
{
    size_t size;
    uint32_t i;
    string_array_t *dup;

    if (dim >= ARRAY_MAX || !arr)
        return NULL;

    if (!dim && arr) {
        for (dim = 0;  arr[dim];  dim++)
            ;
    }

    size = sizeof(string_array_t) + (sizeof(const char *) * (dim + 1));

    if (!(dup = mrp_allocz(size))) {
        errno = ENOMEM;
        return NULL;
    }

    dup->dim = dim;

    for (i = 0;   i < dim;   i++) {
        if (arr[i]) {
            if (!(dup->elems[i] = mrp_strdup(arr[i]))) {
                errno = ENOMEM;
                /* probably no use for freing anything */
                return NULL;
            }
        }
    }

    return dup;
}

static murphy_string_array *murphy_str_array_dup(uint32_t dim, const char **arr)
{
    uint32_t i;
    murphy_string_array *dup;

    if (dim >= ARRAY_MAX || !arr)
        return NULL;

    if (!dim && arr) {
        for (dim = 0;  arr[dim];  dim++)
            ;
    }

    if (!(dup = mrp_allocz(sizeof(murphy_string_array)))) {
        errno = ENOMEM;
        return NULL;
    }

    dup->num_strings = dim;
    dup->strings = mrp_allocz_array(const char *, dim);

    for (i = 0;   i < dim;   i++) {
        if (arr[i]) {
            if (!(dup->strings[i] = mrp_strdup(arr[i]))) {
                errno = ENOMEM;
                /* probably no use for freing anything */
                return NULL;
            }
        }
    }

    return dup;
}


static bool fetch_resource_name(mrp_msg_t *msg, void **pcursor,
                                const char **pname)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_NAME || type != MRP_MSG_FIELD_STRING)
    {
        *pname = "<unknown>";
        return false;
    }

    *pname = value.str;
    return true;
}

static void attribute_array_free(attribute_array_t *arr)
{
    uint32_t i;
    attribute_t *attr;

    if (arr) {
        for (i = 0; i < arr->dim; i++) {
            attr = arr->elems + i;

            mrp_free((void *)attr->name);

            if (attr->type == 's')
                mrp_free((void *)attr->string);
        }
        mrp_free(arr);
    }
}

static void murphy_attribute_array_free(murphy_resource_attribute *arr, uint32_t dim)
{
    uint32_t i;
    murphy_resource_attribute *attr;

    if (arr) {
        for (i = 0; i < dim; i++) {
            attr = arr + i;

            mrp_free((void *)attr->name);

            if (attr->type == murphy_string)
                mrp_free((void *)attr->string);
        }
        mrp_free(arr);
    }
}

static attribute_array_t *attribute_array_dup(uint32_t dim, attribute_t *arr)
{
    size_t size;
    uint32_t i;
    attribute_t *sattr, *dattr;
    attribute_array_t *dup;
    int err;

    MRP_ASSERT(dim < ARRAY_MAX && arr, "invalid argument");

    if (!dim && arr) {
        for (dim = 0;  arr[dim].name;  dim++)
            ;
    }

    size = sizeof(attribute_array_t) + (sizeof(attribute_t) * (dim + 1));

    if (!(dup = mrp_allocz(size))) {
        err = ENOMEM;
        goto failed;
    }

    dup->dim = dim;

    for (i = 0;    i < dim;    i++) {
        sattr = arr + i;
        dattr = dup->elems + i;

        if (!(dattr->name = mrp_strdup(sattr->name))) {
            err = ENOMEM;
            goto failed;
        }

        switch ((dattr->type = sattr->type)) {
        case 's':
            if (!(dattr->string = mrp_strdup(sattr->string))) {
                err = ENOMEM;
                goto failed;
            }
            break;
        case 'i':
            dattr->integer = sattr->integer;
            break;
        case 'u':
            dattr->unsignd = sattr->unsignd;
            break;
        case 'f':
            dattr->floating = sattr->floating;
            break;
        default:
            errno = EINVAL;
            goto failed;
        }
    }

    return dup;

 failed:
    attribute_array_free(dup);
    errno = err;
    return NULL;
}

static murphy_resource_attribute *murphy_attribute_array_dup(uint32_t dim,
                                 murphy_resource_attribute *arr)
{
    size_t size;
    uint32_t i;
    murphy_resource_attribute *sattr, *dattr;
    murphy_resource_attribute *dup;
    int err;

    size = (sizeof(murphy_resource_attribute) * (dim + 1));

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
        case murphy_string:
            if (!(dattr->string = mrp_strdup(sattr->string))) {
                err = ENOMEM;
                goto failed;
            }
            break;
        case murphy_int32:
            dattr->integer = sattr->integer;
            break;
        case murphy_uint32:
        dattr->type = murphy_uint32;
            dattr->unsignd = sattr->unsignd;
            break;
        case murphy_double:
        dattr->type = murphy_double;
            dattr->floating = sattr->floating;
            break;
        default:
            errno = EINVAL;
            goto failed;
        }
    }

    return dup;

 failed:
    murphy_attribute_array_free(dup, dim);
    errno = err;
    return NULL;
}


static int fetch_attribute_array(mrp_msg_t *msg, void **pcursor,
                                 size_t dim, attribute_t *arr)
{
    attribute_t *attr;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    size_t i;

    i = 0;

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {
        if (tag == RESPROTO_SECTION_END && type == MRP_MSG_FIELD_UINT8)
            break;

        if (tag  != RESPROTO_ATTRIBUTE_NAME ||
            type != MRP_MSG_FIELD_STRING ||
            i >= dim - 1) {
            return -1;
        }

        attr = arr + i++;
        attr->name = value.str;

        if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
            tag != RESPROTO_ATTRIBUTE_VALUE) {
            return -1;
        }

        switch (type) {
        case MRP_MSG_FIELD_STRING:
            attr->type = 's';
            attr->string = value.str;
            break;
        case MRP_MSG_FIELD_SINT32:
            attr->type = 'i';
            attr->integer = value.s32;
            break;
        case MRP_MSG_FIELD_UINT32:
            attr->type = 'u';
            attr->unsignd = value.u32;
            break;
        case MRP_MSG_FIELD_DOUBLE:
            attr->type = 'f';
            attr->floating = value.dbl;
            break;
        default:
            return -1;
        }
    }

    memset(arr + i, 0, sizeof(attribute_t));

    return 0;
}


static bool fetch_resource_set_state(mrp_msg_t *msg, void **pcursor,
                                     mrp_resproto_state_t *pstate)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_STATE || type != MRP_MSG_FIELD_UINT16)
    {
        *pstate = 0;
        return false;
    }

    *pstate = value.u16;
    return true;
}


static bool fetch_resource_set_mask(mrp_msg_t *msg, void **pcursor,
                                    int mask_type, mrp_resproto_state_t *pmask)
{
    uint16_t expected_tag;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    switch (mask_type) {
    case 0:    expected_tag = RESPROTO_RESOURCE_GRANT;     break;
    case 1:   expected_tag = RESPROTO_RESOURCE_ADVICE;    break;
    default:       /* don't know what to fetch */              return false;
    }

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_UINT32)
    {
        *pmask = 0;
        return false;
    }

    *pmask = value.u32;
    return true;
}


static bool fetch_resource_set_id(mrp_msg_t *msg, void **pcursor,uint32_t *pid)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_SET_ID || type != MRP_MSG_FIELD_UINT32)
    {
        *pid = 0;
        return false;
    }

    *pid = value.u32;
    return true;
}


static bool fetch_str_array(mrp_msg_t *msg, void **pcursor,
                            uint16_t expected_tag, string_array_t **parr)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_ARRAY_OF(STRING))
    {
        *parr = str_array_dup(0, NULL);
        return false;
    }

    if (!(*parr = str_array_dup(size, (const char **)value.astr)))
        return false;

    return true;
}

static bool fetch_murphy_str_array(mrp_msg_t *msg, void **pcursor,
                   uint16_t expected_tag, murphy_string_array **parr)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_ARRAY_OF(STRING))
    {
        *parr = murphy_str_array_dup(0, NULL);
        return false;
    }

    if (!(*parr = murphy_str_array_dup(size, (const char **)value.astr)))
        return false;

    return true;
}


static bool fetch_seqno(mrp_msg_t *msg, void **pcursor, uint32_t *pseqno)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_SEQUENCE_NO || type != MRP_MSG_FIELD_UINT32)
    {
        *pseqno = 0;
        return -1;
    }

    *pseqno = value.u32;
    return 0;
}


static int fetch_request(mrp_msg_t *msg, void **pcursor, uint16_t *preqtype)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_REQUEST_TYPE || type != MRP_MSG_FIELD_UINT16)
    {
        *preqtype = 0;
        return -1;
    }

    *preqtype = value.u16;
    return 0;
}


static bool fetch_status(mrp_msg_t *msg, void **pcursor, int *pstatus)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_REQUEST_STATUS || type != MRP_MSG_FIELD_SINT16)
    {
        *pstatus = EIO;
        return false;
    }

    *pstatus = value.s16;
    return true;
}


static resource_def_t *copy_resource_def(const resource_def_t *orig)
{
    resource_def_t *copy;

    if (!orig)
        return NULL;

    copy = mrp_allocz(sizeof(resource_def_t));

    copy->name = mrp_strdup(orig->name);
    copy->attrs = attribute_array_dup(orig->attrs->dim, orig->attrs->elems);

    return copy;
}


static void resource_def_free(resource_def_t *def)
{
    if (!def)
        return;

    mrp_free((void *)def->name);
    attribute_array_free(def->attrs);
}


static void resource_def_array_free(resource_def_array_t *arr)
{
    uint32_t i;
    resource_def_t *def;

    if (arr) {
        for (i = 0;   i < arr->dim;   i++) {
            def = arr->defs + i;
            resource_def_free(def);
        }

        mrp_free(arr);
    }
}

#if 0
static resource_def_array_t *resource_query_response(mrp_msg_t *msg, void **pcursor)
{
    int             status;
    uint32_t        dim, i;
    resource_def_t  rdef[RESOURCE_MAX];
    attribute_t     attrs[ATTRIBUTE_MAX + 1];
    resource_def_t *src, *dst;
    resource_def_array_t *arr;
    size_t          size;

    if (!fetch_status(msg, pcursor, &status))
        goto failed;

    if (status != 0)
        printf("Resource query failed (%u): %s\n", status, strerror(status));
    else {
        dim = 0;

        while (fetch_resource_name(msg, pcursor, &rdef[dim].name)) {
            if (fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX+1, attrs) < 0)
                goto failed;

            if (!(rdef[dim].attrs = attribute_array_dup(0, attrs))) {
                mrp_log_error("failed to duplicate attributes");
                return NULL;
            }

            dim++;
        }

        size = sizeof(resource_def_array_t) + sizeof(resource_def_t) * (dim+1);

        arr = mrp_allocz(size);

        arr->dim = dim;

        for (i = 0;  i < dim;  i++) {
            src = rdef + i;
            dst = arr->defs + i;

            dst->name  = mrp_strdup(src->name);
            dst->attrs = src->attrs;
        }
    }

    return arr;

 failed:
    mrp_log_error("malformed reply to recource query");

    return NULL;
}
#endif

void priv_attr_to_murphy_attr(attribute_t *attr, murphy_resource_attribute *attribute) {

    if (attr == NULL || attribute == NULL)
        return;

    attribute->name = mrp_strdup(attr->name);

    switch (attr->type) {
        case 's':
            attribute->type = murphy_string;
            attribute->string = mrp_strdup(attr->name);
            break;
        case 'i':
            attribute->type = murphy_int32;
            attribute->integer = attr->integer;
            break;
        case 'u':
            attribute->type = murphy_uint32;
            attribute->unsignd = attr->unsignd;
            break;
        case 'f':
            attribute->type = murphy_double;
            attribute->floating = attr->floating;
            break;
        default:
            attribute->type = murphy_invalid;
        }
    }

int priv_res_to_murphy_res(resource_def_t *src, murphy_resource *dst) {

    uint32_t i = 0;

    dst->name  = mrp_strdup(src->name);
    dst->state = murphy_resource_lost;
    dst->priv->mandatory = false;
    dst->priv->shared = false;
    dst->priv->num_attributes = src->attrs->dim;

    dst->priv->attrs = mrp_allocz(sizeof(murphy_resource_attribute) * src->attrs->dim);

    for (i = 0; i < src->attrs->dim; i++) {
        priv_attr_to_murphy_attr(&src->attrs->elems[i], &dst->priv->attrs[i]);
    }
    return 0;
}

static murphy_resource_set *resource_query_response(mrp_msg_t *msg, void **pcursor)
{
    int             status;
    uint32_t        dim, i;
    resource_def_t  rdef[RESOURCE_MAX];
    attribute_t     attrs[ATTRIBUTE_MAX + 1];
    resource_def_t *src;
    murphy_resource_set *arr;

    if (!fetch_status(msg, pcursor, &status))
        goto failed;

    if (status != 0)
        printf("Resource query failed (%u): %s\n", status, strerror(status));
    else {
        dim = 0;

        while (fetch_resource_name(msg, pcursor, &rdef[dim].name)) {
            if (fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX+1, attrs) < 0)
                goto failed;

            if (!(rdef[dim].attrs = attribute_array_dup(0, attrs))) {
                mrp_log_error("failed to duplicate attributes");
                return NULL;
            }

            dim++;
        }

        arr = mrp_allocz(sizeof(murphy_resource_set));

        if (!arr)
            goto failed;

        arr->priv = mrp_allocz(sizeof(murphy_resource_private_t));

        if (!arr->priv)
            goto failed;

        arr->application_class = NULL;
        arr->state = murphy_resource_lost;
        arr->priv->num_resources = dim;

        arr->priv->resources = mrp_allocz_array(murphy_resource *, dim);

        for (i = 0; i < dim; i++) {
            src = rdef + i;
            arr->priv->resources[i] = mrp_allocz(sizeof(murphy_resource));
            arr->priv->resources[i]->priv = mrp_allocz(sizeof(murphy_resource_private_t));
            priv_res_to_murphy_res(src, arr->priv->resources[i]);
        }
    }

    return arr;

 failed:
    mrp_log_error("malformed reply to resource query");

    return NULL;
}


#if 0
static string_array_t *class_query_response(mrp_msg_t *msg, void **pcursor)
{
    int status;
    string_array_t *arr;

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_str_array(msg, pcursor, RESPROTO_CLASS_NAME, &arr)))
    {
        mrp_log_error("ignoring malformed response to class query");
        return NULL;
    }

    if (status) {
        mrp_log_error("class query failed with error code %u", status);
        return NULL;
    }

    return arr;
}
#endif

static murphy_string_array *class_query_response(mrp_msg_t *msg, void **pcursor)
{
    int status;
    murphy_string_array *arr;

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_murphy_str_array(msg, pcursor, RESPROTO_CLASS_NAME, &arr)))
    {
        mrp_log_error("ignoring malformed response to class query");
        return NULL;
    }

    if (status) {
        mrp_log_error("class query failed with error code %u", status);
        return NULL;
    }

    return arr;
}


static bool create_resource_set_response(mrp_msg_t *msg, murphy_resource_set *rset, void **pcursor)
{
    int status;
    uint32_t rset_id;

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_resource_set_id(msg, pcursor, &rset_id)))
    {
        mrp_log_error("ignoring malformed response to resource set creation");
        goto error;
    }

    if (status) {
        mrp_log_error("creation of resource set failed. error code %u",status);
        goto error;
    }

    rset->priv->id = rset_id;

    return true;
error:
    return false;
}


static murphy_resource_set *acquire_resource_set_response(mrp_msg_t *msg,
            murphy_resource_context *cx, void **pcursor)
{
    int status;
    uint32_t rset_id;
    murphy_resource_set *rset = NULL;

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_status(msg, pcursor, &status))
    {
        mrp_log_error("ignoring malformed response to resource set");
        goto error;
    }

    if (status) {
        mrp_log_error("acquiring of resource set failed. error code %u",status);
        goto error;
    }

    /* we need the previous resource set because the new one doesn't
     * tell us the resource set class */

    rset = mrp_htbl_lookup(cx->priv->rset_mapping, u_to_p(rset_id));

    if (!rset) {
        printf("no rset found!\n");
        goto error;
    }

#if 0
    /* mark the resource set as acquired */

    rset->state = murphy_resource_acquired;
#endif

    return rset;

error:
    return NULL;
}


static int acquire_resource_set(murphy_resource_context *cx, murphy_resource_set *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    rset->priv->seqno = cx->priv->next_seqno;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32,
                            cx->priv->next_seqno++,
                RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, RESPROTO_ACQUIRE_RESOURCE_SET,
                RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
                RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;


    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    return 0;

error:
    mrp_free(msg);
    return -1;
}


static int release_resource_set(murphy_resource_context *cx, murphy_resource_set *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    rset->priv->seqno = cx->priv->next_seqno;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32,
                            cx->priv->next_seqno++,
                RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, RESPROTO_RELEASE_RESOURCE_SET,
                RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
                RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    return 0;

error:
    mrp_free(msg);
    return -1;
}

static murphy_resource *get_resource_by_name(murphy_resource_set *rset,
        const char *name)
{
    int i;

    if (!rset || !name)
        return NULL;

    for (i = 0; i < rset->priv->num_resources; i++) {
        murphy_resource *res = rset->priv->resources[i];
        printf("    comparing '%s with %s'\n", name, res->name);
        if (strcmp(res->name, name) == 0) {
            return res;
        }
    }

    return NULL;
}


static void resource_event(mrp_msg_t *msg,
        murphy_resource_context *cx,
        int32_t seqno,
        void **pcursor)
{
    uint32_t rset_id;
    uint32_t grant, advice;
    mrp_resproto_state_t state;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    uint32_t resid;
    const char *resnam;
    attribute_t attrs[ATTRIBUTE_MAX + 1];
    attribute_array_t *list;
    uint32_t mask;

    murphy_resource_set *rset;

    printf("\nResource event (request no %u):\n", seqno);

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_resource_set_state(msg, pcursor, &state) ||
        !fetch_resource_set_mask(msg, pcursor, 0, &grant) ||
        !fetch_resource_set_mask(msg, pcursor, 1, &advice))
        goto malformed;

    /* Update our "master copy" of the resource set. */

    rset = mrp_htbl_lookup(cx->priv->rset_mapping, u_to_p(rset_id));

    if (!rset)
        goto malformed;

    switch (state) {
        case RESPROTO_RELEASE:
            rset->state = murphy_resource_lost;
            break;
        case RESPROTO_ACQUIRE:
            rset->state = murphy_resource_acquired;
            break;
    }

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {

        murphy_resource *res = NULL;

        if ((tag != RESPROTO_RESOURCE_ID || type != MRP_MSG_FIELD_UINT32) ||
            !fetch_resource_name(msg, pcursor, &resnam)) {
            printf("malformed 1\n");
            goto malformed;
        }

        res = get_resource_by_name(rset, resnam);

        if (!res) {
            printf("malformed 2\n");
            goto malformed;
        }

        resid = value.u32;
        mask  = (1UL <<  resid);

        if (grant & mask) {
            res->state = murphy_resource_acquired;
        }
        else if (advice & mask) {
            res->state = murphy_resource_available;
        }
        else {
            res->state = murphy_resource_lost;
        }

        if (fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX + 1, attrs) < 0) {
            printf("malformed 3\n");
            goto malformed;
        }

        if (!(list = attribute_array_dup(0, attrs))) {
            mrp_log_error("failed to duplicate attribute list");
        }

        /* TODO attributes */

        attribute_array_free(list);
    }

    /* Check the resource set state. If the set is under construction
     * (we are waiting for "acquire" message), do not do the callback
     * before that. Otherwise, if this is a real event, call the
     * callback right away. */

    /* TODO properly */

    if (!rset->priv->seqno) {
        murphy_resource_set *rset_new;

        if (rset->priv->cb) {
            rset_new = murphy_resource_set_copy(rset);

            rset_new->priv->cb(cx, rset_new, rset_new->priv->user_data);
        }
    }

    return;

 malformed:
    mrp_log_error("ignoring malformed resource event");
}


static void recvfrom_msg(mrp_transport_t *transp, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    murphy_resource_context *cx = user_data;
    void *cursor = NULL;
    uint32_t seqno;
    uint16_t req;

    (void)transp;
    (void)addr;
    (void)addrlen;

    if (fetch_seqno(msg, &cursor, &seqno) < 0 || fetch_request(msg, &cursor, &req) < 0)
        goto error;

    printf("received message %d for %p\n", req, cx);

    switch (req) {
        case RESPROTO_QUERY_RESOURCES:
            printf("received QUERY_RESOURCES response\n");
#if 0
            cx->priv->available_resources = resource_query_response(msg, &cursor);
            if (!cx->priv->available_resources)
                goto error;
#endif
            cx->priv->master_resource_set = resource_query_response(msg, &cursor);
            if (!cx->priv->master_resource_set)
                goto error;
            break;
        case RESPROTO_QUERY_CLASSES:
            printf("received QUERY_CLASSES response\n");
#if 0
            cx->priv->classes = class_query_response(msg, &cursor);
            if (!cx->priv->classes)
                goto error;
#endif
            cx->priv->master_classes = class_query_response(msg, &cursor);
            if (!cx->priv->master_classes)
                goto error;
            break;
        case RESPROTO_CREATE_RESOURCE_SET:
        {
            murphy_resource_set_private_t *priv = NULL;
            murphy_resource_set *rset = NULL;
            mrp_list_hook_t *p, *n;

            printf("received CREATE_RESOURCE_SET response\n");

            /* get the correct resource set from the pending_sets list */

            mrp_list_foreach(&cx->priv->pending_sets, p, n) {
                priv = mrp_list_entry(p, typeof(*priv), hook);

                if (priv->seqno == seqno) {
                    rset = priv->pub;
                    break;
                }
            }

            if (!rset) {
                /* the corresponding set wasn't found */
                goto error;
            }

            mrp_list_delete(&rset->priv->hook);

            if (!create_resource_set_response(msg, rset, &cursor))
                goto error;

            mrp_htbl_insert(cx->priv->rset_mapping, u_to_p(rset->priv->id), rset);

            if (acquire_resource_set(cx, rset) < 0) {
                goto error;
            }
            break;
        }
        case RESPROTO_ACQUIRE_RESOURCE_SET:
        {
            murphy_resource_set *rset;

            printf("received ACQUIRE_RESOURCE_SET response\n");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            /* TODO: make new aqcuires fail until seqno == 0 */
            rset->priv->seqno = 0;

            if (!rset) {
                goto error;
            }

            /* call the resource set callback */

            if (rset->priv->cb) {
                murphy_resource_set *rset_new;

                rset_new = murphy_resource_set_copy(rset);
                rset_new->priv->cb(cx, rset_new, rset_new->priv->user_data);
            }
            break;
        }
        case RESPROTO_RELEASE_RESOURCE_SET:
        {
            murphy_resource_set *rset;
            printf("received RELEASE_RESOURCE_SET response\n");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            /* TODO: make new aqcuires fail until seqno == 0 */
            rset->priv->seqno = 0;

            if (!rset) {
                goto error;
            }

            /* call the resource set callback */

            if (rset->priv->cb) {
                murphy_resource_set *rset_new;

                rset_new = murphy_resource_set_copy(rset);
                rset_new->priv->cb(cx, rset_new, rset_new->priv->user_data);
            }
            break;
        }
        case RESPROTO_RESOURCES_EVENT:
            printf("received RESOURCES_EVENT response\n");

            resource_event(msg, cx, seqno, &cursor);
            break;
        default:
            break;
    }

    if (cx->state == murphy_disconnected &&
            cx->priv->master_classes &&
            cx->priv->master_resource_set) {
        cx->state = murphy_connected;
        cx->priv->cb(cx, murphy_resource_error_none, cx->priv->user_data);
    }

    return;

error:
    printf("error processing a message from Murphy\n");
    cx->priv->cb(cx, murphy_resource_error_internal, cx->priv->user_data);
}


static void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(t, msg, NULL, 0, user_data);
}


void closed_evt(mrp_transport_t *transp, int error, void *user_data)
{
    murphy_resource_context *cx = user_data;
    (void)transp;
    (void)error;

    printf("connection closed for %p\n", cx);
    cx->priv->connected = FALSE;
}


static void destroy_context(murphy_resource_context *cx)
{
    if (cx) {

        if (cx->priv->transp)
            mrp_transport_destroy(cx->priv->transp);

        /* FIXME: is this the way we want to free all resources and
         * resource sets? */
        if (cx->priv->rset_mapping)
            mrp_htbl_destroy(cx->priv->rset_mapping, true);

        if (cx->priv->available_resources)
            resource_def_array_free(cx->priv->available_resources);

        mrp_free(cx->priv);
        mrp_free(cx);
    }
}


static int get_application_classes(murphy_resource_context *cx)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, RESPROTO_QUERY_CLASSES,
            RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    return 0;

error:
    mrp_free(msg);
    return -1;
}


static int get_available_resources(murphy_resource_context *cx)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, RESPROTO_QUERY_RESOURCES,
            RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    return 0;

error:
    mrp_free(msg);
    return -1;
}


static void htbl_free_rset_mapping(void *key, void *object)
{
    MRP_UNUSED(key);
    MRP_UNUSED(object);
}


murphy_resource_context *murphy_create(mrp_mainloop_t *ml,
                       murphy_state_callback cb,
                       void *userdata)
{
    static mrp_transport_evt_t evt = {
        { .recvmsg     = recv_msg },
        { .recvmsgfrom = recvfrom_msg },
        .closed        = closed_evt,
        .connection    = NULL
    };

    int alen;
    const char *type;
    mrp_htbl_config_t conf;
    murphy_resource_context *cx = mrp_allocz(sizeof(murphy_resource_context));

    if (!cx)
        goto error;

    cx->priv = mrp_allocz(sizeof(struct murphy_resource_context_private_s));

    if (!cx->priv)
        goto error;

    cx->priv->next_seqno = 1;
    cx->priv->ml = ml;
    cx->priv->connection_id = 0;
    cx->priv->cb = cb;
    cx->priv->user_data = userdata;

    conf.comp = int_comp;
    conf.hash = int_hash;
    conf.free = htbl_free_rset_mapping;
    conf.nbucket = 0;
    conf.nentry = 5;

    /* When the resource set is "created" on the server side, we get
     * back an id. The id is then mapped to the actual resource set on
     * the client side, so that the event can be addressed to the
     * correct resource set. */
    cx->priv->rset_mapping = mrp_htbl_create(&conf);

    if (!cx->priv->rset_mapping)
        goto error;

    /* connect to Murphy */

    alen = mrp_transport_resolve(NULL, RES_ADDRESS, &cx->priv->saddr,
                                         sizeof(cx->priv->saddr), &type);

    cx->priv->transp = mrp_transport_create(cx->priv->ml, type,
                                          &evt, cx, 0);

    if (!cx->priv->transp)
        goto error;

    if (!mrp_transport_connect(cx->priv->transp, &cx->priv->saddr, alen))
        goto error;

    cx->priv->connected = TRUE;
    cx->state = murphy_disconnected;

    if (get_application_classes(cx) < 0 || get_available_resources(cx) < 0) {
        goto error;
    }

    mrp_list_init(&cx->priv->pending_sets);

    return cx;

error:

    printf("Error connecting to Murphy!\n");
    destroy_context(cx);

    return NULL;
}


void murphy_destroy(murphy_resource_context *cx)
{
    destroy_context(cx);
}

const murphy_string_array * murphy_application_class_list(murphy_resource_context *cx)
{
    if (!cx)
        return NULL;

    return cx->priv->master_classes;
}


static void delete_resource(murphy_resource *res)
{
    if (!res)
        return;

    mrp_free(res->priv);
    mrp_free(res);
}

murphy_resource *murphy_resource_create(murphy_resource_context *cx,
                    murphy_resource_set *set,
                    const char *name,
                    bool mandatory,
                    bool shared)
{
    murphy_resource *res = NULL, *proto = NULL;
    int i = 0;
    bool found = false;

    if (cx == NULL || set == NULL || name == NULL)
        return NULL;

    for (i = 0; i < cx->priv->master_resource_set->priv->num_resources; i++) {
        proto = cx->priv->master_resource_set->priv->resources[i];
        if (strcmp(proto->name, name) == 0) {
            found = true;
            break;
        }
    }

    if (!found)
        goto error;

    res = mrp_allocz(sizeof(murphy_resource));

    if (!res)
        goto error;

    res->name = mrp_strdup(name);

    res->state = murphy_resource_pending;

    res->priv = mrp_allocz(sizeof(murphy_resource_private_t));
    res->priv->mandatory = mandatory;
    res->priv->shared = shared;

    if (!res->priv)
        goto error;

    res->priv->pub = res;

    /* copy the attributes with the default values */
    res->priv->attrs = murphy_attribute_array_dup(proto->priv->num_attributes,
                proto->priv->attrs);

    res->priv->num_attributes = proto->priv->num_attributes;

    /* add resource to resource set */
    set->priv->resources[set->priv->num_resources++] = res;

    return res;

error:
    printf("murphy_create_resource error\n");
    delete_resource(res);

    return NULL;
}


static void delete_resource_set(murphy_resource_set *rs)
{
    int i;

    if (!rs)
        return;

    for (i = 0; i < rs->priv->num_resources; i++) {
        /* FIXME */
        // delete_resource(rs->resources[i]);
    }

    mrp_free(rs->priv);
    mrp_free(rs);
}


static murphy_resource_set *create_resource_set(
        murphy_resource_context *cx,
        const char *klass,
        murphy_resource_callback cb,
        void *userdata)
{
    murphy_resource_set *rs = mrp_allocz(sizeof(murphy_resource_set));

    if (!rs)
        goto error;

    rs->priv = mrp_allocz(sizeof(murphy_resource_set_private_t));
    if (!rs->priv)
        goto error;

    rs->application_class = mrp_strdup(klass);

    rs->priv->pub = rs;
    rs->priv->id = 0;
    rs->priv->seqno = 0;
    rs->priv->cb = cb;
    rs->priv->user_data = userdata;
    rs->state = murphy_resource_pending;

    rs->priv->resources = mrp_allocz_array(murphy_resource *,
            cx->priv->master_resource_set->priv->num_resources);

    mrp_list_init(&rs->priv->hook);

    return rs;

error:
    delete_resource_set(rs);
    return NULL;
}

const murphy_resource_set * murphy_resource_set_list(murphy_resource_context *cx)
{
    if (cx == NULL || cx->priv == NULL)
        return NULL;

    return cx->priv->master_resource_set;
}

#if 0
int murphy_resource_set_list_to_context(murphy_resource_context *cx,
                    murphy_resource_set **set)
{
    int i = 0;

    murphy_resource_set *rs = create_resource_set("implicit");

    rs->num_resources = cx->priv->available_resources->dim;

    if (rs->num_resources > MAX_LEN)
        goto error;

    for (i = 0; i < rs->num_resources; i++) {
        rs->resources[i] = create_resource(cx, cx->priv->available_resources->defs[i].name);
        if (!rs->resources[i]) {
            rs->num_resources = i; /* only allocated this much */
            goto error;
        }
    }

    *set = rs;

    return 0;

error:
    printf("murphy_list_resources error\n");
    delete_resource_set(rs);

    return -1;
}
#endif

int murphy_resource_set_acquire(murphy_resource_context *cx,
                murphy_resource_set *rset)
{
    mrp_msg_t *msg = NULL;
    int i;

    printf("> murphy_acquire_resources\n");
    print_resource_set(rset);

    if (!cx->priv->connected)
        goto error;

    if (rset->priv->id) {

        /* the set has been already created */

        if (rset->state == murphy_resource_acquired) {
            /* already requested, updating is not supported yet */

            /* TODO: when supported by backend
             * type = RESPROTO_UPDATE_RESOURCE_SET
             */
            goto error;
        }
        else {
            /* re-acquire a lost or released set */
            return acquire_resource_set(cx, rset);
        }
    }

    /* First, create the resource set. The acquisition is continued
     * when the set is created. */

    rset->priv->seqno = cx->priv->next_seqno;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno++,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, RESPROTO_CREATE_RESOURCE_SET,
            RESPROTO_RESOURCE_FLAGS, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_RESOURCE_PRIORITY, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_CLASS_NAME, MRP_MSG_FIELD_STRING, rset->application_class,
            RESPROTO_ZONE_NAME, MRP_MSG_FIELD_STRING, "driver",
            RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;

    for (i = 0; i < rset->priv->num_resources; i++) {
        uint32_t j;
        uint32_t flags = 0;
        murphy_resource *res = rset->priv->resources[i];

        if (!res)
            goto error;

        printf("    adding %s\n", res->name);

        if (res->priv->shared)
            flags |= RESPROTO_RESFLAG_SHARED;

        if (res->priv->mandatory)
            flags |= RESPROTO_RESFLAG_MANDATORY;

        mrp_msg_append(msg, RESPROTO_RESOURCE_NAME, MRP_MSG_FIELD_STRING, res->name);
        mrp_msg_append(msg, RESPROTO_RESOURCE_FLAGS, MRP_MSG_FIELD_UINT32, flags);

        for (j = 0; j < res->priv->num_attributes; j++) {
            murphy_resource_attribute *elem = &res->priv->attrs[j];
            const char *attr_name = elem->name;

            mrp_msg_append(msg, RESPROTO_ATTRIBUTE_NAME, MRP_MSG_FIELD_STRING, attr_name);

            switch (elem->type) {
                case 's':
                    mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_STRING, elem->string);
                    break;
                case 'i':
                    mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_SINT32, elem->integer);
                    break;
                case 'u':
                    mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_UINT32, elem->unsignd);
                    break;
                case 'f':
                    mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_DOUBLE, elem->floating);
                    break;
                default:
                    break;
            }
        }

        mrp_msg_append(msg, RESPROTO_SECTION_END, MRP_MSG_FIELD_UINT8, 0);
    }


    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_list_append(&cx->priv->pending_sets, &rset->priv->hook);

    return 0;

error:
    mrp_free(msg);
    return -1;
}


int murphy_resource_set_release(murphy_resource_context *cx,
                murphy_resource_set *rset)
{
    printf("> murphy_release_resources\n");
    print_resource_set(rset);

    if (!cx->priv->connected)
        goto error;

    if (!rset->priv->id)
        goto error;

    if (rset->state != murphy_resource_acquired) {
        return 0;
    }

    return release_resource_set(cx, rset);

error:
    return -1;
}

#if 0
void murphy_resource_set_set_callback(murphy_resource_set *rset,
                      murphy_resource_callback cb,
                      void *userdata)
{
    if (!rset)
        return;

}
#endif

bool murphy_resource_set_equals(const murphy_resource_set *a,
                const murphy_resource_set *b)
{
    if (!a || !b)
        return false;

    return a->priv->id == b->priv->id;
}

murphy_resource_set *murphy_resource_set_create(murphy_resource_context *cx,
                        const char *app_class,
                        murphy_resource_callback cb,
                        void *userdata)
{
    if (cx == NULL)
        return NULL;
    return create_resource_set(cx, app_class, cb, userdata);
}


murphy_resource_set *murphy_resource_set_copy(murphy_resource_set *original)
{
    murphy_resource_set *copy = NULL;
    int i;

    printf("> murphy_resource_set_copy\n");

    copy = mrp_allocz(sizeof(murphy_resource_set));

    /* copy->id = original->id; */

    if (!copy)
        goto error;

    memcpy(copy, original, sizeof(murphy_resource_set));

    copy->priv = mrp_allocz(sizeof(murphy_resource_set_private_t));

    if (!copy->priv)
        goto error;

    memcpy(copy->priv, original->priv, sizeof(murphy_resource_set_private_t));

    for (i = 0; i < copy->priv->num_resources; i++) {
        copy->priv->resources[i]->priv->set = copy;
    }

    copy->priv->pub = copy;

    mrp_list_init(&copy->priv->hook);

    return copy;

error:
    delete_resource_set(copy);
    return NULL;
}


void murphy_resource_set_delete(murphy_resource_set *set)
{
    printf("> murphy_delete_resource_set\n");
    delete_resource_set(set);
}


#if 0
murphy_resource *murphy_resource_create(murphy_resource_context *cx,
                    const char *name,
                    bool mandatory,
                    bool shared)
{
    murphy_resource *rs = create_resource(cx, name);

    if (!rs)
        goto error;

    rs->mandatory = mandatory;
    rs->shared = shared;
    return rs;

error:
    return NULL;
}
#endif

void murphy_resource_delete(murphy_resource_set *set, murphy_resource *res)
{
    (void)set;

    if (res->priv->set) {
        if (!murphy_resource_delete_by_name(res->priv->set, res->name)) {
            /* hmm, strange */
            delete_resource(res);
        }
    }
    else
        delete_resource(res);
}

bool murphy_resource_delete_by_name(murphy_resource_set *rs, const char *name)
{
    int i;
    murphy_resource *res;

    /* assumption: only one resource of given name in the resource set */
    for (i = 0; i < rs->priv->num_resources; i++) {
        if (strcmp(rs->priv->resources[i]->name, name) == 0) {
            /* found at i */
            res = rs->priv->resources[i];
            break;
        }
    }

    if (i == rs->priv->num_resources) {
        /* not found */
        return false;
    }

    memmove(rs->priv->resources+i, rs->priv->resources+i+1,
            (rs->priv->num_resources-i) * sizeof(murphy_resource *));

    rs->priv->num_resources--;
    rs->priv->resources[rs->priv->num_resources] = NULL;

    delete_resource(res);

    return true;
}

#if 0
char **get_attribute_names(murphy_resource_context *cx, murphy_resource
                *res)
{
    int i;
    char **arr;

    if (!cx || !res)
        return NULL;

    arr = mrp_alloc_array(char *, res->priv->attrs->dim + 1);

    for (i = 0; i < res->priv->attrs->dim; i++) {
        arr[i] = mrp_strdup(res->priv->attrs->elems[i].name);
    }

    arr[i] = NULL;

    return arr;
}

enum murphy_attribute_type get_attribute_type(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name)
{
    int i;
    char **arr;

    if (!cx || !res)
        return murphy_invalid;

    arr = mrp_alloc_array(char *, res->priv->attrs->dim + 1);

    for (i = 0; i < res->priv->attrs->dim; i++) {
        attribute_t *elem = &res->priv->attrs->elems[i];
        if (strcmp(elem->name, attribute_name) == 0) {
            switch (elem->type) {
                case 's':
                    return murphy_string;
                case 'u':
                    return murphy_uint32;
                case 'i':
                    return murphy_int32;
                case 'f':
                    return murphy_double;
                default:
                    return murphy_invalid;
            }
        }
    }

    return murphy_invalid;
}


const void *murphy_get_attribute(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name)
{
    int i;
    const void *ret = NULL;

    if (!cx || !res)
        return NULL;

    for (i = 0; i < res->priv->attrs->dim; i++) {
        attribute_t *elem = &res->priv->attrs->elems[i];
        if (strcmp(elem->name, attribute_name) == 0) {
            switch (elem->type) {
                case 's':
                    ret = elem->string;
                case 'u':
                    ret = &elem->unsignd;
                case 'i':
                    ret = &elem->integer;
                case 'f':
                    ret = &elem->floating;
                default:
                    break;
            }
            break;
        }
    }
    return ret;
}


bool murphy_set_attribute(murphy_resource_context *cx, murphy_resource
                *res, const char *attribute_name, void *value)
{
    int i;

    if (!cx || !res || !value)
        return false;

    for (i = 0; i < res->priv->attrs->dim; i++) {
        attribute_t *elem = &res->priv->attrs->elems[i];
        if (strcmp(elem->name, attribute_name) == 0) {
            switch (elem->type) {
                case 's':
                    elem->string = mrp_strdup(value);
                case 'u':
                    elem->unsignd = *(uint32_t *) value;
                case 'i':
                    elem->integer = *(int32_t *) value;
                case 'f':
                    elem->floating = *(double *) value;
                default:
                    return false;
            }
            return true;
        }
    }
    return false;
}
#endif

int murphy_attribute_list(murphy_resource_context *cx,
        murphy_resource *res,
        murphy_string_array **names)
{
    int i;
    murphy_string_array *ret;

    if (!cx || !res)
        return -1;

    ret = mrp_allocz(sizeof(murphy_string_array));

    ret->num_strings = res->priv->num_attributes;
    ret->strings = mrp_allocz_array(const char *, res->priv->num_attributes);

    for (i = 0; i < res->priv->num_attributes; i++) {
        ret->strings[i] = res->priv->attrs[i].name;
    }

    *names = ret;

    return 0;
}

int murphy_attribute_get_by_name(murphy_resource_context *cx,
                 murphy_resource *res,
                 const char *name,
                 murphy_resource_attribute **attribute)
{
    int i;

    if (!cx || !res)
        return -1;

    for (i = 0; i < res->priv->num_attributes; i++) {
        if (strcmp(name, res->priv->attrs[i].name) == 0) {
            *attribute = &res->priv->attrs[i];
            return 0;
        }
    }

    return -1;
}

#if 0
int murphy_attribute_set(murphy_resource_context *cx,
             murphy_resource *res,
             const murphy_resource_attribute *attribute)
{
    return 0;
}
#endif