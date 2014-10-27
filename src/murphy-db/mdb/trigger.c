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
#include <limits.h>
#include <stdio.h>
#include <alloca.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include "table.h"
#include "row.h"

#ifndef LOG_TRIGGER
#define LOG_TRIGGER
#endif

typedef struct callback_s         callback_t;
typedef struct select_s           select_t;

typedef struct column_trigger_s   column_trigger_t;
typedef struct row_trigger_s      row_trigger_t;
typedef struct table_trigger_s    table_trigger_t;
typedef struct transact_trigger_s transact_trigger_t;

struct callback_s {
    mqi_trigger_cb_t  function;
    void             *user_data;
};

struct select_s {
    int               length;
    size_t            cdsiz;
    mqi_column_desc_t column[0];
};

struct column_trigger_s {
    mdb_dlist_t link;
    callback_t  callback;
    select_t    select;
};

struct row_trigger_s {
    mdb_dlist_t link;
    callback_t  callback;
    select_t    select;
};

struct table_trigger_s {
    mdb_dlist_t  link;
    callback_t   callback;
};

struct transact_trigger_s {
    mdb_dlist_t  link;
    callback_t   callback;
};


static int8_t lowest_bit_in[256] = {
    /*         0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
    /*        -------------------------------------------------------------- */
    /* 00 */  -1,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 10 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 20 */   5,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 30 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 40 */   6,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 50 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 60 */   5,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 70 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 80 */   7,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* 90 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* A0 */   5,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* B0 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* C0 */   6,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* D0 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* E0 */   5,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
    /* F0 */   4,  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,
};


static MDB_DLIST_HEAD(table_change_triggers);
static MDB_DLIST_HEAD(transact_change_triggers);

static int get_select_params(mdb_table_t *, mqi_column_desc_t *, int *, int *);
static void row_change(mqi_event_type_t, mdb_table_t *, mdb_row_t *);
static void table_change(mqi_event_type_t, mdb_table_t *);
static void transaction_change(mqi_event_type_t, uint32_t);


void mdb_trigger_init(mdb_trigger_t *trigger, int ncol)
{
    int i;

    if (!trigger || ncol < 1)
        return;

    MDB_DLIST_INIT(trigger->row_change);

    for (i = 0;  i < ncol;  i++)
        MDB_DLIST_INIT(trigger->column_change[i]);
}

void mdb_trigger_reset(mdb_trigger_t *trigger, int ncol)
{
    row_trigger_t *rt, *n;
    column_trigger_t *ct, *m;
    mdb_dlist_t *head;
    int i;

    if (!trigger || ncol < 1)
        return;

    MDB_DLIST_FOR_EACH_SAFE(row_trigger_t, link, rt,n, &trigger->row_change) {
        MDB_DLIST_UNLINK(row_trigger_t, link, rt);
        free(rt);
    }

    for (i = 0;  i < ncol;  i++) {
        head = trigger-> column_change + i;

        MDB_DLIST_FOR_EACH_SAFE(column_trigger_t, link, ct,m, head) {
            MDB_DLIST_UNLINK(column_trigger_t, link, ct);
            free(ct);
        }
    }
}


