#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <alloca.h>
#include <errno.h>

#include <murphy-db/assert.h>
#include <murphy-db/mql.h>
#include <murphy-db/hash.h>
#include "mql-parser.h"

#ifndef MQL_CALLBACK_HASH_CHAINS
#define MQL_CALLBACK_HASH_CHAINS  128
#endif

#ifndef MQL_TRIGGER_HASH_CHAINS
#define MQL_TRIGGER_HASH_CHAINS  128
#endif


typedef enum trigger_type_e          trigger_type_t;
typedef struct select_s              select_t;
typedef struct column_s              column_t;
typedef struct trigger_s             trigger_t;
typedef struct transation_trigger_s  transation_trigger_t;
typedef struct table_trigger_s       table_trigger_t;
typedef struct row_trigger_s         row_trigger_t;
typedef struct column_trigger_s      column_trigger_t;


struct mql_callback_s {
    int                refcnt;
    char              *name;
    mql_result_type_t  rtype;
    mql_trigger_cb_t   function;
    void              *user_data;
};

enum trigger_type_e {
    trigger_unknown = 0,
    trigger_first = trigger_unknown,

    trigger_transaction,
    trigger_table,
    trigger_row,
    trigger_column,

    trigger_last
};

#define TRIGGER_COMMON                          \
    char               *name;                   \
    trigger_type_t      type;                   \
    mql_callback_t     *callback

struct select_s {
    struct {
        int                 ncol;
        char              **names;
        mqi_column_desc_t  *descs;
        mqi_data_type_t    *types;
        int                *sizes;
    }                   column;
    int                 rowsize;
    struct {
        char   *addr;
        size_t  size;
    }                   strpool;
};

struct column_s {
    int                 index;
    mqi_data_type_t     type;
};

struct trigger_s {
    TRIGGER_COMMON;
};

struct transation_trigger_s {
    TRIGGER_COMMON;
};

struct table_trigger_s {
    TRIGGER_COMMON;
};

struct row_trigger_s {
    TRIGGER_COMMON;
    mqi_handle_t table;
    select_t     select;
    uint8_t      data[0];
};

struct column_trigger_s {
    TRIGGER_COMMON;
    mqi_handle_t table;
    column_t     column;
    select_t     select;
    uint8_t      data[0];
};


static mdb_hash_t *callbacks;
static mdb_hash_t *triggers;

static int unref_callback(mql_callback_t *);
static mql_callback_t *ref_callback(mql_callback_t *);

static void column_event_callback(mqi_event_t *, void *);


int mql_register_callback(const char        *name,
                          mql_result_type_t  rtype,
                          mql_trigger_cb_t   function,
                          void              *user_data)
{
    mql_callback_t *cb;

    MDB_CHECKARG(name && *name && function &&
                 (rtype == mql_result_event  ||
                  rtype == mql_result_string ||
                  rtype == mql_result_dontcare), -1);

    if (!callbacks) {
        callbacks = MDB_HASH_TABLE_CREATE(string, MQL_CALLBACK_HASH_CHAINS);
        MDB_PREREQUISITE(callbacks, -1);
    }

    if (rtype == mql_result_dontcare)
        rtype = mql_result_event;

    if (!(cb = calloc(1, sizeof(mql_callback_t)))) {
        errno = ENOMEM;
        return -1;
    }

    cb->refcnt    = 0;
    cb->name      = strdup(name);
    cb->rtype     = rtype;
    cb->function  = function;
    cb->user_data = user_data;

    if (!cb->name || mdb_hash_add(callbacks, 0,cb->name, cb) < 0) {
        free(cb->name);
        free(cb);
        return -1;
    }

    return 0;
}


int mql_unregister_callback(const char *name)
{
    mql_callback_t *cb;

    MDB_CHECKARG(name, -1);
    
    if (!(cb = mdb_hash_delete(callbacks, 0,(void *)name)))
        return -1;

    return unref_callback(cb);
}


mql_callback_t *mql_find_callback(char *name)
{
    mql_callback_t *cb;

    MDB_CHECKARG(name, NULL);
    MDB_PREREQUISITE(callbacks, NULL);

    cb = mdb_hash_get_data(callbacks, 0,name);

    return cb;
}

