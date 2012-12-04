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

    mrp_res_resource_set_t *rset;

    printf("\nResource event (request no %u):\n", seqno);

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_resource_set_state(msg, pcursor, &state) ||
        !fetch_resource_set_mask(msg, pcursor, 0, &grant) ||
        !fetch_resource_set_mask(msg, pcursor, 1, &advice)) {
        printf("malformed 0.2\n");
        goto malformed;
    }

    /* Update our "master copy" of the resource set. */

    printf("going to get rset of id %d\n", rset_id);
    rset = mrp_htbl_lookup(cx->priv->rset_mapping, u_to_p(rset_id));

    if (!rset) {
        printf("resource event before creating the resource set!\n");
        goto malformed;
    }

    printf("resource set %p id: %u\n", rset, rset->priv->id);

    switch (state) {
        case RESPROTO_RELEASE:
            rset->state = MRP_RES_RESOURCE_LOST;
            break;
        case RESPROTO_ACQUIRE:
            rset->state = MRP_RES_RESOURCE_ACQUIRED;
            break;
    }

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {

        mrp_res_resource_t *res = NULL;

        printf("iterating through the resources...\n");

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
            res->state = MRP_RES_RESOURCE_ACQUIRED;
        }
        else if (advice & mask) {
            res->state = MRP_RES_RESOURCE_AVAILABLE;
        }
        else {
            res->state = MRP_RES_RESOURCE_LOST;
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

    if (!rset->priv->seqno) {
        if (rset->priv->cb) {
            increase_ref(cx, rset);
            rset->priv->cb(cx, rset, rset->priv->user_data);
            decrease_ref(cx, rset);
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
    mrp_res_context_t *cx = user_data;
    void *cursor = NULL;
    uint32_t seqno;
    uint16_t req;

    MRP_UNUSED(transp);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    if (fetch_seqno(msg, &cursor, &seqno) < 0 ||
                fetch_request(msg, &cursor, &req) < 0)
        goto error;

    printf("received message %d for %p\n", req, cx);

    switch (req) {
        case RESPROTO_QUERY_RESOURCES:

            printf("received QUERY_RESOURCES response\n");

            cx->priv->master_resource_set =
                    resource_query_response(msg, &cursor);
            if (!cx->priv->master_resource_set)
                goto error;
            break;
        case RESPROTO_QUERY_CLASSES:

            printf("received QUERY_CLASSES response\n");

            cx->priv->master_classes = class_query_response(msg, &cursor);
            if (!cx->priv->master_classes)
                goto error;
            break;
        case RESPROTO_CREATE_RESOURCE_SET:
        {
            mrp_res_resource_set_private_t *priv = NULL;
            mrp_res_resource_set_t *rset = NULL;
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

            printf("inserting rset %p of id %d\n", rset, rset->priv->id);
            mrp_htbl_insert(cx->priv->rset_mapping,
                    u_to_p(rset->priv->id), rset);

            if (acquire_resource_set_request(cx, rset) < 0) {
                goto error;
            }
            break;
        }
        case RESPROTO_ACQUIRE_RESOURCE_SET:
        {
            mrp_res_resource_set_t *rset;

            printf("received ACQUIRE_RESOURCE_SET response\n");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            if (!rset) {
                goto error;
            }

            /* TODO: make new aqcuires fail until seqno == 0 */
            rset->priv->seqno = 0;

            /* call the resource set callback */

            if (rset->priv->cb) {
                increase_ref(cx, rset);
                rset->priv->cb(cx, rset, rset->priv->user_data);
                decrease_ref(cx, rset);
            }
            break;
        }
        case RESPROTO_RELEASE_RESOURCE_SET:
        {
            mrp_res_resource_set_t *rset;
            printf("received RELEASE_RESOURCE_SET response\n");

            rset = acquire_resource_set_response(msg, cx, &cursor);

            if (!rset) {
                goto error;
            }

            /* TODO: make new aqcuires fail until seqno == 0 */
            rset->priv->seqno = 0;

            /* call the resource set callback */

            if (rset->priv->cb) {
                increase_ref(cx, rset);
                rset->priv->cb(cx, rset, rset->priv->user_data);
                decrease_ref(cx, rset);
            }
            break;
        }
        case RESPROTO_RESOURCES_EVENT:
            printf("received RESOURCES_EVENT response\n");

            resource_event(msg, cx, seqno, &cursor);
            break;
        case RESPROTO_DESTROY_RESOURCE_SET:
            /* TODO */
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
    printf("error processing a message from Murphy\n");
    cx->priv->cb(cx, MRP_RES_ERROR_INTERNAL, cx->priv->user_data);
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

    printf("connection closed for %p\n", cx);
    cx->priv->connected = FALSE;
}


static void destroy_context(mrp_res_context_t *cx)
{
    if (!cx)
        return;

    if (cx->priv) {

        if (cx->priv->transp)
            mrp_transport_destroy(cx->priv->transp);

        delete_resource_set(cx, cx->priv->master_resource_set);

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
    printf("> htbl_free_rset_mapping(%d, %p)\n", p_to_u(key), object);
#else
    MRP_UNUSED(key);
#endif
    free_resource_set(object);
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

    alen = mrp_transport_resolve(NULL, RESPROTO_DEFAULT_ADDRESS,
            &cx->priv->saddr, sizeof(cx->priv->saddr), &type);

    cx->priv->transp = mrp_transport_create(cx->priv->ml, type,
                                          &evt, cx, 0);

    if (!cx->priv->transp)
        goto error;

    if (!mrp_transport_connect(cx->priv->transp, &cx->priv->saddr, alen))
        goto error;

    cx->priv->connected = TRUE;
    cx->state = MRP_RES_DISCONNECTED;

    if (get_application_classes(cx) < 0 || get_available_resources(cx) < 0) {
        goto error;
    }

    /* TODO: this needs to be gotten from an environment variable */
    cx->zone = "driver";

    mrp_list_init(&cx->priv->pending_sets);

    return cx;

error:

    printf("Error connecting to Murphy!\n");
    destroy_context(cx);

    return NULL;
}


void mrp_res_destroy(mrp_res_context_t *cx)
{
    printf("> mrp_res_destroy\n");
    destroy_context(cx);
}