int mdb_trigger_add_column_callback(mdb_table_t       *tbl,
                                    int                cidx,
                                    mqi_trigger_cb_t   cb_function,
                                    void              *cb_data,
                                    mqi_column_desc_t *cds)
{
    column_trigger_t *tr;
    size_t cdsiz;
    int length, ncd;
    mdb_dlist_t *head;

    MDB_CHECKARG(tbl && cidx >= 0 && cidx < tbl->ncolumn && cb_function, -1);

    if (!cds)
        ncd = length = 0;
    else {
        if (get_select_params(tbl, cds, &ncd, &length) < 0) {
            errno = EINVAL;
            return -1;
        }
    }

    cdsiz = sizeof(mqi_column_desc_t) * ncd;
    head  = tbl->trigger.column_change + cidx;

    MDB_DLIST_FOR_EACH(column_trigger_t, link, tr, head) {
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            if (cdsiz == tr->select.cdsiz) {
                if (!cdsiz || memcmp(cds, tr->select.column, cdsiz))
                    return 0; /* silently ignore multiple registrations */
            }

            errno = EEXIST;
            return -1;
        }
    }

    if (!(tr = calloc(1, sizeof(column_trigger_t) + cdsiz))) {
        errno = ENOMEM;
        return -1;
    }

    MDB_DLIST_APPEND(column_trigger_t, link, tr, head);

    tr->callback.function = cb_function;
    tr->callback.user_data = cb_data;

    tr->select.length = length;
    tr->select.cdsiz = cdsiz;

    if (ncd > 0)
        memcpy(tr->select.column, cds, cdsiz);

    return 0;
}

int mdb_trigger_delete_column_callback(mdb_table_t      *tbl,
                                       int               cidx,
                                       mqi_trigger_cb_t  cb_function,
                                       void             *cb_data)
{
    column_trigger_t *tr, *n;
    mdb_dlist_t *head;

    MDB_CHECKARG(tbl && cidx >= 0 && cidx < tbl->ncolumn && cb_function, -1);

    head = tbl->trigger.column_change + cidx;

    MDB_DLIST_FOR_EACH_SAFE(column_trigger_t, link, tr,n, head) {
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            MDB_DLIST_UNLINK(column_trigger_t, link, tr);
            free(tr);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}

int mdb_trigger_add_row_callback(mdb_table_t       *tbl,
                                 mqi_trigger_cb_t   cb_function,
                                 void              *cb_data,
                                 mqi_column_desc_t *cds)
{
    row_trigger_t *tr;
    size_t cdsiz;
    int length, ncd;
    mdb_dlist_t *head;

    MDB_CHECKARG(tbl && cb_function, -1);

    if (!cds)
        ncd = length = 0;
    else {
        if (get_select_params(tbl, cds, &ncd, &length) < 0) {
            errno = EINVAL;
            return -1;
        }
    }

    cdsiz = sizeof(mqi_column_desc_t) * ncd;
    head  = &tbl->trigger.row_change;

    MDB_DLIST_FOR_EACH(row_trigger_t, link, tr, head) {
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            if (cdsiz == tr->select.cdsiz) {
                if (!cdsiz || memcmp(cds, tr->select.column, cdsiz))
                    return 0; /* silently ignore multiple registrations */
            }

            errno = EEXIST;
            return -1;
        }
    }

    if (!(tr = calloc(1, sizeof(row_trigger_t) + cdsiz))) {
        errno = ENOMEM;
        return -1;
    }

    MDB_DLIST_APPEND(row_trigger_t, link, tr, head);

    tr->callback.function = cb_function;
    tr->callback.user_data = cb_data;

    tr->select.length = length;
    tr->select.cdsiz = cdsiz;

    if (ncd > 0)
        memcpy(tr->select.column  , cds, cdsiz);

    return 0;
}


int mdb_trigger_delete_row_callback(mdb_table_t      *tbl,
                                    mqi_trigger_cb_t  cb_function,
                                    void             *cb_data)
{
    row_trigger_t *tr, *n;

    MDB_CHECKARG(tbl && cb_function, -1);

    MDB_DLIST_FOR_EACH_SAFE(row_trigger_t,link, tr,n,&tbl->trigger.row_change){
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            MDB_DLIST_UNLINK(row_trigger_t, link, tr);
            free(tr);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}


int mdb_trigger_add_table_callback(mqi_trigger_cb_t  cb_function,
                                   void             *cb_data)
{
    table_trigger_t *tr;

    MDB_CHECKARG(cb_function, -1);

    MDB_DLIST_FOR_EACH(table_trigger_t, link, tr, &table_change_triggers) {
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            return 0; /* silently ignore multiple registrations */
        }
    }

    if (!(tr = calloc(1, sizeof(table_trigger_t)))) {
        errno = ENOMEM;
        return -1;
    }

    MDB_DLIST_APPEND(table_trigger_t, link, tr, &table_change_triggers);

    tr->callback.function = cb_function;
    tr->callback.user_data = cb_data;

    return 0;
}


