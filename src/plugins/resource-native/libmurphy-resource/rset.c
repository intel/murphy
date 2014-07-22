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

#include "rset.h"
#include "attribute.h"
#include "message.h"


static char *state_to_str(mrp_res_resource_state_t st)
{
    char *state = "unknown";
    switch (st) {
        case MRP_RES_RESOURCE_ACQUIRED:
            state = "acquired";
            break;
        case MRP_RES_RESOURCE_LOST:
            state = "lost";
            break;
        case MRP_RES_RESOURCE_AVAILABLE:
            state = "available";
            break;
        case MRP_RES_RESOURCE_PENDING:
            state = "pending";
            break;
    }
    return state;
}


void print_resource(mrp_res_resource_t *res)
{
    mrp_res_info("   resource '%s' -> '%s' : %smandatory, %sshared",
            res->name, state_to_str(res->state),
            res->priv->mandatory ? " " : "not ",
            res->priv->shared ? "" : "not ");
}

#if 0
void print_resource_set(mrp_res_resource_set_t *rset)
{
    uint32_t i;
    mrp_res_resource_t *res;

    mrp_res_info("Resource set %i/%i (%s) -> '%s':",
            rset->priv->id, rset->priv->internal_id,
            rset->application_class, state_to_str(rset->state));

    for (i = 0; i < rset->priv->num_resources; i++) {
        res = rset->priv->resources[i];
        print_resource(res);
    }
}
#endif

void increase_ref(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    MRP_UNUSED(cx);

    if (!rset)
        return;

    rset->priv->internal_ref_count++;
}


