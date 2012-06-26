#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include "transaction.h"
#include "log.h"
#include "index.h"
#include "table.h"

#define TRANSACTION_STATISTICS


static uint32_t txdepth;

static int destroy_row(mdb_table_t *, mdb_row_t *);
static int remove_row(mdb_table_t *, mdb_row_t *);
static int add_row(mdb_table_t *, mdb_row_t *);
static int copy_row(mdb_table_t *, mdb_row_t *, mdb_row_t *);


uint32_t mdb_transaction_begin(void)
{
    return ++txdepth;
}

int mdb_transaction_commit(uint32_t depth)
{
#define DATA_MAX  (MQI_COLUMN_MAX * MQI_QUERY_RESULT_MAX)

    static uint8_t    blank[sizeof(mdb_row_t) + DATA_MAX];

    mdb_log_entry_t  *en;
    mdb_row_t        *before;
    mdb_row_t        *after;
    void             *cursor;
    bool              start_triggered = false;
    int               sts = 0, s;

    MDB_CHECKARG(depth > 0 && depth == txdepth, -1);

    MDB_TRANSACTION_LOG_FOR_EACH_DELETE(depth, en, MDB_BACKWARD, cursor) {

        if (!start_triggered) {
            start_triggered = true;
            mdb_trigger_transaction_start();
        }

        if (!(before = en->before))
            before = (mdb_row_t *)blank;

        if (!(after = en->after))
            after = (mdb_row_t *)blank;

        switch (en->change) {

        case mdb_log_insert:
            mdb_trigger_row_insert(en->table, after);
            mdb_trigger_column_change(en->table, en->colmask, before, after);
            s = 0;
            break;

        case mdb_log_update:
            mdb_trigger_column_change(en->table, en->colmask, before, after);
            s = destroy_row(en->table, en->before);
            break;

        case mdb_log_delete:
            mdb_trigger_row_delete(en->table, before);
            s = destroy_row(en->table, en->before);
            break;

        default:
            s = -1;
            break;
        }

        if (sts == 0)
            sts = s;
    }

    if (start_triggered)
        mdb_trigger_transaction_end();

    return sts;

#undef DATA_MAX
}

int mdb_transaction_rollback(uint32_t depth)
{
    mdb_log_entry_t  *en;
    mdb_table_t      *tbl;
    void             *cursor;
    int               sts = 0, s;

    MDB_CHECKARG(depth > 0 && depth == txdepth, -1);

    MDB_TRANSACTION_LOG_FOR_EACH_DELETE(depth, en, MDB_FORWARD, cursor) {

        tbl = en->table;

        switch (en->change) {

        case mdb_log_insert:  s = remove_row(tbl, en->after);            break;
        case mdb_log_delete:  s = add_row(tbl, en->before);              break;
        case mdb_log_update:  s = copy_row(tbl, en->after, en->before);  break;
        default:              s = -1;                                    break;
        }

        if (sts == 0)
            sts = s;
    }

    return sts;
}

int mdb_transaction_drop_table(mdb_table_t *tbl)
{
    mdb_log_entry_t *en;
    void            *cursor;
    int              sts = 0, s;

    MDB_CHECKARG(tbl, -1);

    MDB_TABLE_LOG_FOR_EACH_DELETE(tbl, en, cursor) {

        switch (en->change) {

        case mdb_log_insert:  s = 0;                                     break;
        case mdb_log_delete:
        case mdb_log_update:  s = destroy_row(en->table, en->before);    break;
        default:              s = -1;                                    break;
        }

        if (sts == 0)
            sts = s;
    }

    return sts;
}

uint32_t mdb_transaction_get_depth(void)
{
    return txdepth;
}

static int destroy_row(mdb_table_t *tbl, mdb_row_t *row)
{
    MDB_CHECKARG(tbl && row && MDB_DLIST_EMPTY(row->link), -1);

    return mdb_row_delete(tbl, row, 0, 1);
}

static int remove_row(mdb_table_t *tbl, mdb_row_t *row)
{
    MDB_CHECKARG(tbl && row, -1);

    if (mdb_index_delete(tbl, row) < 0 ||
        mdb_row_delete(tbl, row, 0, 1) < 0    )
    {
        return -1;
    }

    return 0;
}

static int add_row(mdb_table_t *tbl, mdb_row_t *row)
{
    MDB_CHECKARG(tbl && row, -1);

    MDB_DLIST_APPEND(mdb_row_t, link, row, &tbl->rows);

    return mdb_index_insert(tbl, row, 0, 0);
}

static int copy_row(mdb_table_t *tbl, mdb_row_t *dst, mdb_row_t *src)
{

    MDB_CHECKARG(tbl && dst && src && MDB_DLIST_EMPTY(src->link), -1);

    if (src == dst)
        return 0;

    if (mdb_row_copy_over(tbl,dst,src) < 0 || mdb_row_delete(tbl,src,0,1) < 0)
        return -1;

    return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
