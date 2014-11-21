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
#include <murphy-db/handle.h>
#include <murphy-db/sequence.h>
#include "table.h"
#include "row.h"
#include "table.h"
#include "cond.h"
#include "transaction.h"

#define TABLE_STATISTICS


typedef struct {
    int          indexed;
    void        *cursor;
} table_iterator_t;


static mdb_hash_t *table_hash;
static int         table_count;

static void destroy_table(mdb_table_t *);
static mdb_row_t *table_iterator(mdb_table_t *, table_iterator_t *);
#if 0
static int table_print_info(mdb_table_t *, char *, int);
#endif
static int select_conditional(mdb_table_t *, mqi_cond_entry_t *,
                              mqi_column_desc_t *,void *, int, int);
static int select_all(mdb_table_t *, mqi_column_desc_t  *, void *, int, int);
static int select_by_index(mdb_table_t*, int,void *, mqi_column_desc_t*,void*);
static int update_conditional(mdb_table_t *, mqi_cond_entry_t *,
                              mqi_column_desc_t *, void *, int);
static int update_all(mdb_table_t *, mqi_column_desc_t *, void *, int);
static int update_single_row(mdb_table_t *, mdb_row_t *, mqi_column_desc_t *,
                             void *, int);
static int delete_conditional(mdb_table_t *, mqi_cond_entry_t *);
static int delete_all(mdb_table_t *);
static int delete_single_row(mdb_table_t *, mdb_row_t *, int);


mdb_table_t *mdb_table_create(char *name,
                              char **index_columns,
                              mqi_column_def_t *cdefs)
{
    mdb_table_t      *tbl;
    mdb_hash_t       *chash;
    mqi_data_type_t   type;
    int               length;
    int               align;
    int               ncolumn;
    mdb_column_t     *columns;
    mdb_column_t     *col;
    mqi_column_def_t *cdef;
    int               dlgh;
    int               i;

    MDB_CHECKARG(name && cdefs, NULL);

    if (!table_hash && !(table_hash = MDB_HASH_TABLE_CREATE(varchar, 256))) {
        errno = EIO;
        return NULL;
    }

    for (ncolumn = 0;  cdefs[ncolumn].name;  ncolumn++) {
        cdef   = cdefs + ncolumn;
        type   = cdef->type;
        length = cdef->length;

        if (!cdef->name[0]) {
            ncolumn = 0;
            break;
        }

        if (type == mqi_varchar) {
            if (length < 1 || length > MDB_COLUMN_LENGTH_MAX) {
                ncolumn = 0;
                break;
            }
        }
        else if (type != mqi_integer &&
                 type != mqi_unsignd &&
                 type != mqi_floating )
        {
            ncolumn = 0;
            break;
        }
    }

    if (!ncolumn) {
        errno = EINVAL;
        return NULL;
    }


    length = sizeof(mdb_table_t) + sizeof(mdb_dlist_t) * ncolumn;

    if (!(tbl = calloc(1, length)) ||
        !(columns = calloc(ncolumn, sizeof(mdb_column_t))))
    {
        free(tbl);
        errno = ENOMEM;
        return NULL;
    }

    if (!(chash = MDB_HASH_TABLE_CREATE(varchar, 16))) {
        free(tbl);
        free(columns);
        return NULL;
    }

    for (i = 0, dlgh = 0;  i < ncolumn;  i++) {
        cdef = cdefs  + i;
        col  = columns + i;

        switch (cdef->type) {
        case mqi_varchar:  length = cdef->length + 1;  align = 1;    break;
        case mqi_integer:  length = sizeof(int32_t);   align = 4;    break;
        case mqi_unsignd:  length = sizeof(uint32_t);  align = 4;    break;
        case mqi_floating: length = sizeof(double);    align = 4;    break;
        default:           length = cdef->length;      align = 2;    break;
        }

        col->name   = strdup(cdef->name);
        col->type   = cdef->type;
        col->length = length;
        col->offset = (dlgh + (align - 1)) & ~(align - 1);

        dlgh = col->offset + col->length;

        mdb_hash_add(chash, 0,col->name, NULL + (i+1));
    }

    dlgh = (dlgh + 3) & ~3;

    tbl->handle    = MQI_HANDLE_INVALID;
    tbl->name      = strdup(name);
    tbl->cnt.stamp = 1;
    tbl->chash     = chash;
    tbl->ncolumn   = ncolumn;
    tbl->columns   = columns;
    tbl->dlgh      = dlgh;

    MDB_DLIST_INIT(tbl->rows);
    mdb_log_create(tbl);
    mdb_trigger_init(&tbl->trigger, ncolumn);

    if (mdb_hash_add(table_hash, 0,tbl->name, tbl) < 0) {
        destroy_table(tbl);
        return NULL;
    }

    if (index_columns)
        mdb_index_create(tbl, index_columns);

    table_count++;

    return tbl;
}

