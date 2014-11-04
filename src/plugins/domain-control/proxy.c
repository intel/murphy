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

#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "domain-control-types.h"
#include "table.h"
#include "proxy.h"


/*
 * a pending proxied invocation
 */

typedef struct {
    mrp_list_hook_t         hook;        /* to pending list */
    uint32_t                id;          /* request id */
    mrp_domain_return_cb_t  cb;          /* return callback */
    void                   *user_data;   /* opaque callback data */
} pending_t;

static void purge_pending(pep_proxy_t *proxy);


int init_proxies(pdp_t *pdp)
{
    mrp_list_init(&pdp->proxies);

    return TRUE;
}


void destroy_proxies(pdp_t *pdp)
{
    MRP_UNUSED(pdp);

    return;
}


pep_proxy_t *create_proxy(pdp_t *pdp)
{
    pep_proxy_t *proxy;

    proxy = mrp_allocz(sizeof(*proxy));

    if (proxy != NULL) {
        mrp_list_init(&proxy->hook);
        mrp_list_init(&proxy->watches);
        mrp_list_init(&proxy->pending);

        proxy->pdp   = pdp;
        proxy->seqno = 1;

        mrp_list_append(&pdp->proxies, &proxy->hook);
    }

    return proxy;
}


void destroy_proxy(pep_proxy_t *proxy)
{
    int i;

    if (proxy != NULL) {
        mrp_list_delete(&proxy->hook);

        for (i = 0; i < proxy->ntable; i++)
            destroy_proxy_table(proxy->tables + i);

        destroy_proxy_watches(proxy);

        purge_pending(proxy);

        mrp_free(proxy);
    }
}


int register_proxy(pep_proxy_t *proxy, char *name,
                   mrp_domctl_table_t *tables, int ntable,
                   mrp_domctl_watch_t *watches, int nwatch,
                   int *error, const char **errmsg)
{
    pep_table_t        *t;
    mrp_domctl_watch_t *w;
    int                 i;

    proxy->name   = mrp_strdup(name);
    proxy->tables = mrp_allocz_array(typeof(*proxy->tables) , ntable);
    proxy->ntable = ntable;
    proxy->notify = true;

    if (proxy->name == NULL || (ntable && proxy->tables == NULL)) {
        *error  = ENOMEM;
        *errmsg = "failed to allocate proxy table";

        return FALSE;
    }

    for (i = 0, t = proxy->tables; i < ntable; i++, t++) {
        t->h           = MQI_HANDLE_INVALID;
        t->name        = mrp_strdup(tables[i].table);
        t->mql_columns = mrp_strdup(tables[i].mql_columns);
        t->mql_index   = mrp_strdup(tables[i].mql_index);

        if (t->name == NULL || t->mql_columns == NULL || t->mql_index == NULL) {
            mrp_log_error("Failed to allocate proxy table %s for %s.",
                          tables[i].table, name);
            *error  = ENOMEM;
            *errmsg = "failed to allocate proxy table";

            return FALSE;
        }

        if (create_proxy_table(t, error, errmsg))
            mrp_log_info("Client %s created table %s.", proxy->name,
                         tables[i].table);
        else {
            mrp_log_error("Client %s failed to create table %s (%d: %s).",
                          proxy->name, tables[i].table, *error, *errmsg);
            return FALSE;
        }
    }

    for (i = 0, w = watches; i < nwatch; i++, w++) {
        if (create_proxy_watch(proxy, i, w->table, w->mql_columns,
                               w->mql_where, w->max_rows, error, errmsg))
            mrp_log_info("Client %s subscribed for table %s.", proxy->name,
                         w->table);
        else
            mrp_log_error("Client %s failed to subscribe for table %s.",
                          proxy->name, w->table);
    }

    return TRUE;
}


int unregister_proxy(pep_proxy_t *proxy)
{
    destroy_proxy(proxy);

    return TRUE;
}


pep_proxy_t *find_proxy(pdp_t *pdp, const char *name)
{
    mrp_list_hook_t *p, *n;
    pep_proxy_t     *proxy;

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);

        if (!strcmp(proxy->name, name))
            return proxy;
    }

    return NULL;
}


uint32_t proxy_queue_pending(pep_proxy_t *proxy,
                             mrp_domain_return_cb_t return_cb, void *user_data)
{
    pending_t *pending;

    if (return_cb == NULL)
        return proxy->seqno++;

    pending = mrp_allocz(sizeof(*pending));

    if (pending == NULL)
        return 0;

    mrp_list_init(&pending->hook);

    pending->id        = proxy->seqno++;
    pending->cb        = return_cb;
    pending->user_data = user_data;

    mrp_list_append(&proxy->pending, &pending->hook);

    return pending->id;
}


int proxy_dequeue_pending(pep_proxy_t *proxy, uint32_t id,
                          mrp_domain_return_cb_t *cbp, void **user_datap)
{
    mrp_list_hook_t *p, *n;
    pending_t       *pending;

    mrp_list_foreach(&proxy->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        if (pending->id == id) {
            mrp_list_delete(&pending->hook);
            *cbp        = pending->cb;
            *user_datap = pending->user_data;

            mrp_free(pending);

            return TRUE;
        }
    }

    return FALSE;
}


static void purge_pending(pep_proxy_t *proxy)
{
    mrp_list_hook_t *p, *n;
    pending_t       *pending;

    mrp_list_foreach(&proxy->pending, p, n) {
        pending = mrp_list_entry(p, typeof(*pending), hook);

        mrp_list_delete(&pending->hook);
        mrp_free(pending);
    }
}
