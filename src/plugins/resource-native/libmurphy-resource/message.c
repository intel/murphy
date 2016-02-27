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

#include "message.h"

#include "rset.h"
#include "attribute.h"

bool fetch_resource_set_state(mrp_msg_t *msg, void **pcursor,
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


bool fetch_resource_set_mask(mrp_msg_t *msg, void **pcursor,
                                    int mask_type, uint32_t *pmask)
{
    uint16_t expected_tag;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    switch (mask_type) {
    case 0:    expected_tag = RESPROTO_RESOURCE_GRANT;     break;
    case 1:   expected_tag = RESPROTO_RESOURCE_ADVICE;    break;
    case 2:   expected_tag = RESPROTO_RESOURCE_PENDING;    break;
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


bool fetch_resource_set_id(mrp_msg_t *msg, void **pcursor,uint32_t *pid)
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


bool fetch_mrp_str_array(mrp_msg_t *msg, void **pcursor,
                   uint16_t expected_tag, mrp_res_string_array_t **parr)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_ARRAY_OF(STRING))
    {
        *parr = mrp_str_array_dup(0, NULL);
        return false;
    }

    if (!(*parr = mrp_str_array_dup(size, (const char **)value.astr)))
        return false;

    return true;
}


bool fetch_seqno(mrp_msg_t *msg, void **pcursor, uint32_t *pseqno)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_SEQUENCE_NO || type != MRP_MSG_FIELD_UINT32)
    {
        *pseqno = 0;
        return false;
    }

    *pseqno = value.u32;
    return true;
}


bool fetch_request(mrp_msg_t *msg, void **pcursor, uint16_t *preqtype)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_REQUEST_TYPE || type != MRP_MSG_FIELD_UINT16)
    {
        *preqtype = 0;
        return false;
    }

    *preqtype = value.u16;
    return true;
}


bool fetch_status(mrp_msg_t *msg, void **pcursor, int *pstatus)
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



bool fetch_attribute_array(mrp_msg_t *msg, void **pcursor,
                                 size_t dim, mrp_res_attribute_t *arr,
                                 int *n_arr)
{
    mrp_res_attribute_t *attr;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    size_t i;
    *n_arr = 0;

    i = 0;

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {
        if (tag == RESPROTO_SECTION_END && type == MRP_MSG_FIELD_UINT8)
            break;

        if (tag  != RESPROTO_ATTRIBUTE_NAME ||
            type != MRP_MSG_FIELD_STRING ||
            i >= dim - 1) {
            return false;
        }

        attr = arr + i++;
        attr->name = value.str;

        if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
            tag != RESPROTO_ATTRIBUTE_VALUE) {
            return false;
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
            return false;
        }
    }

    memset(arr + i, 0, sizeof(mrp_res_attribute_t));

    *n_arr = i;

    return TRUE;
}


bool fetch_resource_name(mrp_msg_t *msg, void **pcursor,
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

bool fetch_resource_sync_release(mrp_msg_t *msg, void **pcursor,
                                    bool *psync_release)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_SYNC_RELEASE || type != MRP_MSG_FIELD_BOOL)
    {
        *psync_release = false;
        return false;
    }

    *psync_release = value.bln;
    return true;
}


static int priv_res_to_mrp_res(uint32_t id, resource_def_t *src, mrp_res_resource_t *dst,
        mrp_res_resource_set_t *set)
{
    dst->name  = mrp_strdup(src->name);
    dst->state = MRP_RES_RESOURCE_LOST;
    dst->priv->mandatory = false;
    dst->priv->shared = false;

    dst->priv->server_id = id;

    dst->priv->sync_release = src->sync_release;
    dst->priv->num_attributes = src->num_attrs;
    dst->priv->attrs = src->attrs;
    dst->priv->set = set;
    return 0;
}


