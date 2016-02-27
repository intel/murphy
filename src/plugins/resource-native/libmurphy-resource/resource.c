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

#include <murphy/resource/protocol.h>
#include "resource-api.h"
#include "resource-private.h"

#include "string_array.h"
#include "message.h"
#include "rset.h"
#include "attribute.h"


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


static void resource_event(mrp_msg_t *msg,
        mrp_res_context_t *cx,
        int32_t seqno,
        void **pcursor)
{
    uint32_t rset_id;
    uint32_t grant, advice, pending;
    mrp_resproto_state_t state;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    uint32_t resid;
    const char *resnam;
    mrp_res_attribute_t attrs[ATTRIBUTE_MAX + 1];
    int n_attrs;
    uint32_t mask, all = 0x0, mandatory = 0x0;
    uint32_t i;
    mrp_res_resource_set_t *rset;
    bool should_release_resource;

    mrp_res_info("Resource event (request no %u):", seqno);

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_resource_set_state(msg, pcursor, &state) ||
        !fetch_resource_set_mask(msg, pcursor, 0, &grant) ||
        !fetch_resource_set_mask(msg, pcursor, 1, &advice) ||
        !fetch_resource_set_mask(msg, pcursor, 2, &pending)) {
        mrp_res_error("failed to fetch data from message");
        goto ignore;
    }

    /* Update our "master copy" of the resource set. */

    rset = mrp_htbl_lookup(cx->priv->rset_mapping, u_to_p(rset_id));

    if (!rset) {
        mrp_res_info("resource event outside the resource set lifecycle");
        goto ignore;
    }

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {

        mrp_res_resource_t *res = NULL;

        if ((tag != RESPROTO_RESOURCE_ID || type != MRP_MSG_FIELD_UINT32) ||
                !fetch_resource_name(msg, pcursor, &resnam)) {
            mrp_res_error("failed to read resource from message");
            goto ignore;
        }

        res = get_resource_by_name(rset, resnam);

        if (!res) {
            mrp_res_error("resource doesn't exist in resource set");
            goto ignore;
        }

        resid = value.u32;

        mrp_res_info("data for '%s': %d", res->name, resid);

        if (!fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX + 1, attrs,
                &n_attrs)) {
            mrp_res_error("failed to read attributes from message");
            goto ignore;
        }

        /* copy the attributes */
        for (i = 0; (int) i < n_attrs; i++) {
            mrp_res_attribute_t *src = &attrs[i];
            mrp_res_attribute_t *dst = mrp_res_get_attribute_by_name(res, src->name);

            if (!dst) {
                mrp_log_error("unknown attribute '%s'!", src->name);
                continue;
            }

            if (src->type != dst->type) {
                mrp_log_error("attribute types don't match for '%s'!", src->name);
            }

            switch (src->type) {
                case mrp_int32:
                    mrp_res_set_attribute_int(dst, src->integer);
                    break;
                case mrp_uint32:
                    mrp_res_set_attribute_uint(dst, src->unsignd);
                    break;
                case mrp_double:
                    mrp_res_set_attribute_double(dst, src->floating);
                    break;
                case mrp_string:
                    mrp_res_set_attribute_string(dst, src->string);
                    break;
                default: /* mrp_invalid */
                    break;
            }
        }
    }

    /* go through all resources and see if they have been modified */

    for (i = 0; i < rset->priv->num_resources; i++)
    {
        mrp_res_resource_t *res = rset->priv->resources[i];

        mask  = (1UL << res->priv->server_id);
        all  |= mask;

        if (res->priv->mandatory)
            mandatory |= mask;

        if (grant & mask) {
            res->state = MRP_RES_RESOURCE_ACQUIRED;
        }
        else if (pending & mask) {
            res->state = MRP_RES_RESOURCE_ABOUT_TO_LOOSE;
        }
        else {
            res->state = MRP_RES_RESOURCE_LOST;
        }
    }

    mrp_res_info("advice = 0x%08x, grant = 0x%08x, mandatory = 0x%08x, all = 0x%08x",
            advice, grant, mandatory, all);

    should_release_resource = false;
    if (pending) {
        rset->state = MRP_RES_RESOURCE_ABOUT_TO_LOOSE;
        should_release_resource = true;
    }
    else if (grant) {
        rset->state = MRP_RES_RESOURCE_ACQUIRED;
    }
    else if (advice == mandatory) {
        rset->state = MRP_RES_RESOURCE_AVAILABLE;
    }
    else {
        rset->state = MRP_RES_RESOURCE_LOST;
    }

    /* Check the resource set state. If the set is under construction
     * (we are waiting for "acquire" or "release" message), do not do the
     * callback before that. Otherwise, if this is a real event, call the
     * callback right away. */