int mql_create_column_trigger(char              *name,
                              mqi_handle_t       table,
                              int                colidx,
                              mqi_data_type_t    coltyp,
                              mql_callback_t    *callback,
                              int                nselcol,
                              char             **selcolnams,
                              mqi_column_desc_t *selcoldscs,
                              mqi_data_type_t   *selcoltypes,
                              int               *selcolsizes,
                              int                rowsize)
{
    column_trigger_t *tr;
    size_t nlens[MQI_COLUMN_MAX];
    size_t asiz;
    size_t nsiz;
    size_t dsiz;
    size_t tsiz;
    size_t ssiz;
    size_t size;
    uint8_t *data;
    int sts;
    int i;

    MDB_CHECKARG(name && table != MQI_HANDLE_INVALID && callback &&
                 (!nselcol || (nselcol > 0 && nselcol < MQI_COLUMN_MAX && 
                  selcoldscs && selcolsizes && rowsize > 0)), -1);

    if (!triggers) {
        triggers = MDB_HASH_TABLE_CREATE(string, MQL_TRIGGER_HASH_CHAINS);
        MDB_PREREQUISITE(triggers, -1);
    }

    if (!nselcol)
        size = sizeof(column_trigger_t);
    else {
        nsiz = asiz = sizeof(char *) * nselcol;

        for (i = 0;  i < nselcol;  i++)
            nsiz += (nlens[i] = strlen(selcolnams[i]) + 1);

        dsiz = sizeof(mqi_column_desc_t) * (nselcol ? nselcol + 1 : 0);
        tsiz = sizeof(mqi_data_type_t) * nselcol;
        ssiz = sizeof(int) * nselcol;
        size = sizeof(column_trigger_t) + nsiz + dsiz + tsiz + ssiz;
    }

    if (!(tr = calloc(1, size))) {
        errno = ENOMEM;
        return -1;
    }

    tr->name     = strdup(name);
    tr->type     = trigger_column;
    tr->callback = ref_callback(callback);

    tr->table = table;

    tr->column.index = colidx;
    tr->column.type  = coltyp; 

    if (nselcol > 0) {
        data = tr->data;

        tr->select.column.ncol  = nselcol;
        tr->select.column.names = (char **)data;
        tr->select.column.descs = (mqi_column_desc_t *)(data += asiz);
        tr->select.column.types = (mqi_data_type_t *)(data += dsiz);
        tr->select.column.sizes = (int *)(data += tsiz);

        tr->select.strpool.addr = (char *)(data += ssiz);
        tr->select.strpool.size = nsiz - asiz;

        tr->select.rowsize = rowsize; 

        memcpy(tr->select.column.descs, selcoldscs , dsiz);
        memcpy(tr->select.column.types, selcoltypes, tsiz);
        memcpy(tr->select.column.sizes, selcolsizes, ssiz);

        for (i = 0;   i < nselcol;   i++) {
            tr->select.column.names[i] = (char *)data;
            memcpy(data, selcolnams[i], nlens[i]);
            data += nlens[i];
        }
    }

    if (!tr->name || mdb_hash_add(triggers, 0,tr->name, tr) < 0) {
        free(tr->name);
        free(tr);
        return -1;
    }

    sts = mqi_create_column_trigger(table, colidx, column_event_callback, tr,
                                    tr->select.column.descs);
    if (sts < 0)
        return -1;

    return 0;
}
                              


static mql_callback_t *ref_callback(mql_callback_t *cb)
{
    cb->refcnt++;
    return cb;
}

static int unref_callback(mql_callback_t *cb)
{
    if (cb->refcnt > 0)
        cb->refcnt--;
    else {
        free(cb->name);
        free(cb);
    }

    return 0;
}


static void column_event_callback(mqi_event_t *evt, void *user_data)
{
    mqi_column_event_t *ce;
    column_trigger_t   *tr;
    mql_callback_t     *cb;
    select_t           *s;
    mql_result_t       *rsel;
    mql_result_t       *rslt;

    if (!evt || !user_data)
        return;

    ce = &evt->column;
    tr = (column_trigger_t *)user_data;
    cb = tr->callback;

    if (ce->event  != mqi_column_changed ||
        tr->type   != trigger_column ||
        (cb->rtype != mql_result_event &&
         cb->rtype != mql_result_string))
    {
        return;
    }


    if (tr->select.column.ncol <= 0) {
        rsel = NULL;

        if (cb->rtype == mql_result_event) {
            rslt = mql_result_event_column_change_create(ce->table.handle,
                                                         ce->column.index,
                                                         &ce->value,
                                                         NULL);
        }
        else {
            rslt = mql_result_string_create_column_change(ce->table.name,
                                                          ce->column.name,
                                                          &ce->value,
                                                          NULL);
        }
    }
    else {
        s = &tr->select;

        if (cb->rtype == mql_result_event) {

            rsel = mql_result_rows_create(s->column.ncol,
                                          s->column.descs,
                                          s->column.types,
                                          s->column.sizes,
                                          1,
                                          s->rowsize,
                                          ce->select.data);

            rslt = mql_result_event_column_change_create(ce->table.handle,
                                                         ce->column.index,
                                                         &ce->value,
                                                         rsel);
        }
        else {
            rsel = mql_result_string_create_row_list(s->column.ncol,
                                                     s->column.names,
                                                     s->column.descs,
                                                     s->column.types,
                                                     s->column.sizes,
                                                     1,
                                                     s->rowsize,
                                                     ce->select.data);

            rslt = mql_result_string_create_column_change(ce->table.name,
                                                          ce->column.name,
                                                          &ce->value,
                                                          rsel);
        }
    }
    
    if (!rslt)
        free(rsel);
    else {
        cb->function(rslt, cb->user_data);
        free(rslt);
    }
}



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