mrp_res_resource_set_t *resource_query_response(mrp_res_context_t *cx,
        mrp_msg_t *msg, void **pcursor)
{
    int             status;
    uint32_t        dim, i;
    resource_def_t  rdef[RESOURCE_MAX];
    mrp_res_attribute_t attrs[ATTRIBUTE_MAX + 1];
    resource_def_t *src;
    mrp_res_resource_set_t *arr = NULL;

    if (!cx)
        goto failed;

    if (!fetch_status(msg, pcursor, &status))
        goto failed;

    if (status != 0)
        mrp_res_error("Resource query failed (%u): %s", status, strerror(status));
    else {
        dim = 0;

        while (fetch_resource_name(msg, pcursor, &rdef[dim].name)) {
            int n_attrs = 0;

            if (!fetch_resource_sync_release(msg, pcursor, &rdef[dim].sync_release)) {
                goto failed;
            }

            if (!fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX+1,
                    attrs, &n_attrs))
                goto failed;

            if (!(rdef[dim].attrs = mrp_attribute_array_dup(n_attrs, attrs))) {
                mrp_res_error("failed to duplicate attributes");
                goto failed;
            }

            rdef[dim].num_attrs = n_attrs;

            dim++;
        }

        arr = mrp_allocz(sizeof(mrp_res_resource_set_t));

        if (!arr)
            goto failed;

        arr->priv = mrp_allocz(sizeof(mrp_res_resource_set_private_t));

        if (!arr->priv)
            goto failed;

        arr->application_class = NULL;
        arr->state = MRP_RES_RESOURCE_LOST;
        arr->priv->cx = cx;
        arr->priv->num_resources = dim;

        arr->priv->resources = mrp_allocz_array(mrp_res_resource_t *, dim);

        if (!arr->priv->resources)
            goto failed;

        for (i = 0; i < dim; i++) {
            src = rdef + i;
            arr->priv->resources[i] = mrp_allocz(sizeof(mrp_res_resource_t));
            if (!arr->priv->resources[i]) {
                arr->priv->num_resources = i;
                goto failed;
            }
            arr->priv->resources[i]->priv =
                    mrp_allocz(sizeof(mrp_res_resource_private_t));
            if (!arr->priv->resources[i]->priv) {
                mrp_free(arr->priv->resources[i]);
                arr->priv->num_resources = i;
                goto failed;
            }
            priv_res_to_mrp_res(i, src, arr->priv->resources[i], arr);
        }
    }

    return arr;

 failed:
    mrp_res_error("malformed reply to resource query");
    free_resource_set(arr);

    return NULL;
}


mrp_res_string_array_t *class_query_response(mrp_msg_t *msg, void **pcursor)
{
    int status;
    mrp_res_string_array_t *arr = NULL;

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_mrp_str_array(msg, pcursor, RESPROTO_CLASS_NAME, &arr)))
    {
        mrp_res_error("ignoring malformed response to class query");
        return NULL;
    }

    if (status) {
        mrp_res_error("class query failed with error code %u", status);
        mrp_res_free_string_array(arr);
        return NULL;
    }

    return arr;
}


bool create_resource_set_response(mrp_msg_t *msg,
        mrp_res_resource_set_t *rset, void **pcursor)
{
    int status;
    uint32_t rset_id;

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_resource_set_id(msg, pcursor, &rset_id)))
    {
        mrp_res_error("ignoring malformed response to resource set creation");
        goto error;
    }

    if (status) {
        mrp_res_error("creation of resource set failed. error code %u",status);
        goto error;
    }

    rset->priv->id = rset_id;

    return true;
error:
    return false;
}


mrp_res_resource_set_t *acquire_resource_set_response(mrp_msg_t *msg,
            mrp_res_context_t *cx, void **pcursor)
{
    int status;
    uint32_t rset_id;
    mrp_res_resource_set_t *rset = NULL;

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_status(msg, pcursor, &status))
    {
        mrp_res_error("ignoring malformed response to resource set");
        goto error;
    }

    if (status) {
        mrp_res_error("acquiring of resource set failed. error code %u",status);
        goto error;
    }

    /* we need the previous resource set because the new one doesn't
     * tell us the resource set class */

    rset = mrp_htbl_lookup(cx->priv->rset_mapping, u_to_p(rset_id));

    if (!rset) {
        mrp_res_error("no rset found!");
        goto error;
    }

    return rset;