#if 0
    print_resource_set(rset);
#endif
    if (!rset->priv->seqno) {
        increase_ref(cx, rset);
        if (should_release_resource) {
            if (rset->priv->release_cb) {
                rset->priv->release_cb(cx, rset, rset->priv->release_cb_user_data);
            }
        }
        else if (rset->priv->cb) {
            rset->priv->cb(cx, rset, rset->priv->user_data);
        }
        decrease_ref(cx, rset);

        if (should_release_resource) {
            did_release_resource_set_request(cx, rset);
        }
    }

    return;

 ignore:
    mrp_res_info("ignoring resource event");
}


static void recvfrom_msg(mrp_transport_t *transp, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    mrp_res_context_t *cx = user_data;
    void *cursor = NULL;
    uint32_t seqno;
    uint16_t req;
    mrp_res_error_t err = MRP_RES_ERROR_INTERNAL;

    MRP_UNUSED(transp);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    if (!fetch_seqno(msg, &cursor, &seqno) ||
                !fetch_request(msg, &cursor, &req))
        goto error;

    mrp_res_info("received message %d for %p", req, cx);

    err = MRP_RES_ERROR_MALFORMED;

    switch (req) {
        case RESPROTO_QUERY_RESOURCES:

            mrp_res_info("received QUERY_RESOURCES response");

            cx->priv->master_resource_set =
                    resource_query_response(cx, msg, &cursor);
            if (!cx->priv->master_resource_set)
                goto error;
            break;
        case RESPROTO_QUERY_CLASSES:

            mrp_res_info("received QUERY_CLASSES response");

            cx->priv->master_classes = class_query_response(msg, &cursor);
            if (!cx->priv->master_classes)
                goto error;
            break;
        case RESPROTO_CREATE_RESOURCE_SET:
        {
            mrp_res_resource_set_private_t *priv = NULL;
            mrp_res_resource_set_t *rset = NULL;
            mrp_list_hook_t *p, *n;

            mrp_res_info("received CREATE_RESOURCE_SET response");

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

            mrp_htbl_insert(cx->priv->rset_mapping,
                    u_to_p(rset->priv->id), rset);

            /* TODO: if the operation was "acquire", do that. Otherwise
             * release. */

            if (rset->priv->waiting_for == MRP_RES_PENDING_OPERATION_ACQUIRE) {
                rset->priv->waiting_for = MRP_RES_PENDING_OPERATION_NONE;
                if (acquire_resource_set_request(cx, rset) < 0) {
                    goto error;
                }
            }
            else if (rset->priv->waiting_for == MRP_RES_PENDING_OPERATION_RELEASE) {
                rset->priv->waiting_for = MRP_RES_PENDING_OPERATION_NONE;
                if (release_resource_set_request(cx, rset) < 0) {
                    goto error;
                }
            }
            else {
                goto error;
            }
            break;
        }
        case RESPROTO_ACQUIRE_RESOURCE_SET:
        {
            mrp_res_resource_set_t *rset;

            mrp_res_info("received ACQUIRE_RESOURCE_SET response");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            if (!rset) {
                goto error;
            }

            rset->priv->seqno = 0;

            break;
        }
        case RESPROTO_RELEASE_RESOURCE_SET:
        {
            mrp_res_resource_set_t *rset;
            mrp_res_info("received RELEASE_RESOURCE_SET response");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            if (!rset) {
                goto error;
            }

            /* TODO: make new releases fail until seqno == 0 */
            rset->priv->seqno = 0;

            break;
        }
        case RESPROTO_DID_RELEASE_RESOURCE_SET:
        {
            mrp_res_resource_set_t *rset;
            mrp_res_info("received DID_RELEASE_RESOURCE_SET response");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            if (!rset) {
                goto error;
            }

            rset->priv->seqno = 0;

            break;
        }
        case RESPROTO_RESOURCES_EVENT:
            mrp_res_info("received RESOURCES_EVENT response");

            resource_event(msg, cx, seqno, &cursor);
            break;
        case RESPROTO_DESTROY_RESOURCE_SET:
            mrp_res_info("received DESTROY_RESOURCE_SET response");
            /* TODO? */
            break;
        default:
            break;
    }

    if (cx->state == MRP_RES_DISCONNECTED &&
            cx->priv->master_classes &&
            cx->priv->master_resource_set) {
        cx->state = MRP_RES_CONNECTED;
        cx->priv->cb(cx, MRP_RES_ERROR_NONE, cx->priv->user_data);
    }

    return;

error:
    mrp_res_error("error processing a message from the server");
    cx->priv->cb(cx, err, cx->priv->user_data);
}