int mdb_table_register_handle(mdb_table_t *tbl, mqi_handle_t handle)
{
    MDB_CHECKARG(tbl && handle != MQI_HANDLE_INVALID, -1);
    MDB_PREREQUISITE(tbl->handle == MQI_HANDLE_INVALID, -1);

    tbl->handle = handle;

    mdb_trigger_table_create(tbl);

    return 0;
}

int mdb_table_drop(mdb_table_t *tbl)
{
    MDB_CHECKARG(tbl, -1);

    mdb_trigger_table_drop(tbl);
    mdb_trigger_reset(&tbl->trigger, tbl->ncolumn);

    mdb_transaction_drop_table(tbl);

    mdb_hash_delete(table_hash, 0,tbl->name);

    destroy_table(tbl);

    if (table_count > 1)
        table_count--;
    else {
        MDB_HASH_TABLE_DESTROY(table_hash);
        table_hash  = NULL;
        table_count = 0;
    }

    return 0;
}

int mdb_table_create_index(mdb_table_t *tbl, char **index_columns)
{
    mdb_row_t *row, *n;
    int        error = 0;

    MDB_CHECKARG(tbl && index_columns && index_columns[0], -1);

    if (MDB_TABLE_HAS_INDEX(tbl)) {
        errno = EEXIST;
        return -1;
    }

    if (mdb_index_create(tbl, index_columns) < 0)
        return -1;

    MDB_DLIST_FOR_EACH_SAFE(mdb_row_t, link, row,n, &tbl->rows) {
        if (mdb_index_insert(tbl, row, 0, 0) < 0) {
            if ((error = errno) != EEXIST)
                return -1;
        }
    }

    if (error) {
        errno = error;
        return -1;
    }

    return 0;
}


int mdb_table_describe(mdb_table_t *tbl, mqi_column_def_t *defs, int len)
{
    mdb_column_t *col;
    mqi_column_def_t *def;
    int i,n;

    MDB_CHECKARG(tbl && defs && len > 0 && len >= (n = tbl->ncolumn), -1);

    for (i = 0;    i < n;    i++) {
        col = tbl->columns + i;
        def = defs + i;

        def->name   = col->name;
        def->type   = col->type;
        def->length = col->length;
        def->flags  = col->flags;

        if (def->type == mqi_varchar && def->length > 0)
            def->length--;
    }

    return n;
}

int mdb_table_insert(mdb_table_t        *tbl,
                     int                 ignore,
                     mqi_column_desc_t  *cds,
                     void              **data)
{
    uint32_t   txdepth = mdb_transaction_get_depth();
    mdb_row_t    *row;
    int           error;
    int           nrow;
    int           ninsert;
    mqi_bitfld_t  cmask;
    int           i;

    MDB_CHECKARG(tbl && cds && data && data[0], -1);

    for (i = 0, error = 0, ninsert = 0;    data[i];    i++) {
        if (!(row = mdb_row_create(tbl))) {
            errno = ENOMEM;
            return -1;
        }

        mdb_row_update(tbl, row, cds, data[i], 0, &cmask);

        if ((nrow = mdb_index_insert(tbl, row, cmask, ignore)) < 0) {
            if ((error = errno) != EEXIST)
                return -1;

            ninsert = -1;
        }
        else if (nrow > 0) {
            tbl->nrow++;

            if (mdb_log_change(tbl,txdepth,mdb_log_insert,cmask,NULL,row) < 0)
                ninsert = -1;
            else
                ninsert += (ninsert >= 0) ? 1 : 0;
        }

    }

    if (error) {
        errno = error;
        return -1;
    }

    return ninsert;
}

int mdb_table_select(mdb_table_t       *tbl,
                     mqi_cond_entry_t  *cond,
                     mqi_column_desc_t *cds,
                     void              *results,
                     int                size,
                     int                dim)
{
    int  ndata;

    MDB_CHECKARG(tbl, -1);

    if (dim > MQI_QUERY_RESULT_MAX)
        dim = MQI_QUERY_RESULT_MAX;

    if (cond)
        ndata = select_conditional(tbl, cond, cds, results, size, dim);
    else
        ndata = select_all(tbl, cds, results, size, dim);

    return ndata;
}

