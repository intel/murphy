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

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include <murphy-db/mql-result.h>

#include "domain-control-types.h"
#include "message.h"
#include "table.h"
#include "notify.h"


static void prepare_proxy_notification(pep_proxy_t *proxy)
{
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = false;
}


static int collect_watch_notification(pep_watch_t *w)
{
    pep_proxy_t  *proxy = w->proxy;
    mql_result_t *r     = NULL;
    int           n;

    mrp_debug("updating %s watch for %s", w->table->name, proxy->name);

    if (proxy->notify_msg == NULL) {
        if (!proxy->ops->create_notify(proxy))
            goto fail;
    }

    if (w->table->h != MQI_HANDLE_INVALID) {
        if (!exec_mql(mql_result_rows, &r, "select %s from %s%s%s",
                      w->mql_columns, w->table->name,
                      w->mql_where[0] ? " where " : "", w->mql_where)) {
            mrp_debug("select from table %s failed", w->table->name);
            goto fail;
        }
    }

    n = proxy->ops->update_notify(proxy, w->id, r);

    if (r != NULL)
        mql_result_free(r);

    if (n >= 0)
        return TRUE;
    else {
    fail:
        proxy->ops->free_notify(proxy);
        proxy->notify_fail = true;

        return FALSE;
    }
}


static int send_proxy_notification(pep_proxy_t *proxy)
{
    if (proxy->notify_msg == NULL)
        return TRUE;

    if (!proxy->notify_fail) {
        mrp_debug("notifying client %s", proxy->name);

        proxy->ops->send_notify(proxy);
        proxy->ops->free_notify(proxy);
    }
    else
        mrp_log_error("Failed to generate/send notification to %s.",
                      proxy->name);

    proxy->notify_msg     = NULL;
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = false;

    return TRUE;
}


void notify_table_changes(pdp_t *pdp)
{
    mrp_list_hook_t *p, *n, *wp, *wn;
    pep_proxy_t     *proxy;
    pep_table_t     *t;
    pep_watch_t     *w;

    mrp_debug("notifying clients about table changes");

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);
        prepare_proxy_notification(proxy);
    }

    mrp_list_foreach(&pdp->tables, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        mrp_debug("table '%s' has %s changes", t->name,
                  t->changed ? "unsynced" : "no");

        if (!t->changed)
            continue;

        mrp_list_foreach(&t->watches, wp, wn) {
            w = mrp_list_entry(wp, typeof(*w), tbl_hook);
            w->proxy->notify = true;
        }
    }

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);

        mrp_debug("proxy %s needs %supdate", proxy->name,
                  proxy->notify ? "" : "no ");

        if (proxy->notify) {
            mrp_list_foreach(&proxy->watches, wp, wn) {
                w = mrp_list_entry(wp, typeof(*w), pep_hook);
                if (!collect_watch_notification(w))
                    break;
            }

            send_proxy_notification(proxy);

            proxy->notify = false;
        }
    }

    mrp_list_foreach(&pdp->tables, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);
        t->changed = false;
    }
}
