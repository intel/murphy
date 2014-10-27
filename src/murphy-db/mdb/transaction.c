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
static int check_stamp(mdb_log_entry_t *);


uint32_t mdb_transaction_begin(void)
{
    return ++txdepth;
}

int mdb_transaction_commit(uint32_t depth)
{
#define DATA_MAX  (MQI_COLUMN_MAX * MQI_QUERY_RESULT_MAX)
#define CHECK_TRIGGER_START(en) do {                    \
        if (!start_triggered) {                         \
            start_triggered = true;                     \
            mdb_trigger_transaction_start(depth);       \
        }                                               \
    } while (0)
#define CHECK_TRIGGER_END() do {                        \
        if (start_triggered) {                          \
            mdb_trigger_transaction_end(depth);         \
        }                                               \
    } while (0)


    static uint8_t    blank[sizeof(mdb_row_t) + DATA_MAX];

    mdb_log_entry_t  *en;
    mdb_row_t        *before;
    mdb_row_t        *after;
    void             *cursor;
    bool              start_triggered = false;
    int               sts = 0, s;

    MDB_CHECKARG(depth > 0 && depth == txdepth, -1);

    MDB_TRANSACTION_LOG_FOR_EACH_DELETE(depth, en, MDB_BACKWARD, cursor) {

        if (!(before = en->before))
            before = (mdb_row_t *)blank;

        if (!(after = en->after))
            after = (mdb_row_t *)blank;

        switch (en->change) {

        case mdb_log_insert:
            CHECK_TRIGGER_START(en);
            mdb_trigger_row_insert(en->table, after);
            mdb_trigger_column_change(en->table, en->colmask, before, after);
            s = 0;
            break;

        case mdb_log_update:
            CHECK_TRIGGER_START(en);
            mdb_trigger_column_change(en->table, en->colmask, before, after);
            s = destroy_row(en->table, en->before);
            break;

        case mdb_log_delete:
            CHECK_TRIGGER_START(en);
            mdb_trigger_row_delete(en->table, before);
            s = destroy_row(en->table, en->before);
            break;

        case mdb_log_start:
            check_stamp(en);
            free(en->cnt);
            s = 0;
            break;

        default:
            s = -1;
            break;
        }

        if (sts == 0)
            sts = s;
    }

    txdepth--;

    CHECK_TRIGGER_END();

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
        case mdb_log_start:   s = check_stamp(en);                       break;
        default:              s = -1;                                    break;
        }

        if (sts == 0)
            sts = s;
    }

    txdepth--;

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
        case mdb_log_start:   s = 0;                                     break;
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

    tbl->cnt.inserts--;

    return 0;
}

static int add_row(mdb_table_t *tbl, mdb_row_t *row)
{
    MDB_CHECKARG(tbl && row, -1);

    MDB_DLIST_APPEND(mdb_row_t, link, row, &tbl->rows);

    tbl->cnt.deletes--;

    return mdb_index_insert(tbl, row, 0, 0);
}

static int copy_row(mdb_table_t *tbl, mdb_row_t *dst, mdb_row_t *src)
{

    MDB_CHECKARG(tbl && dst && src && MDB_DLIST_EMPTY(src->link), -1);

    if (src == dst)
        return 0;

    if (mdb_row_copy_over(tbl,dst,src) < 0 || mdb_row_delete(tbl,src,0,1) < 0)
        return -1;

    tbl->cnt.updates--;

    return 0;
}


static int check_stamp(mdb_log_entry_t *en)
{
    mdb_table_t *tbl;

    if (en->change != mdb_log_start)
        return -1;

    tbl = en->table;

    if (tbl->cnt.inserts == en->cnt->inserts &&
        tbl->cnt.deletes == en->cnt->deletes &&
        tbl->cnt.updates == en->cnt->updates)
        tbl->cnt.stamp = en->cnt->stamp;

    return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