int mdb_table_select_by_index(mdb_table_t *tbl,
                              mqi_variable_t *idxvars,
                              mqi_column_desc_t *cds,
                              void *result)
{
    mqi_variable_t    *var;
    mdb_index_t       *ix;
    mdb_column_t      *col;
    void              *data;
    mqi_column_desc_t  src;
    int                idxlen;
    char               idxval[MDB_INDEX_LENGTH_MAX];
    int                i;

    MDB_CHECKARG(tbl && idxvars && cds && result, -1);
    MDB_PREREQUISITE(MDB_TABLE_HAS_INDEX(tbl), -1);

    ix = &tbl->index;
    data = idxval - ix->offset;
    src.offset = 0;
    idxlen = ix->length;

    for (i = 0;   i < ix->ncolumn;   i++) {
        var = idxvars + i;
        col = tbl->columns + (src.cindex = ix->columns[i]);

        if (col->type != var->type) {
            errno = EINVAL;
            return -1;
        }

        mdb_column_write(col, data, &src, var->v.generic);
    }

    return select_by_index(tbl, idxlen,idxval, cds, result);
}

int mdb_table_update(mdb_table_t       *tbl,
                     mqi_cond_entry_t  *cond,
                     mqi_column_desc_t *cds,
                     void              *data)
{
    int           index_update = 0;
    mdb_column_t *col;
    int           cindex;
    int           nupdate;
    int           i;

    MDB_CHECKARG(tbl, -1);


    if (MDB_TABLE_HAS_INDEX(tbl)) {
        for (i = 0;   (cindex = cds[i].cindex) >= 0;    i++) {
            col = tbl->columns + cindex;
            if ((col->flags & MQI_COLUMN_KEY)) {
                index_update = 1;
                break;
            }
        }
    }

    if (cond)
        nupdate = update_conditional(tbl, cond, cds, data, index_update);
    else
        nupdate = update_all(tbl, cds, data, index_update);

    return nupdate;
}

int mdb_table_delete(mdb_table_t *tbl, mqi_cond_entry_t *cond)
{
    int ndelete;

    MDB_CHECKARG(tbl, -1);

    if (cond)
        ndelete = delete_conditional(tbl, cond);
    else
        ndelete = delete_all(tbl);

    return ndelete;
}

mdb_table_t *mdb_table_find(char *table_name)
{
    MDB_CHECKARG(table_name, NULL);
    MDB_PREREQUISITE(table_hash, NULL);

    return mdb_hash_get_data(table_hash, 0,table_name);
}


int mdb_table_get_column_index(mdb_table_t *tbl, char *column_name)
{
    MDB_CHECKARG(tbl && column_name, -1);

    return (mdb_hash_get_data(tbl->chash, 0,column_name) - NULL) - 1;
}

int mdb_table_get_size(mdb_table_t *tbl)
{
    MDB_CHECKARG(tbl, -1);

    return tbl->nrow;
}

char *mdb_table_get_column_name(mdb_table_t *tbl, int colidx)
{
    MDB_CHECKARG(tbl && colidx >= 0 && colidx < tbl->ncolumn, NULL);

    return tbl->columns[colidx].name;
}

mqi_data_type_t mdb_table_get_column_type(mdb_table_t *tbl, int colidx)
{
    MDB_CHECKARG(tbl && colidx >= 0 && colidx < tbl->ncolumn, mqi_error);

    return tbl->columns[colidx].type;
}

int mdb_table_get_column_size(mdb_table_t *tbl, int colidx)
{
    MDB_CHECKARG(tbl && colidx >= 0 && colidx < tbl->ncolumn, -1);

    return tbl->columns[colidx].length;
}

uint32_t mdb_table_get_stamp(mdb_table_t *tbl)
{
    return tbl->cnt.stamp;
}

int mdb_table_print_rows(mdb_table_t *tbl, char *buf, int len)
{
    mdb_row_t *row;
    char      *p, *e;
    int       l;
    int       i;
    char      dashes[1024];
    table_iterator_t it;

    MDB_CHECKARG(tbl && buf && len > 0, 0);

    e = (p = buf) + len;

    for (i = 0;  i < tbl->ncolumn;  i++)
        p += mdb_column_print_header(tbl->columns + i, p, e-p);

    if (p + ((l = p - buf) + 3) < e) {
        if (l > (int)sizeof(dashes) - 1)
            l = sizeof(dashes) - 1;

        memset(dashes, '-', l);
        dashes[l] = '\0';

        p += snprintf(p, e-p, "\n%s\n", dashes);

        for (it.cursor = NULL;  (row = table_iterator(tbl, &it)) && p < e;) {
            for (i = 0;  i < tbl->ncolumn && p < e;  i++)
                p += mdb_column_print(tbl->columns + i, row->data, p, e-p);
            if (p < e)
                p += snprintf(p, e-p, "\n");
        }
    }

    return p - buf;
}