static int destroy_resource_set_request(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    mrp_msg_t *msg = NULL;

    if (!cx->priv->connected)
        goto error;

    rset->priv->seqno = cx->priv->next_seqno;

    msg = mrp_msg_create(
            RESPROTO_SEQUENCE_NO, MRP_MSG_FIELD_UINT32, cx->priv->next_seqno++,
            RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16,
                    RESPROTO_DESTROY_RESOURCE_SET,
            RESPROTO_RESOURCE_SET_ID, MRP_MSG_FIELD_UINT32, rset->priv->id,
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


void decrease_ref(mrp_res_context_t *cx,
        mrp_res_resource_set_t *rset)
{
    if (!rset)
        return;

    rset->priv->internal_ref_count--;

    if (rset->priv->internal_ref_count == 0) {
        mrp_log_info("delete the server resource set now");
        destroy_resource_set_request(cx, rset);

        /* if a rset is deleted, remove it from the pending sets */
        mrp_list_delete(&rset->priv->hook);

        mrp_htbl_remove(cx->priv->rset_mapping,
                u_to_p(rset->priv->id), FALSE);
        mrp_htbl_remove(cx->priv->internal_rset_mapping,
                u_to_p(rset->priv->internal_id), TRUE);
    }
}


mrp_res_resource_t *get_resource_by_name(mrp_res_resource_set_t *rset,
        const char *name)
{
    uint32_t i;

    if (!rset || !name)
        return NULL;

    for (i = 0; i < rset->priv->num_resources; i++) {
        mrp_res_resource_t *res = rset->priv->resources[i];
        if (strcmp(res->name, name) == 0) {
            return res;
        }
    }

    return NULL;
}


static void free_resource(mrp_res_resource_t *res)
{
    if (!res)
        return;

    mrp_free((void *) res->name);

    if (res->priv) {
        mrp_attribute_array_free(res->priv->attrs,
                res->priv->num_attributes);
    }

    mrp_free(res->priv);
    mrp_free(res);
}


void free_resource_set(mrp_res_resource_set_t *rset)
{
    uint32_t i;

    if (!rset)
        return;

    mrp_free((void *) rset->application_class);

    if (!rset->priv)
        goto end;

    for (i = 0; i < rset->priv->num_resources; i++) {
        free_resource(rset->priv->resources[i]);
    }
    mrp_free(rset->priv->resources);
    mrp_free(rset->priv);

end:
    mrp_free(rset);
}



void delete_resource_set(mrp_res_resource_set_t *rs)
{
    mrp_res_context_t *cx = NULL;

    if (!rs)
        return;

    if (rs->priv && rs->priv->cx) {
        cx = rs->priv->cx;

        /* check if the resource set being deleted is a library resource set */
        mrp_res_resource_set_t *internal_rset = mrp_htbl_lookup(
                cx->priv->internal_rset_mapping, u_to_p(rs->priv->internal_id));

        if (internal_rset && internal_rset != rs) {
            decrease_ref(cx, internal_rset);
        }
    }

    free_resource_set(rs);
}



static mrp_res_resource_t *resource_copy(const mrp_res_resource_t *original,
        mrp_res_resource_set_t *new_rset)
{
    mrp_res_resource_t *copy;

    copy = mrp_allocz(sizeof(mrp_res_resource_t));

    if (!copy)
        goto error;

    memcpy(copy, original, sizeof(mrp_res_resource_t));

    copy->name = mrp_strdup(original->name);

    if (!copy->name)
        goto error;

    copy->priv = mrp_allocz(sizeof(mrp_res_resource_private_t));

    if (!copy->priv)
        goto error;

    memcpy(copy->priv, original->priv, sizeof(mrp_res_resource_private_t));

    copy->priv->pub = copy;
    copy->priv->set = new_rset;

    copy->priv->attrs = mrp_attribute_array_dup(original->priv->num_attributes,
            original->priv->attrs);

    if (!copy->priv->attrs)
        goto error;

    return copy;

error:
    mrp_res_error("failed to copy resource");

    if (copy) {
        mrp_free((void *) copy->name);
        if (copy->priv) {
            mrp_attribute_array_free(copy->priv->attrs,
                    original->priv->num_attributes);
            mrp_free(copy->priv);
        }
        mrp_free(copy);
    }

    return NULL;
}



mrp_res_resource_set_t *resource_set_copy(
        const mrp_res_resource_set_t *original)
{
    mrp_res_resource_set_t *copy = NULL;
    uint32_t i;

    copy = mrp_allocz(sizeof(mrp_res_resource_set_t));

    if (!copy)
        goto error;

    copy->state = original->state;
    copy->application_class = mrp_strdup(original->application_class);

    if (!copy->application_class)
        goto error;

    copy->priv = mrp_allocz(sizeof(mrp_res_resource_set_private_t));

    if (!copy->priv)
        goto error;

    memcpy(copy->priv, original->priv, sizeof(mrp_res_resource_set_private_t));

    copy->priv->pub = copy;
    copy->priv->resources = mrp_allocz_array(mrp_res_resource_t *,
            original->priv->num_resources);

    if (copy->priv->resources == NULL && copy->priv->num_resources)
        goto error;

    for (i = 0; i < copy->priv->num_resources; i++) {
        copy->priv->resources[i] = resource_copy(original->priv->resources[i],
                copy);
        if (!copy->priv->resources[i]) {
            copy->priv->num_resources = --i;
            goto error;
        }
    }

    memset(&copy->priv->hook, 0, sizeof(mrp_list_hook_t));
    mrp_list_init(&copy->priv->hook);

    return copy;

error:
    free_resource_set(copy);
    return NULL;
}


static mrp_res_resource_set_t *create_resource_set(
        mrp_res_context_t *cx,
        const char *klass,
        mrp_res_resource_callback_t cb,
        void *userdata)
{
    mrp_res_resource_set_t *rs;
    mrp_res_resource_set_t *internal;

    if (cx->priv->master_resource_set == NULL)
        return NULL;

    rs = mrp_allocz(sizeof(mrp_res_resource_set_t));

    if (!rs)
        goto error;

    rs->priv = mrp_allocz(sizeof(mrp_res_resource_set_private_t));
    if (!rs->priv)
        goto error;

    rs->application_class = mrp_strdup(klass);

    rs->priv->pub = rs;
    rs->priv->cx = cx;
    rs->priv->id = 0;
    rs->priv->internal_id = cx->priv->next_internal_id++;
    rs->priv->seqno = 0;
    rs->priv->cb = cb;
    rs->priv->user_data = userdata;
    rs->state = MRP_RES_RESOURCE_PENDING;
    rs->priv->autorelease = FALSE;

    rs->priv->resources = mrp_allocz_array(mrp_res_resource_t *,
            cx->priv->master_resource_set->priv->num_resources);

    rs->priv->waiting_for = MRP_RES_PENDING_OPERATION_NONE;

    mrp_list_init(&rs->priv->hook);

    /* ok, create an library-side resource set that we can compare this one to */

    internal = resource_set_copy(rs);
    if (!internal)
        goto error;

    increase_ref(cx, internal);

    mrp_htbl_insert(cx->priv->internal_rset_mapping,
            u_to_p(internal->priv->internal_id), internal);

    return rs;

error:
    mrp_log_error("error creating resource set");
    delete_resource_set(rs);
    return NULL;
}


static int update_library_resource_set(mrp_res_context_t *cx,
        const mrp_res_resource_set_t *original,
        mrp_res_resource_set_t *rset)
{
    char *application_class = NULL;
    mrp_res_resource_t **resources = NULL;
    uint32_t i, num_resources = 0;

    if (!cx || !original)
        return -1;

    /* Update the rset with the values in the original resource set. There
     * is only one "library-side" resource set corresponding 1-1 to the server
     * resource set. The original is the "client-side" resource set, which there
     * can be many. */

     application_class = mrp_strdup(original->application_class);
     if (!application_class) {
        mrp_log_error("error with memory allocation");
        goto error;
    }

     resources = mrp_allocz_array(mrp_res_resource_t *,
            original->priv->num_resources);
     if (!resources) {
        mrp_log_error("error allocating %d resources", original->priv->num_resources);
        goto error;
    }

    for (i = 0; i < original->priv->num_resources; i++) {
        resources[i] = resource_copy(original->priv->resources[i], rset);
        if (!resources[i]) {
            mrp_log_error("error copying resources to library resource set");
            goto error;
        }
        num_resources++;
    }

    mrp_free((void *) rset->application_class);
    for (i = 0; i < rset->priv->num_resources; i++) {
        free_resource(rset->priv->resources[i]);
    }
    mrp_free(rset->priv->resources);

    rset->application_class = application_class;
    rset->priv->resources = resources;
    rset->priv->num_resources = num_resources;
    rset->priv->autorelease = original->priv->autorelease;

    return 0;

error:
    mrp_log_error("error updating library resource set");
    mrp_free(application_class);
    for (i = 0; i < num_resources; i++) {
        free_resource(resources[i]);
    }
    mrp_free(resources);

    return -1;
}

/* public API */

const mrp_res_string_array_t * mrp_res_list_application_classes(
        mrp_res_context_t *cx)
{
    if (!cx)
        return NULL;

    return cx->priv->master_classes;
}


mrp_res_resource_t *mrp_res_create_resource(
                    mrp_res_resource_set_t *set,
                    const char *name,
                    bool mandatory,
                    bool shared)
{
    mrp_res_resource_t *res = NULL, *proto = NULL;
    uint32_t i = 0;
    bool found = false;
    uint32_t server_id = 0;
    mrp_res_context_t *cx = NULL;

    if (set == NULL)
        return NULL;

    cx = set->priv->cx;

    if (cx == NULL || name == NULL)
        return NULL;

    for (i = 0; i < cx->priv->master_resource_set->priv->num_resources; i++) {
        proto = cx->priv->master_resource_set->priv->resources[i];
        if (strcmp(proto->name, name) == 0) {
            found = true;
            server_id = proto->priv->server_id;
            break;
        }
    }

    if (!found)
        goto error;

    res = mrp_allocz(sizeof(mrp_res_resource_t));

    if (!res)
        goto error;

    res->name = mrp_strdup(name);

    res->state = MRP_RES_RESOURCE_PENDING;

    res->priv = mrp_allocz(sizeof(mrp_res_resource_private_t));

    if (!res->priv)
        goto error;

    res->priv->server_id = server_id;
    res->priv->mandatory = mandatory;
    res->priv->shared = shared;
    res->priv->pub = res;
    res->priv->set = set;

    /* copy the attributes with the default values */
    res->priv->attrs = mrp_attribute_array_dup(proto->priv->num_attributes,
                proto->priv->attrs);

    res->priv->num_attributes = proto->priv->num_attributes;

    /* add resource to resource set */
    set->priv->resources[set->priv->num_resources++] = res;

    return res;

error:
    mrp_res_error("mrp_res_create_resource error");
    free_resource(res);

    return NULL;
}


mrp_res_resource_set_t *mrp_res_copy_resource_set(
        const mrp_res_resource_set_t *original)
{
    mrp_res_resource_set_t *copy, *internal;
    mrp_res_context_t *cx = NULL;

    copy = resource_set_copy(original);

    if (!copy)
        goto error;

    cx = original->priv->cx;

    /* increase the reference count of the library resource set */

    internal = mrp_htbl_lookup(cx->priv->internal_rset_mapping,
            u_to_p(original->priv->internal_id));

    if (!internal)
        goto error;

    increase_ref(cx, internal);

    return copy;

error:
    mrp_log_error("error copying a resource set");
    free_resource_set(copy);
    return NULL;
}

const mrp_res_resource_set_t * mrp_res_list_resources(
        mrp_res_context_t *cx)
{
    if (cx == NULL || cx->priv == NULL)
        return NULL;

    return cx->priv->master_resource_set;
}


int mrp_res_release_resource_set(mrp_res_resource_set_t *original)
{
    mrp_res_resource_set_t *internal_set = NULL;
    mrp_res_context_t *cx = original->priv->cx;

    if (!cx || !cx->priv->connected)
        goto error;

    if (!original->priv->internal_id)
        goto error;

    internal_set = mrp_htbl_lookup(cx->priv->internal_rset_mapping,
            u_to_p(original->priv->internal_id));

    if (!internal_set)
        goto error;

    update_library_resource_set(cx, original, internal_set);

    if (internal_set->priv->id) {
        return release_resource_set_request(cx, internal_set);
    }
    else {
        mrp_list_hook_t *p, *n;
        mrp_res_resource_set_private_t *pending_rset;
        bool found = FALSE;

        /* Create the resource set if it doesn't already exist on the
         * server. The releasing is continued when the set is created.
         */

        /* only append if not already present in the list */

        mrp_list_foreach(&cx->priv->pending_sets, p, n) {
            pending_rset = mrp_list_entry(p, mrp_res_resource_set_private_t, hook);
            if (pending_rset == internal_set->priv) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            mrp_list_append(&cx->priv->pending_sets, &internal_set->priv->hook);
        }

        internal_set->priv->waiting_for = MRP_RES_PENDING_OPERATION_RELEASE;

        if (create_resource_set_request(cx, internal_set) < 0) {
            mrp_res_error("creating resource set failed");
            mrp_list_delete(&internal_set->priv->hook);
            goto error;
        }

        return 0;
    }

error:
    mrp_res_error("mrp_release_resources error");

    return -1;
}


bool mrp_res_equal_resource_set(const mrp_res_resource_set_t *a,
                const mrp_res_resource_set_t *b)
{
    if (!a || !b)
        return false;

    /* Compare the internal IDs to figure out if the both sets are result
     * of the same "create" call. */

    return a->priv->internal_id == b->priv->internal_id;
}


mrp_res_resource_set_t *mrp_res_create_resource_set(mrp_res_context_t *cx,
                        const char *app_class,
                        mrp_res_resource_callback_t cb,
                        void *userdata)
{
    if (cx == NULL)
        return NULL;

    return create_resource_set(cx, app_class, cb, userdata);
}


void mrp_res_delete_resource_set(mrp_res_resource_set_t *set)
{
    delete_resource_set(set);
}


void mrp_res_delete_resource(mrp_res_resource_t *res)
{
    if (res->priv->set) {
        if (!mrp_res_delete_resource_by_name(res->priv->set, res->name)) {
            /* hmm, strange */
            free_resource(res);
        }
    }
    else
        free_resource(res);
}


bool mrp_res_delete_resource_by_name(mrp_res_resource_set_t *rs, const char *name)
{
    uint32_t i;
    mrp_res_resource_t *res = NULL;

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
            (rs->priv->num_resources-i) * sizeof(mrp_res_resource_t *));

    rs->priv->num_resources--;
    rs->priv->resources[rs->priv->num_resources] = NULL;

    free_resource(res);

    return true;
}


mrp_res_string_array_t * mrp_res_list_resource_names(
                const mrp_res_resource_set_t *rs)
{
    uint32_t i;
    mrp_res_string_array_t *ret;

    if (!rs)
        return NULL;

    ret = mrp_allocz(sizeof(mrp_res_string_array_t));

    if (!ret)
        return NULL;

    ret->num_strings = rs->priv->num_resources;
    ret->strings = mrp_allocz_array(const char *, rs->priv->num_resources);

    if (!ret->strings) {
        mrp_free(ret);
        return NULL;
    }

    for (i = 0; i < rs->priv->num_resources; i++) {
        ret->strings[i] = mrp_strdup(rs->priv->resources[i]->name);
        if (!ret->strings[i]) {
            ret->num_strings = i;
            mrp_res_free_string_array(ret);
            return NULL;
        }
    }

    return ret;
}


mrp_res_resource_t * mrp_res_get_resource_by_name(
                 const mrp_res_resource_set_t *rs,
                 const char *name)
{
    uint32_t i;

    if (!rs)
        return NULL;

    for (i = 0; i < rs->priv->num_resources; i++) {
        if (strcmp(name, rs->priv->resources[i]->name) == 0) {
            return rs->priv->resources[i];
        }
    }

    return NULL;
}

bool mrp_res_set_autorelease(bool status,
        mrp_res_resource_set_t *rs)
{
    if (!rs || !rs->priv->cx)
        return FALSE;

     /* the resource library doesn't allow updating already  used sets */
    if (rs->state != MRP_RES_RESOURCE_PENDING)
        return FALSE;

    rs->priv->autorelease = status;

    return TRUE;
}


int mrp_res_acquire_resource_set(
                const mrp_res_resource_set_t *original)
{
    mrp_res_resource_set_t *rset;
    mrp_res_context_t *cx = original->priv->cx;

    if (!cx->priv->connected) {
        mrp_res_error("not connected to server");
        goto error;
    }

    rset = mrp_htbl_lookup(cx->priv->internal_rset_mapping,
            u_to_p(original->priv->internal_id));

    if (!rset) {
        mrp_res_error("trying to acquire non-existent resource set");
        goto error;
    }

    update_library_resource_set(cx, original, rset);

#if 0
    print_resource_set(rset);
#endif
    if (rset->priv->id) {
        /* the set has been already created on server */

        if (rset->state == MRP_RES_RESOURCE_ACQUIRED) {
            /* already requested, updating is not supported yet */
            mrp_res_error("trying to re-acquire already acquired set");

            /* TODO: when supported by backend
             * type = RESPROTO_UPDATE_RESOURCE_SET
             */
            goto error;
        }
        else {
            /* re-acquire a lost or released set */
            return acquire_resource_set_request(cx, rset);
        }
    }
    else {
        mrp_list_hook_t *p, *n;
        mrp_res_resource_set_private_t *pending_rset;
        bool found = FALSE;

        /* Create the resource set. The acquisition is continued
         * when the set is created. */

        /* only append if not already present in the list */

        mrp_list_foreach(&cx->priv->pending_sets, p, n) {
            pending_rset = mrp_list_entry(p, mrp_res_resource_set_private_t, hook);
            if (pending_rset == rset->priv) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            mrp_list_append(&cx->priv->pending_sets, &rset->priv->hook);
        }

        rset->priv->waiting_for = MRP_RES_PENDING_OPERATION_ACQUIRE;

        if (create_resource_set_request(cx, rset) < 0) {
            mrp_res_error("creating resource set failed");
            mrp_list_delete(&rset->priv->hook);
            goto error;
        }
    }

    return 0;

error:
    mrp_log_error("error acquiring a resource set");
    return -1;
}


int mrp_res_get_resource_set_id(mrp_res_resource_set_t *rs)
{
    mrp_res_resource_set_t *internal_set;
    mrp_res_context_t *cx = NULL;

    if (!rs || !rs->priv || !rs->priv->cx)
        return 0;

    cx = rs->priv->cx;

    internal_set = mrp_htbl_lookup(cx->priv->internal_rset_mapping,
            u_to_p(rs->priv->internal_id));

    if (!internal_set || !internal_set->priv)
        return 0;

    return internal_set->priv->id;
}
