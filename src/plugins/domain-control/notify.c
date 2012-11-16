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
    proxy->notify_update  = FALSE;
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = FALSE;
}


static void check_watch_notification(pep_watch_t *w)
{
    pep_proxy_t *proxy = w->proxy;
    pep_table_t *t     = w->table;
    int          update;

    if (t->notify_all) {
        t->h   = mqi_get_table_handle(t->name);
        update = TRUE;
    }
    else {
        if (t->h != MQI_HANDLE_INVALID)
            update = (w->stamp < mqi_get_table_stamp(t->h));
        else
            update = FALSE;
    }

    proxy->notify_update |= update;
}


static int collect_watch_notification(pep_watch_t *w)
{
    pep_proxy_t  *proxy = w->proxy;
    pep_table_t  *t     = w->table;
    mql_result_t *r     = NULL;
    uint16_t      tid   = w->id;
    uint16_t      nrow, ncol;
    mrp_msg_t    *msg;
    int           i, j;
    int           types[MQI_COLUMN_MAX];
    const char   *str;
    uint32_t      u32;
    int32_t       s32;
    double        dbl;

    mrp_debug("updating %s watch for %s", t->name, proxy->name);

    if (proxy->notify_msg == NULL) {
        proxy->notify_msg = create_notify_message();

        if (proxy->notify_msg == NULL)
            goto fail;
    }

    if (t->h != MQI_HANDLE_INVALID) {
        if (!exec_mql(mql_result_rows, &r, "select %s from %s%s%s",
                      w->mql_columns, t->name,
                     w->mql_where[0] ? " where " : "", w->mql_where)) {
            mrp_debug("select from table %s failed", t->name);
            goto fail;
        }
    }

    if (r != NULL) {
        nrow = mql_result_rows_get_row_count(r);
        ncol = mql_result_rows_get_row_column_count(r);
    }
    else
        nrow = ncol = 0;

    msg = proxy->notify_msg;

    if (!mrp_msg_append(msg, MRP_PEPMSG_UINT16(TBLID, tid))  ||
        !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NROW , nrow)) ||
        !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NCOL , ncol)))
        goto fail;

    for (i = 0; i < ncol; i++)
        types[i] = mql_result_rows_get_row_column_type(r, i);

    for (i = 0; i < nrow; i++) {
        for (j = 0; j < ncol; j++) {
            switch (types[j]) {
            case mqi_string:
                str = mql_result_rows_get_string(r, j, i, NULL, 0);
                if (!mrp_msg_append(msg, MRP_PEPMSG_STRING(DATA, str)))
                    goto fail;
                break;
            case mqi_integer:
                s32 = mql_result_rows_get_integer(r, j, i);
                if (!mrp_msg_append(msg, MRP_PEPMSG_SINT32(DATA, s32)))
                    goto fail;
                break;
            case mqi_unsignd:
                u32 = mql_result_rows_get_unsigned(r, j, i);
                if (!mrp_msg_append(msg, MRP_PEPMSG_UINT32(DATA, u32)))
                    goto fail;
                break;

            case mqi_floating:
                dbl = mql_result_rows_get_floating(r, j, i);
                if (!mrp_msg_append(msg, MRP_PEPMSG_DOUBLE(DATA, dbl)))
                    goto fail;
                break;

            default:
                goto fail;
            }
        }
    }

    if (r != NULL)
        mql_result_free(r);

    proxy->notify_ncolumn += nrow * ncol;
    proxy->notify_ntable++;

    return TRUE;

 fail:
    if (r != NULL)
        mql_result_free(r);
    mrp_msg_unref(proxy->notify_msg);
    proxy->notify_msg = NULL;
    proxy->notify_fail = TRUE;

    return FALSE;
}


static int send_proxy_notification(pep_proxy_t *proxy)
{
    uint16_t nchange, ntotal;

    if (proxy->notify_msg == NULL)
        return TRUE;

    if (!proxy->notify_fail) {
        mrp_debug("notifying client %s", proxy->name);

        nchange = proxy->notify_ntable;
        ntotal  = proxy->notify_ncolumn;

        mrp_msg_set(proxy->notify_msg, MRP_PEPMSG_UINT16(NCHANGE, nchange));
        mrp_msg_set(proxy->notify_msg, MRP_PEPMSG_UINT16(NTOTAL , ntotal ));

        /*
          mrp_log_info("Notification message for client %s:", proxy->name);
          mrp_msg_dump(proxy->notify_msg, stdout);
        */

        mrp_transport_send(proxy->t, proxy->notify_msg);
    }
    else
        mrp_log_error("Failed to generate/send notification to %s.",
                      proxy->name);

    mrp_msg_unref(proxy->notify_msg);

    proxy->notify_msg     = NULL;
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = FALSE;
    proxy->notify_all     = FALSE;

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

        mrp_list_foreach(&t->watches, wp, wn) {
            w = mrp_list_entry(wp, typeof(*w), tbl_hook);
            check_watch_notification(w);
        }

        t->notify_all = FALSE;
    }

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);

        if (proxy->notify_update || proxy->notify_all) {
            mrp_list_foreach(&proxy->watches, wp, wn) {
                w = mrp_list_entry(wp, typeof(*w), pep_hook);
                if (!collect_watch_notification(w))
                    break;
            }

            send_proxy_notification(proxy);
        }
    }
}