static void destroy_table(mdb_table_t *tbl)
{
    mdb_row_t    *row, *n;
    mdb_column_t *cols;
    int           i;

    mdb_index_drop(tbl);

    mdb_hash_table_destroy(tbl->chash);

    MDB_DLIST_FOR_EACH_SAFE(mdb_row_t, link, row,n, &tbl->rows)
        mdb_row_delete(tbl, row, 0, 1);

    for (i = 0, cols = tbl->columns;   i < tbl->ncolumn;    i++)
        free(cols[i].name);

    free(tbl->columns);
    free(tbl->name);
    free(tbl);
}


static mdb_row_t *table_iterator(mdb_table_t *tbl, table_iterator_t *it)
{
    mdb_dlist_t *next;
    mdb_dlist_t *head;
    mdb_row_t   *row;

    if (!it->cursor)
        it->indexed = MDB_TABLE_HAS_INDEX(tbl);

    if (it->indexed)
        row = mdb_sequence_iterate(tbl->index.sequence, &it->cursor);
    else {
        head = &tbl->rows;
        next = it->cursor ? (mdb_dlist_t *)it->cursor : head->next;

        if (next == head)
            row = NULL;
        else {
            row = MDB_LIST_RELOCATE(mdb_row_t, link, next);
            it->cursor = next->next;
        }
    }

    return row;
}

#if 0
static int table_print_info(mdb_table_t *tbl, char *buf, int len)
{
#define PRINT(args...)  if (e > p) p += snprintf(p, e-p, args)

    mdb_column_t *col;
    char         *p, *e;
    int           i;

    MDB_CHECKARG(tbl && buf && len > 0, 0);

    e = (p = buf) + len;
    *buf = '\0';

    PRINT("table name  : '%s'\n", tbl->name);
    PRINT("table stamp : %u\n"  , tbl->stamp);
    PRINT("row length  : %d\n"  , tbl->dlgh);
    PRINT("no of column: %d\n"  , tbl->ncolumn);
    PRINT("    index name             type     offset length\n"
          "    ---------------------------------------------\n");

    for (i = 0;  i < tbl->ncolumn;  i++) {
        col = tbl->columns + i;

        PRINT("    %s %02d: %-16s %-8s   %4d   %4d\n",
              col->flags & MQI_COLUMN_KEY ? "*" : " ",
              i+1, col->name, mqi_data_type_str(col->type),
              col->offset, col->length);
    }

    p += mdb_index_print(tbl, p, e-p);

    return p - buf;

#undef PRINT
}
#endif


static int select_conditional(mdb_table_t       *tbl,
                              mqi_cond_entry_t  *cond,
                              mqi_column_desc_t *cds,
                              void              *results,
                              int                size,
                              int                dim)
{
    mdb_column_t      *columns = tbl->columns;
    mdb_row_t         *row;
    mqi_cond_entry_t  *ce;
    table_iterator_t   it;
    int                nresult;
    void              *result;
    mqi_column_desc_t *result_dsc;
    int                cindex;
    int                i;

    for (it.cursor = NULL, nresult = 0;  (row = table_iterator(tbl, &it)); ) {
        ce = cond;
        if (mdb_cond_evaluate(tbl, &ce, row->data)) {
            if (nresult >= dim) {
                errno = EOVERFLOW;
                return -1;
            }

            result = results + (size * nresult++);

            for (i = 0;  (cindex = (result_dsc = cds + i)->cindex) >= 0;   i++)
                mdb_column_read(result_dsc, result, columns+cindex, row->data);
        }
    }

    return nresult;
}

static int select_all(mdb_table_t       *tbl,
                      mqi_column_desc_t *cds,
                      void              *results,
                      int                size,
                      int                dim)
{
    mdb_column_t      *columns = tbl->columns;
    mdb_row_t         *row;
    table_iterator_t   it;
    int                nresult;
    void              *result;
    mqi_column_desc_t *result_dsc;
    int                cindex;
    int                j;

    MQI_UNUSED(dim);

    for (it.cursor = NULL, nresult = 0;
         (row = table_iterator(tbl, &it));
         nresult++)
    {
        result = results + (size * nresult);

        for (j = 0;   (cindex = (result_dsc = cds + j)->cindex) >= 0;    j++)
            mdb_column_read(result_dsc, result, columns + cindex, row->data);
    }

    return nresult;
}

