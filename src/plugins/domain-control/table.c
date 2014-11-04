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

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>

#include <murphy-db/mqi.h>
#include <murphy-db/mql.h>
#include <murphy-db/mdb.h>

#include "domain-control.h"
#include "table.h"

#define FAIL(ec, msg) do {                      \
        *errcode = ec;                          \
        *errmsg = msg;                          \
        goto fail;                              \
    } while (0)

static pep_table_t *lookup_watch_table(pdp_t *pdp, const char *name);

/*
 * proxied and tracked tables
 */


static void table_change_cb(mqi_event_t *e, void *tptr)
{
    static const char *events[] = {
        "unknown (?)",
        "column change",
        "row insert",
        "row delete",
        "table create",
        "table drop",
        "transaction start (?)",
        "transaction end (?)",
    };
    pep_table_t *t = (pep_table_t *)tptr;

    if (!t->changed) {
        t->changed = true;
        mrp_debug("table '%s' changed by %s event", t->name, events[e->event]);
    }
}


static int add_table_triggers(pep_table_t *t)
{
    mdb_table_t      *tbl;
    mqi_column_def_t  cols[256];
    int               ncol, i;

    if (t->h == MQI_HANDLE_INVALID) {
        errno = EAGAIN;
        return -1;
    }

    if ((tbl = mdb_table_find(t->name)) == NULL) {
        errno = EINVAL;
        return -1;
    }

    if ((ncol = mdb_table_describe(tbl, &cols[0], sizeof(cols))) <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (mdb_trigger_add_row_callback(tbl, table_change_cb, t, NULL)) {
        errno = EINVAL;
        return -1;
    }

    for (i = 0; i < ncol; i++) {
        if (mdb_trigger_add_column_callback(tbl, i, table_change_cb,
                                            t, NULL) < 0) {
            mdb_trigger_delete_row_callback(tbl, table_change_cb, t);
            errno = EINVAL;
            return -1;
        }
    }

    return 0;
}


static void del_table_triggers(pep_table_t *t)
{
    mdb_table_t      *tbl;
    mqi_column_def_t  cols[256];
    int               ncol, i;

    if (t->h == MQI_HANDLE_INVALID)
        return;

    if ((tbl = mdb_table_find(t->name)) == NULL)
        return;

    ncol = mdb_table_describe(tbl, &cols[0], sizeof(cols));

    mdb_trigger_delete_row_callback(tbl, table_change_cb, t);

    for (i = 0; i < ncol; i++)
        mdb_trigger_delete_column_callback(tbl, i, table_change_cb, t);
}


static void table_event_cb(mqi_event_t *e, void *user_data)
{
    pdp_t        *pdp  = (pdp_t *)user_data;
    const char   *name = e->table.table.name;
    mqi_handle_t  h    = e->table.table.handle;
    pep_table_t  *t;

    switch (e->event) {
    case mqi_table_created:
        mrp_debug("table %s (0x%x) created", name, h);
        break;

    case mqi_table_dropped:
        mrp_debug("table %s (0x%x) dropped", name, h);
        break;

    default:
        return;
    }

    t = lookup_watch_table(pdp, name);

    if (t != NULL) {
        t->changed = true;

        if (e->event == mqi_table_created) {
            t->h = h;
            add_table_triggers(t);
        }
        else {
            t->h = MQI_HANDLE_INVALID;
            del_table_triggers(t);
        }
    }

    schedule_notification(pdp);
}


static void transaction_event_cb(mqi_event_t *e, void *user_data)
{
    pdp_t *pdp   = (pdp_t *)user_data;
    int    depth = e->transact.depth;

    switch (e->event) {
    case mqi_transaction_end:
        if (depth == 1) {
            mrp_debug("outermost transaction ended");

            if (pdp->ractive) {
                mrp_debug("resolver active, delaying client notifications");
                pdp->rblocked = true;
            }
            else
                schedule_notification(pdp);
        }
        else
            mrp_debug("nested transaction (#%d) ended", depth);
        break;

    case mqi_transaction_start:
        if (depth == 1)
            mrp_debug("outermost transaction started");
        else
            mrp_debug("nested transaction (#%d) started", depth);
        break;

    default:
        break;
    }
}


static int open_db(pdp_t *pdp)
{
    static bool done = false;

    if (done)
        return TRUE;

    if (mqi_open() == 0) {
        if (mqi_create_transaction_trigger(transaction_event_cb, pdp) == 0 &&
            mqi_create_table_trigger(table_event_cb, pdp) == 0) {
            done = true;
            return TRUE;
        }

        mqi_drop_transaction_trigger(transaction_event_cb, pdp);
    }

    return FALSE;
}


static void close_db(pdp_t *pdp)
{
    mqi_drop_table_trigger(table_event_cb, pdp);
    mqi_drop_transaction_trigger(transaction_event_cb, pdp);
}


static void purge_watch_table_cb(void *key, void *entry);



int init_tables(pdp_t *pdp)
{
    mrp_htbl_config_t hcfg;

    if (open_db(pdp)) {
        mrp_list_init(&pdp->tables);

        mrp_clear(&hcfg);
        hcfg.comp = mrp_string_comp;
        hcfg.hash = mrp_string_hash;
        hcfg.free = purge_watch_table_cb;

        pdp->watched = mrp_htbl_create(&hcfg);
    }

    return (pdp->watched != NULL);
}


void destroy_tables(pdp_t *pdp)
{
    close_db(pdp);
    mrp_htbl_destroy(pdp->watched, TRUE);

    pdp->watched = NULL;
}


int exec_mql(mql_result_type_t type, mql_result_t **resultp,
             const char *format, ...)
{
    mql_result_t *r;
    char          buf[4096];
    va_list       ap;
    int           success, n;

    va_start(ap, format);
    n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (n < (int)sizeof(buf)) {
        r       = mql_exec_string(type, buf);
        success = (r == NULL || mql_result_is_success(r));

        if (resultp != NULL) {
            *resultp = r;
            return success;
        }
        else {
            mql_result_free(r);
            return success;
        }
    }
    else {
        errno = EOVERFLOW;
        if (resultp != NULL)
            *resultp = NULL;

        return FALSE;
    }
}


static int get_table_description(pep_table_t *t)
{
    mqi_column_def_t    columns[MQI_COLUMN_MAX];
    mrp_domctl_value_t *values = NULL;
    int                 ncolumn, i;

    if (t->h == MQI_HANDLE_INVALID)
        t->h = mqi_get_table_handle((char *)t->name);

    if (t->h != MQI_HANDLE_INVALID) {
        ncolumn = mqi_describe(t->h, columns, MRP_ARRAY_SIZE(columns));

        if (ncolumn > 0) {
            t->columns = mrp_allocz_array(typeof(*t->columns), ncolumn);
            t->coldesc = mrp_allocz_array(typeof(*t->coldesc), ncolumn + 1);

            if (t->columns != NULL && t->coldesc != NULL) {
                memcpy(t->columns, columns, ncolumn * sizeof(*t->columns));
                t->ncolumn = ncolumn;

                for (i = 0; i < t->ncolumn; i++) {
                    t->coldesc[i].cindex = i;
                    t->coldesc[i].offset = (int)(ptrdiff_t)&values[i].str;
                }

                t->coldesc[i].cindex = -1;
                t->coldesc[i].offset = 0;

                return TRUE;
            }
        }
    }

    return FALSE;
}


int create_proxy_table(pep_table_t *t, int *errcode, const char **errmsg)
{
    mrp_list_init(&t->hook);
    mrp_list_init(&t->watches);

    if (mqi_get_table_handle((char *)t->name) != MQI_HANDLE_INVALID)
        FAIL(EEXIST, "DB error: table already exists");

    if (exec_mql(mql_result_dontcare, NULL,
                 "create temporary table %s (%s)", t->name, t->mql_columns)) {
        if (t->mql_index && t->mql_index[0]) {
            if (!exec_mql(mql_result_dontcare, NULL,
                          "create index on %s (%s)", t->name, t->mql_index))
                FAIL(EINVAL, "DB error: failed to create table index");
        }

        if (!get_table_description(t))
            FAIL(EINVAL, "DB error: failed to get table description");

        return TRUE;
    }
    else
        FAIL(ENOMEM, "DB error: failed to create table");

 fail:
    return FALSE;
}


void destroy_proxy_table(pep_table_t *t)
{
    mrp_debug("destroying table %s", t->name ? t->name : "<unknown>");

    if (t->h != MQI_HANDLE_INVALID)
        mqi_drop_table(t->h);

    mrp_free(t->mql_columns);
    mrp_free(t->mql_index);

    mrp_free(t->columns);
    mrp_free(t->coldesc);
    mrp_free(t->name);

    t->name    = NULL;
    t->h       = MQI_HANDLE_INVALID;
    t->columns = NULL;
    t->ncolumn = 0;
}


void destroy_proxy_tables(pep_proxy_t *proxy)
{
    mqi_handle_t tx;
    int          i;

    mrp_debug("destroying tables of client %s", proxy->name);

    tx = mqi_begin_transaction();
    for (i = 0; i < proxy->ntable; i++)
        destroy_proxy_table(proxy->tables + i);
    mqi_commit_transaction(tx);

    proxy->tables = NULL;
    proxy->ntable = 0;
}


pep_table_t *create_watch_table(pdp_t *pdp, const char *name)
{
    pep_table_t *t;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        mrp_list_init(&t->hook);
        mrp_list_init(&t->watches);

        t->h    = MQI_HANDLE_INVALID;
        t->name = mrp_strdup(name);

        if (t->name == NULL)
            goto fail;

        get_table_description(t);

        if (t->h != MQI_HANDLE_INVALID)
            add_table_triggers(t);

        if (!mrp_htbl_insert(pdp->watched, t->name, t))
            goto fail;

        mrp_list_append(&pdp->tables, &t->hook);
    }

    return t;

 fail:
    destroy_watch_table(pdp, t);

    return FALSE;
}