int mdb_trigger_delete_table_callback(mqi_trigger_cb_t  cb_function,
                                      void             *cb_data)
{
    table_trigger_t *tr, *n;

    MDB_CHECKARG(cb_function, -1);

    MDB_DLIST_FOR_EACH_SAFE(table_trigger_t,link, tr,n,&table_change_triggers){
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            MDB_DLIST_UNLINK(table_trigger_t,link, tr);
            free(tr);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}

int mdb_trigger_add_transaction_callback(mqi_trigger_cb_t  cb_function,
                                         void             *cb_data)
{
    transact_trigger_t *tr;

    MDB_CHECKARG(cb_function, -1);

    MDB_DLIST_FOR_EACH(transact_trigger_t,link, tr, &transact_change_triggers){
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            return 0; /* silently ignore multiple registrations */
        }
    }

    if (!(tr = calloc(1, sizeof(transact_trigger_t)))) {
        errno = ENOMEM;
        return -1;
    }

    MDB_DLIST_APPEND(transact_trigger_t, link, tr, &transact_change_triggers);

    tr->callback.function = cb_function;
    tr->callback.user_data = cb_data;

    return 0;
}

int mdb_trigger_delete_transaction_callback(mqi_trigger_cb_t  cb_function,
                                            void             *cb_data)
{
    mdb_dlist_t *head = &transact_change_triggers;
    transact_trigger_t *tr, *n;

    MDB_CHECKARG(cb_function, -1);

    MDB_DLIST_FOR_EACH_SAFE(transact_trigger_t, link, tr,n, head) {
        if (cb_function == tr->callback.function &&
            cb_data == tr->callback.user_data)
        {
            MDB_DLIST_UNLINK(transact_trigger_t, link, tr);
            free(tr);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}

void mdb_trigger_column_change(mdb_table_t  *tbl,
                               mqi_bitfld_t  colmask,
                               mdb_row_t    *before,
                               mdb_row_t    *after)
{
    mqi_event_t         evt;
    mdb_dlist_t        *hd;
    column_trigger_t   *tr;
    mdb_column_t       *col;
    mqi_column_desc_t   cd;
    mqi_column_event_t *ce;
    int                 cx;
    int                 sx;
    mqi_bitfld_t        mask, byte;
    int                 i,j,k;

    if (!tbl || !colmask || !before || !after)
        return;

    memset(&evt, 0, sizeof(evt));
    ce = &evt.column;

    ce->event = mqi_column_changed;

    ce->table.handle = tbl->handle;
    ce->table.name   = tbl->name;

    ce->select.data = alloca(MDB_COLUMN_LENGTH_MAX * tbl->ncolumn);

    if (!ce->select.data)
        return;

    for (mask = colmask, i = 0;     mask != 0;     mask >>= 8, i += 8) {
        byte = mask & 0xff;

        while ((j = lowest_bit_in[byte]) >= 0) {
            byte &= ~MQI_BIT(j);
            cx  = i + j;
            col = tbl->columns + cx;
            hd  = tbl->trigger.column_change + cx;

            MDB_DLIST_FOR_EACH(column_trigger_t, link, tr, hd) {
                ce->column.index = cx;
                ce->column.name  = tbl->columns[cx].name;

                ce->value.type = tbl->columns[cx].type;

                cd.cindex = cx;
                cd.offset = 0;

                mdb_column_read(&cd, &ce->value.old, col, before->data);
                mdb_column_read(&cd, &ce->value.new_, col, after->data );

                if (tr->select.length > 0) {
                    for (k = 0; (sx = tr->select.column[k].cindex) >= 0;  k++){
                        mdb_column_read(tr->select.column + k, ce->select.data,
                                        tbl->columns + sx, after->data);
                    }
                }

                tr->callback.function(&evt, tr->callback.user_data);
            }
        }
    }
}


void mdb_trigger_row_insert(mdb_table_t *tbl, mdb_row_t *row)
{
    if (tbl && row)
        row_change(mqi_row_inserted, tbl, row);
}

void mdb_trigger_row_delete(mdb_table_t *tbl, mdb_row_t *row)
{
    if (tbl && row)
        row_change(mqi_row_deleted, tbl, row);
}


void mdb_trigger_table_create(mdb_table_t *tbl)
{
    if (tbl)
        table_change(mqi_table_created, tbl);
}

void mdb_trigger_table_drop(mdb_table_t *tbl)
{
    if (tbl)
        table_change(mqi_table_dropped, tbl);
}

void mdb_trigger_transaction_start(uint32_t depth)
{
    transaction_change(mqi_transaction_start, depth);
}

void mdb_trigger_transaction_end(uint32_t depth)
{
    transaction_change(mqi_transaction_end, depth);
}

static int get_select_params(mdb_table_t       *tbl,
                             mqi_column_desc_t *cds,
                             int               *ncd_ret,
                             int               *length_ret)
{
    mqi_column_desc_t *cd;
    int ncd, length;
    int end;
    int cx;

    *ncd_ret = *length_ret = 0;

    for (ncd = length = 0;    (cx = (cd = cds + ncd)->cindex) >= 0;    ncd++) {
        if ((end = cd->offset + tbl->columns[cx].length) > length)
            length = end;
    }

    *ncd_ret = ncd + 1;
    *length_ret = length;

    return 0;
}


static void row_change(mqi_event_type_t  event,
                       mdb_table_t      *tbl,
                       mdb_row_t        *row)
{
    mqi_event_t      evt;
    row_trigger_t   *tr;
    mqi_row_event_t *re;
    int              sx;
    int              i;

    memset(&evt, 0, sizeof(evt));
    re = &evt.row;

    re->event = event;

    re->table.handle = tbl->handle;
    re->table.name   = tbl->name;

    re->select.data = alloca(MDB_COLUMN_LENGTH_MAX * tbl->ncolumn);

    if (!re->select.data)
        return;

    MDB_DLIST_FOR_EACH(row_trigger_t, link, tr, &tbl->trigger.row_change) {
        if (tr->select.length > 0) {
            for (i = 0;  (sx = tr->select.column[i].cindex) >= 0;   i++)  {
                mdb_column_read(tr->select.column + i, re->select.data,
                                tbl->columns + sx, row->data);
            }
        }

        tr->callback.function(&evt, tr->callback.user_data);
    }
}

static void table_change(mqi_event_type_t event, mdb_table_t *tbl)
{
    mqi_event_t        evt;
    table_trigger_t   *tr;
    mqi_table_event_t *te;

    memset(&evt, 0, sizeof(evt));
    te = &evt.table;

    te->event = event;

    te->table.handle = tbl->handle;
    te->table.name   = tbl->name;

    MDB_DLIST_FOR_EACH(table_trigger_t, link, tr, &table_change_triggers) {
        tr->callback.function(&evt, tr->callback.user_data);
    }
}

static void transaction_change(mqi_event_type_t event, uint32_t depth)
{
    mqi_event_t           evt;
    transact_trigger_t   *tr;
    mqi_transact_event_t *te;

    memset(&evt, 0, sizeof(evt));
    te = &evt.transact;

    te->event = event;
    te->depth = depth;

    MDB_DLIST_FOR_EACH(transact_trigger_t,link, tr, &transact_change_triggers){
        tr->callback.function(&evt, tr->callback.user_data);
    }
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