static int select_by_index(mdb_table_t       *tbl,
                           int                idxlen,
                           void              *idxval,
                           mqi_column_desc_t *cds,
                           void              *result)
{
    mdb_column_t      *columns = tbl->columns;
    mdb_row_t         *row;
    mqi_column_desc_t *result_dsc;
    int                cindex;
    int                j;

    if (!(row = mdb_index_get_row(tbl, idxlen,idxval)))
        return 0;

    for (j = 0;   (cindex = (result_dsc = cds + j)->cindex) >= 0;    j++)
            mdb_column_read(result_dsc, result, columns + cindex, row->data);

    return 1;
}


static int update_conditional(mdb_table_t       *tbl,
                              mqi_cond_entry_t  *cond,
                              mqi_column_desc_t *cds,
                              void              *data,
                              int                index_update)
{
    mdb_row_t        *row;
    mqi_cond_entry_t *ce;
    table_iterator_t  it;
    int               nupdate, changed;

    for (it.cursor = NULL, nupdate = 0;  (row = table_iterator(tbl, &it)); ) {
        ce = cond;
        if (mdb_cond_evaluate(tbl, &ce, row->data)) {
            changed = update_single_row(tbl, row, cds, data, index_update);

            if (changed < 0)
                nupdate = -1;
            else
                nupdate += (nupdate >= 0) ? changed : 0;
        }
    }

    return nupdate;
}

static int update_all(mdb_table_t       *tbl,
                      mqi_column_desc_t *cds,
                      void              *data,
                      int                index_update)
{
    mdb_row_t        *row;
    table_iterator_t  it;
    int               nupdate, changed;

    for (it.cursor = NULL, nupdate = 0; (row = table_iterator(tbl, &it)); )
    {
        changed = update_single_row(tbl, row, cds, data, index_update);

        if (changed < 0)
            nupdate = -1;
        else
            nupdate += (nupdate >= 0) ? changed : 0;
    }

    if (nupdate < 0)
        errno = EEXIST;

    return nupdate;
}

static int update_single_row(mdb_table_t       *tbl,
                             mdb_row_t         *row,
                             mqi_column_desc_t *cds,
                             void              *data,
                             int                index_update)
{
    mdb_row_t   *before  = NULL;
    uint32_t     txdepth = mdb_transaction_get_depth();
    mqi_bitfld_t cmask;
    int          changed;

    if (txdepth > 0 && !(before = mdb_row_duplicate(tbl, row)))
        return -1;

    changed = mdb_row_update(tbl, row, cds, data, index_update, &cmask);

    if (changed <= 0) {
        mdb_row_delete(tbl, before, 0, 1);
        return changed;
    }

    if (mdb_log_change(tbl, txdepth, mdb_log_update, cmask, before, row) < 0)
        return -1;

    return 1;
}

static int delete_conditional(mdb_table_t *tbl, mqi_cond_entry_t *cond)
{
    table_iterator_t  it;
    mdb_row_t        *row;
    mqi_cond_entry_t *ce;
    int               ndelete;

    for (it.cursor = NULL, ndelete = 0; (row = table_iterator(tbl, &it)); )
    {
        ce = cond;
        if (mdb_cond_evaluate(tbl, &ce, row->data)) {
            if (delete_single_row(tbl, row, 1) < 0)
                ndelete = -1;
            else
                ndelete += (ndelete >= 0) ? 1 : 0;
        }
    }

    return ndelete;
}


static int delete_all(mdb_table_t *tbl)
{
    mdb_row_t *row, *n;
    int        ndelete = 0;

    mdb_index_reset(tbl);

    MDB_DLIST_FOR_EACH_SAFE(mdb_row_t, link, row,n, &tbl->rows) {
        if (delete_single_row(tbl, row, 0) < 0)
            ndelete = -1;
        else
            ndelete += (ndelete >= 0) ? 1 : 0;
    }

    return ndelete;
}

static int delete_single_row(mdb_table_t *tbl, mdb_row_t *row,int index_update)
{
    uint32_t txdepth = mdb_transaction_get_depth();

    mdb_row_delete(tbl, row, index_update, !txdepth);

    if (txdepth)
        mdb_log_change(tbl, txdepth, mdb_log_delete, 0, row, NULL);

    return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