static void destroy_table_watches(pep_table_t *t)
{
    pep_watch_t     *w;
    mrp_list_hook_t *p, *n;

    if (t != NULL) {
        del_table_triggers(t);

        mrp_list_foreach(&t->watches, p, n) {
            w = mrp_list_entry(p, typeof(*w), tbl_hook);

            mrp_list_delete(&w->tbl_hook);
            mrp_list_delete(&w->pep_hook);

            mrp_free(w->mql_columns);
            mrp_free(w->mql_where);
            mrp_free(w);
        }
    }
}


void destroy_watch_table(pdp_t *pdp, pep_table_t *t)
{
    mrp_list_delete(&t->hook);
    t->h = MQI_HANDLE_INVALID;

    if (pdp != NULL)
        mrp_htbl_remove(pdp->watched, t->name, FALSE);

    destroy_table_watches(t);
}


static pep_table_t *lookup_watch_table(pdp_t *pdp, const char *name)
{
    return mrp_htbl_lookup(pdp->watched, (void *)name);
}


static void purge_watch_table_cb(void *key, void *entry)
{
    pep_table_t *t = (pep_table_t *)entry;

    MRP_UNUSED(key);

    destroy_watch_table(NULL, t);
}


int create_proxy_watch(pep_proxy_t *proxy, int id,
                       const char *table, const char *mql_columns,
                       const char *mql_where, int max_rows,
                       int *error, const char **errmsg)
{
    pdp_t       *pdp = proxy->pdp;
    pep_table_t *t;
    pep_watch_t *w;

    t = lookup_watch_table(pdp, table);

    if (t == NULL) {
        t = create_watch_table(pdp, table);

        if (t == NULL) {
            *error  = EINVAL;
            *errmsg = "failed to watch table";
        }
    }

    w = mrp_allocz(sizeof(*w));

    if (w != NULL) {
        mrp_list_init(&w->tbl_hook);
        mrp_list_init(&w->pep_hook);

        w->table        = t;
        w->mql_columns  = mrp_strdup(mql_columns);
        w->mql_where    = mrp_strdup(mql_where ? mql_where : "");
        w->max_rows     = max_rows;
        w->proxy        = proxy;
        w->id           = id;
        w->notify       = true;

        if (w->mql_columns == NULL || w->mql_where == NULL)
            goto fail;

        mrp_list_append(&t->watches, &w->tbl_hook);
        mrp_list_append(&proxy->watches, &w->pep_hook);

        return TRUE;
    }
    else {
        *error  = ENOMEM;
        *errmsg = "failed to allocate table watch";
    }

 fail:
    if (w != NULL) {
        mrp_free(w->mql_columns);
        mrp_free(w->mql_where);
        mrp_free(w);
    }

    return FALSE;
}


