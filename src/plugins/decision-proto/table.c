#include <errno.h>

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>

#include <murphy-db/mqi.h>

#include "decision.h"
#include "table.h"

#define FAIL(ec, msg) do {                      \
        *errcode = ec;                          \
        *errmsg = msg;                          \
        goto fail;                              \
    } while (0)

static pep_table_t *lookup_watch_table(pdp_t *pdp, const char *name);

#include "table-common.c"

/*
 * proxied and tracked tables
 */


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
        t->notify_all = TRUE;
        t->h          = h;
    }

    schedule_notification(pdp);
}


static void transaction_event_cb(mqi_event_t *e, void *user_data)
{
    pdp_t *pdp = (pdp_t *)user_data;

    switch (e->event) {
    case mqi_transaction_end:
        mrp_debug("transaction ended");
        schedule_notification(pdp);
        break;

    case mqi_transaction_start:
        mrp_debug("transaction started");
        break;

    default:
        break;
    }
}


static int open_db(pdp_t *pdp)
{
    if (mqi_open() == 0) {
        if (mqi_create_transaction_trigger(transaction_event_cb, pdp) == 0) {
            if (mqi_create_table_trigger(table_event_cb, pdp) == 0)
                return TRUE;
            else
                mqi_drop_transaction_trigger(transaction_event_cb, pdp);
        }
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


int create_proxy_table(pep_table_t *t, mrp_pep_table_t *def,
                       int *errcode, const char **errmsg)
{
    mqi_column_def_t  **cols;
    mqi_column_desc_t **desc;
    int                *ncol;
    char               *index[2];

    if (mqi_get_table_handle((char *)def->name) != MQI_HANDLE_INVALID)
        FAIL(EEXIST, "table already exists");

    if (def->idx_col >= def->ncolumn)
        FAIL(EINVAL, "invalid index column specified");

    mrp_list_init(&t->hook);
    mrp_list_init(&t->watches);

    t->name = mrp_strdup(def->name);

    if (t->name != NULL) {
        cols = &t->columns;
        ncol = &t->ncolumn;
        desc = &t->coldesc;

        if (!copy_column_definitions(def->columns, def->ncolumn, cols, ncol))
            FAIL(ENOMEM, "failed to create table columns");

        if (!setup_column_descriptors(t->columns, t->ncolumn, desc))
            FAIL(ENOMEM, "failed to create table descriptor");

        t->h = mqi_create_table(t->name, MQI_TEMPORARY, NULL, t->columns);

        if (t->h != MQI_HANDLE_INVALID) {
            if (def->idx_col >= 0) {
                index[0] = (char *)def->columns[def->idx_col].name;
                index[1] = NULL;

                if (mqi_create_index(t->h, index) != 0)
                    FAIL(EINVAL, "failed to create table index");
            }

            mrp_debug("create table %s", t->name);

            return TRUE;
        }
        else
            FAIL(EINVAL, "failed to create table");
    }
    else
        FAIL(ENOMEM, "failed to create table");

 fail:
    return FALSE;
}


void destroy_proxy_table(pep_table_t *t)
{
    mrp_debug("destroying table %s", t->name ? t->name : "<unknown>");

    if (t->h != MQI_HANDLE_INVALID)
        mqi_drop_table(t->h);

    free_column_definitions(t->columns, t->ncolumn);
    free_column_descriptors(t->coldesc);

    mrp_free(t->name);

    t->name    = NULL;
    t->h       = MQI_HANDLE_INVALID;
    t->columns = NULL;
    t->ncolumn = 0;
}


void destroy_proxy_tables(pep_proxy_t *proxy)
{
    int i;

    mrp_debug("destroying tables of client %s", proxy->name);

    for (i = 0; i < proxy->ntable; i++)
        destroy_proxy_table(proxy->tables + i);

    proxy->tables = NULL;
    proxy->ntable = 0;
}


pep_table_t *create_watch_table(pdp_t *pdp, const char *name,
                                mqi_column_def_t *columns, int ncolumn)
{
    pep_table_t        *t;
    mqi_column_def_t  **cols;
    mqi_column_desc_t **desc;
    int                *ncol;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        mrp_list_init(&t->hook);
        mrp_list_init(&t->watches);

        t->name = mrp_strdup(name);

        if (t->name == NULL)
            goto fail;

        cols = &t->columns;
        ncol = &t->ncolumn;
        desc = &t->coldesc;

        if (!copy_column_definitions(columns, ncolumn, cols, ncol))
            goto fail;

        if (!setup_column_descriptors(t->columns, t->ncolumn, desc))
            goto fail;

        t->h = mqi_get_table_handle(t->name);

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
        mrp_list_foreach(&t->watches, p, n) {
            w = mrp_list_entry(p, typeof(*w), tbl_hook);

            mrp_list_delete(&w->tbl_hook);
            mrp_list_delete(&w->pep_hook);

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


int create_proxy_watch(pep_proxy_t *proxy, int id, mrp_pep_table_t *def,
                       int *error, const char **errmsg)
{
    pdp_t       *pdp = proxy->pdp;
    pep_table_t *t;
    pep_watch_t *w;

    t = lookup_watch_table(pdp, def->name);

    if (t == NULL) {
        t = create_watch_table(pdp, def->name, def->columns, def->ncolumn);

        if (t == NULL) {
            *error  = EINVAL;
            *errmsg = "failed to watch table";
        }
    }
    else {
        if (!check_columns(t->columns, t->ncolumn, def->columns, def->ncolumn)){
            *error  = EINVAL;
            *errmsg = "table columns don't match";
            t = NULL;
        }
    }

    if (t == NULL)
        return FALSE;

    w = mrp_allocz(sizeof(*w));

    if (w != NULL) {
        mrp_list_init(&w->tbl_hook);
        mrp_list_init(&w->pep_hook);

        w->table = t;
        w->proxy = proxy;
        w->id    = id;

        mrp_list_append(&t->watches, &w->tbl_hook);
        mrp_list_append(&proxy->watches, &w->pep_hook);

        return TRUE;
    }
    else {
        *error  = ENOMEM;
        *errmsg = "failed to allocate table watch";
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


static int insert_into_table(pep_table_t *t, mrp_pep_value_t *values, int nrow)
{
    int   i;
    void *data[2];

    data[1] = NULL;

    for (i = 0; i < nrow; i++) {
        data[0] = values;
        if (mqi_insert_into(t->h, 0, t->coldesc, data) != 1)
            return FALSE;
        else
            values += t->ncolumn;
    }

    return TRUE;
}


int set_proxy_tables(pep_proxy_t *proxy, mrp_pep_data_t *tables, int ntable,
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
            if (!delete_from_table(t, tables[i].columns, tables[i].nrow))
                goto fail;
#endif

            if (!insert_into_table(t, tables[i].columns, tables[i].nrow))
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