error:
    return NULL;
}


int acquire_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        return -1;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_ACQUIRE_RESOURCE_SET,
            RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
            RESPROTO_MESSAGE_END);

    if (!msg)
        return -1;

    rset->priv->seqno = cx->priv->next_seqno;
    cx->priv->next_seqno++;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}


int release_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        return -1;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_RELEASE_RESOURCE_SET,
            RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
            RESPROTO_MESSAGE_END);

    if (!msg)
        return -1;

    rset->priv->seqno = cx->priv->next_seqno;
    cx->priv->next_seqno++;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}

int did_release_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        return -1;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_DID_RELEASE_RESOURCE_SET,
            RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
            RESPROTO_MESSAGE_END);

    if (!msg)
        return -1;

    rset->priv->seqno = cx->priv->next_seqno;
    cx->priv->next_seqno++;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}


int create_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    mrp_msg_t *msg = NULL;
    uint32_t i;
    uint32_t rset_flags = 0;

    if (!cx || !rset)
        return -1;

    if (!cx->priv->connected)
        return -1;

    if (rset->priv->autorelease)
        rset_flags |= RESPROTO_RSETFLAG_AUTORELEASE;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_CREATE_RESOURCE_SET,
            RESPROTO_RESOURCE_FLAGS, MRP_MSG_FIELD_UINT32, rset_flags,
            RESPROTO_RESOURCE_PRIORITY, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_CLASS_NAME, MRP_MSG_FIELD_STRING, rset->application_class,
            RESPROTO_ZONE_NAME, MRP_MSG_FIELD_STRING, cx->zone,
            RESPROTO_MESSAGE_END);

    if (!msg)
        return -1;

    rset->priv->seqno = cx->priv->next_seqno;
    cx->priv->next_seqno++;

    for (i = 0; i < rset->priv->num_resources; i++) {
        int j;
        uint32_t res_flags = 0;
        mrp_res_resource_t *res = rset->priv->resources[i];

        if (!res)
            goto error;

        if (res->priv->shared)
            res_flags |= RESPROTO_RESFLAG_SHARED;

        if (res->priv->mandatory)
            res_flags |= RESPROTO_RESFLAG_MANDATORY;

        if (!mrp_msg_append(msg, RESPROTO_RESOURCE_NAME, MRP_MSG_FIELD_STRING,
                res->name))
            goto error;

        if (!mrp_msg_append(msg, RESPROTO_RESOURCE_FLAGS, MRP_MSG_FIELD_UINT32,
                res_flags))
            goto error;

        for (j = 0; j < res->priv->num_attributes; j++) {
            mrp_res_attribute_t *elem = &res->priv->attrs[j];
            const char *attr_name = elem->name;

            if (!mrp_msg_append(msg, RESPROTO_ATTRIBUTE_NAME, MRP_MSG_FIELD_STRING,
                    attr_name))
                goto error;

            switch (elem->type) {
                case 's':
                    if (!mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_STRING, elem->string))
                        goto error;
                    break;
                case 'i':
                    if (!mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_SINT32, elem->integer))
                        goto error;
                    break;
                case 'u':
                    if (!mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_UINT32, elem->unsignd))
                        goto error;
                    break;
                case 'f':
                    if (!mrp_msg_append(msg, RESPROTO_ATTRIBUTE_VALUE,
                            MRP_MSG_FIELD_DOUBLE, elem->floating))
                        goto error;
                    break;
                default:
                    break;
            }
        }

        if (!mrp_msg_append(msg, RESPROTO_SECTION_END, MRP_MSG_FIELD_UINT8, 0))
            goto error;
    }

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}


int get_application_classes_request(mrp_res_context_t *cx)
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

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}


int get_available_resources_request(mrp_res_context_t *cx)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, 0,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_QUERY_RESOURCES,
            RESPROTO_MESSAGE_END);

    if (!msg)
        goto error;

    if (!mrp_transport_send(cx->priv->transp, msg))
        goto error;

    mrp_msg_unref(msg);
    return 0;

error:
    mrp_msg_unref(msg);
    return -1;
}