void destroy_proxy_watches(pep_proxy_t *proxy)
{
    pep_watch_t     *w;
    mrp_list_hook_t *p, *n;

    if (proxy != NULL) {
        mrp_list_foreach(&proxy->watches, p, n) {
            w = mrp_list_entry(p, typeof(*w), pep_hook);

            mrp_list_delete(&w->tbl_hook);
            mrp_list_delete(&w->pep_hook);

            mrp_free(w);
        }
    }
}


static void reset_proxy_tables(pep_proxy_t *proxy)
{
    int i;

    for (i = 0; i < proxy->ntable; i++)
        mqi_delete_from(proxy->tables[i].h, NULL);
}


static int insert_into_table(pep_table_t *t,
                             mrp_domctl_value_t **rows, int nrow)
{
    void *data[2];
    int   i;

    data[1] = NULL;

    for (i = 0; i < nrow; i++) {
        data[0] = rows[i];
        if (mqi_insert_into(t->h, 0, t->coldesc, data) != 1)
            return FALSE;
    }

    return TRUE;
}


int set_proxy_tables(pep_proxy_t *proxy, mrp_domctl_data_t *tables, int ntable,
                     int *error, const char **errmsg)
{
    mqi_handle_t    tx;
    pep_table_t    *t;
    int             i, id;

    tx = mqi_begin_transaction();

    if (tx != MQI_HANDLE_INVALID) {
        reset_proxy_tables(proxy);

        for (i = 0; i < ntable; i++) {
            id = tables[i].id;

            if (id < 0 || id >= proxy->ntable)
                goto fail;

            t = proxy->tables + id;

            if (tables[i].ncolumn != t->ncolumn)
                goto fail;

#if 0
            if (!delete_from_table(t, tables[i].rows, tables[i].nrow))
                goto fail;
#endif

            if (!insert_into_table(t, tables[i].rows, tables[i].nrow))
                goto fail;


        }

        mqi_commit_transaction(tx);

        return TRUE;

    fail:
        *error  = EINVAL;
        *errmsg = "failed to set tables";
        mqi_rollback_transaction(tx);
    }

    return FALSE;
}