static void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(t, msg, NULL, 0, user_data);
}


void closed_evt(mrp_transport_t *transp, int error, void *user_data)
{
    mrp_res_context_t *cx = user_data;
    MRP_UNUSED(transp);
    MRP_UNUSED(error);

    mrp_res_error("connection closed for %p", cx);
    cx->priv->connected = FALSE;

    if (cx->state == MRP_RES_CONNECTED) {
        cx->state = MRP_RES_DISCONNECTED;
        cx->priv->cb(cx, MRP_RES_ERROR_CONNECTION_LOST, cx->priv->user_data);
    }
}


static void destroy_context(mrp_res_context_t *cx)
{
    if (!cx)
        return;

    if (cx->priv) {

        if (cx->priv->transp)
            mrp_transport_destroy(cx->priv->transp);

        delete_resource_set(cx->priv->master_resource_set);

        /* FIXME: is this the way we want to free all resources and
         * resource sets? */
        if (cx->priv->rset_mapping)
            mrp_htbl_destroy(cx->priv->rset_mapping, false);

        if (cx->priv->internal_rset_mapping)
            mrp_htbl_destroy(cx->priv->internal_rset_mapping, true);

        mrp_res_free_string_array(cx->priv->master_classes);

        mrp_free(cx->priv);
    }
    mrp_free(cx);
}


static void htbl_free_rset_mapping(void *key, void *object)
{
#if 0
    mrp_res_info("> htbl_free_rset_mapping(%d, %p)", p_to_u(key), object);
#else
    MRP_UNUSED(key);
#endif

    mrp_res_resource_set_t *rset = object;
    free_resource_set(rset);
}

/* public API */

mrp_res_context_t *mrp_res_create(mrp_mainloop_t *ml,
                       mrp_res_state_callback_t cb,
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
    mrp_res_context_t *cx = mrp_allocz(sizeof(mrp_res_context_t));

    if (!cx)
        goto error;

    cx->priv = mrp_allocz(sizeof(struct mrp_res_context_private_s));

    if (!cx->priv)
        goto error;

    cx->priv->next_seqno = 1;
    cx->priv->next_internal_id = 1;
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

    /* When a resource set is acquired, we are keeping a "master copy" on the
     * server side. The client can free and copy this resource set as much as
     * it wants. The internal id is a method for understanding which resource
     * set maps to which. */
    cx->priv->internal_rset_mapping = mrp_htbl_create(&conf);

    if (!cx->priv->internal_rset_mapping)
        goto error;

    /* connect to Murphy */

    alen = mrp_transport_resolve(NULL, mrp_resource_get_default_address(),
            &cx->priv->saddr, sizeof(cx->priv->saddr), &type);

    cx->priv->transp = mrp_transport_create(cx->priv->ml, type,
                                          &evt, cx, 0);

    if (!cx->priv->transp)
        goto error;

    if (!mrp_transport_connect(cx->priv->transp, &cx->priv->saddr, alen))
        goto error;

    cx->priv->connected = TRUE;
    cx->state = MRP_RES_DISCONNECTED;

    if (get_application_classes_request(cx) < 0 || get_available_resources_request(cx) < 0) {
        goto error;
    }

    /* TODO: this needs to be gotten from an environment variable */
    cx->zone = "driver";

    mrp_list_init(&cx->priv->pending_sets);

    return cx;

error:

    mrp_res_error("error connecting to server");
    destroy_context(cx);

    return NULL;
}


void mrp_res_destroy(mrp_res_context_t *cx)
{
    destroy_context(cx);
}
