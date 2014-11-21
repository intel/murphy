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
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include "index.h"
#include "row.h"
#include "column.h"
#include "table.h"

#include "transaction.h"

#define INDEX_HASH_CREATE(t)        MDB_HASH_TABLE_CREATE(t,100)
#define INDEX_SEQUENCE_CREATE(t)    MDB_SEQUENCE_TABLE_CREATE(t,16)

#define INDEX_HASH_DROP(ix)         mdb_hash_table_destroy(ix->hash)
#define INDEX_SEQUENCE_DROP(ix)     mdb_sequence_table_destroy(ix->sequence)

#define INDEX_HASH_RESET(ix)        mdb_hash_table_reset(ix->hash)
#define INDEX_SEQUENCE_RESET(ix)    mdb_sequence_table_reset(ix->sequence)



int mdb_index_create(mdb_table_t *tbl, char **index_columns)
{
    mdb_index_t     *ix;
    mdb_column_t    *col;
    mqi_data_type_t  type;
    int              beg, end;
    int             *idxcols;
    int              i,j, idx;

    MDB_CHECKARG(tbl && index_columns && index_columns[0], -1);

    ix = &tbl->index;

    beg = end = 0;
    type = mqi_unknown;
    idxcols = NULL;

    for (i = 0;    index_columns[i];    i++) {
        if (!(idx = mdb_hash_get_data(tbl->chash,0,index_columns[i]) - NULL)) {
            errno = ENOENT;
            return -1;
        }

        col = tbl->columns + --idx;
        col->flags |= MQI_COLUMN_KEY;

        if (i == 0) {
            type = col->type;
            beg  = col->offset;
            end  = beg + col->length;
        }
        else {
            type = mqi_blob;

            if (col->offset == end)
                end += col->length;
            else if (col->offset == beg - col->length)
                beg = col->offset;
            else {
                type = mqi_unknown;
                break; /* not an adjacent column */
            }
        }

        if (!(idxcols = realloc(idxcols, sizeof(int) * (i+1)))) {
            errno = ENOMEM;
            return -1;
        }

        for (j = 0;  j < i;  j++) {
            if (idx == idxcols[j])
                break;

            if (idx < idxcols[j]) {
                memmove(idxcols + j+1, idxcols + j, sizeof(*idxcols) * (i-j));
                break;
            }
        }

        idxcols[j] = idx;
    }

    if (type == mqi_unknown || beg < 0 || end <= beg ||
        end - beg > MDB_INDEX_LENGTH_MAX)
    {
        free(idxcols);
        errno = EIO;
        return -1;
    }

    ix->type    = type;
    ix->length  = end - beg;
    ix->offset  = beg;
    ix->ncolumn = i;
    ix->columns = idxcols;

    switch (type) {
    case mqi_varchar:
        ix->hash = INDEX_HASH_CREATE(varchar);
        ix->sequence = INDEX_SEQUENCE_CREATE(varchar);
        break;
    case mqi_integer:
        ix->hash = INDEX_HASH_CREATE(integer);
        ix->sequence = INDEX_SEQUENCE_CREATE(integer);
        break;
    case mqi_unsignd:
        ix->hash = INDEX_HASH_CREATE(unsignd);
        ix->sequence = INDEX_SEQUENCE_CREATE(unsignd);
        break;
    case mqi_blob:
        ix->hash = INDEX_HASH_CREATE(blob);
        ix->sequence = INDEX_SEQUENCE_CREATE(blob);
        break;
    default:
        free(idxcols);
        memset(ix, 0, sizeof(*ix));
        break;
    }

    return 0;
}

void mdb_index_drop(mdb_table_t *tbl)
{
    mdb_index_t *ix;

    MDB_CHECKARG(tbl,);

    ix = &tbl->index;

    if (MDB_INDEX_DEFINED(ix)) {
        INDEX_HASH_DROP(ix);
        INDEX_SEQUENCE_DROP(ix);

        free(ix->columns);

        memset(ix, 0, sizeof(*ix));

        ix->type = mqi_unknown;
    }
}

void mdb_index_reset(mdb_table_t *tbl)
{
    mdb_index_t *ix;

    MDB_CHECKARG(tbl,);

    ix = &tbl->index;

    if (MDB_INDEX_DEFINED(ix)) {
        INDEX_HASH_RESET(ix);
        INDEX_SEQUENCE_RESET(ix);
    }
}


int mdb_index_insert(mdb_table_t   *tbl,
                     mdb_row_t     *row,
                     mqi_bitfld_t   cmask,
                     int            ignore)
{
    mdb_index_t    *ix;
    int             lgh;
    void           *key;
    mdb_hash_t     *hash;
    mdb_sequence_t *seq;
    mdb_row_t      *old;
    uint32_t        txdepth;

    MDB_CHECKARG(tbl && row, -1);

    ix = &tbl->index;

    if (!MDB_INDEX_DEFINED(ix))
        return 1;               /* fake a sucessful insertion */

    hash = ix->hash;
    seq  = ix->sequence;
    lgh  = ix->length;
    key  = (void *)row->data + ix->offset;

    if (mdb_hash_add(hash, lgh,key, row) == 0) {
        mdb_sequence_add(seq, lgh,key, row);
        return 1;
    }

    /*
     * we have a duplicate at hand
     */

    if (ignore) { /* replace the duplicate with the new row */

        /* TODO: move the transaction & log related stuff to table,
           ie. here deal with indexes only */
        if ((txdepth = mdb_transaction_get_depth()) < 1) {
            errno = EIO;
            return -1;
        }

        if (!(old = mdb_hash_delete(hash, lgh,key)) ||
            (old != mdb_sequence_delete(seq, lgh,key)))
        {
            /* something is really broken: get out quickly */
            errno = EIO;
            return -1;
        }
        else {
            if (mdb_row_delete(tbl, old, 0,0) < 0 ||
                mdb_log_change(tbl, txdepth, mdb_log_update,cmask,old,row) < 0)
            {
                /* errno is either EEXIST or ENOMEM set by mdb_hash_add */
                return -1;
            }

            mdb_hash_add(hash, lgh,key, row);
            mdb_sequence_add(seq, lgh,key, row);
        }
    }
    else { /* duplicate insertion is an error. keep the original row */
        mdb_row_delete(tbl, row, 0, 1);
        /* errno is either EEXIST or ENOMEM set by mdb_hash_add */
        return -1;
    }

    return 0;
}

int mdb_index_delete(mdb_table_t *tbl, mdb_row_t *row)
{
    mdb_index_t    *ix;
    int             lgh;
    void           *key;
    mdb_hash_t     *hash;
    mdb_sequence_t *seq;

    MDB_CHECKARG(tbl && row, -1);

    ix = &tbl->index;

    if (!MDB_INDEX_DEFINED(ix))
        return 0;

    hash = ix->hash;
    seq  = ix->sequence;
    lgh  = ix->length;
    key  = (void *)row->data + ix->offset;

    if (mdb_hash_delete(hash, lgh,key)    != row ||
        mdb_sequence_delete(seq, lgh,key) != row)
    {
        errno = EIO;
        return -1;
    }

    return 0;
}

mdb_row_t *mdb_index_get_row(mdb_table_t *tbl, int idxlen, void *idxval)
{
    mdb_index_t *ix;

    MDB_CHECKARG(tbl && idxlen >= 0 && idxval, NULL);

    ix = &tbl->index;

    return mdb_hash_get_data(ix->hash, idxlen, idxval);
}

int mdb_index_print(mdb_table_t *tbl, char *buf, int len)
{
#define PRINT(args...)  if (e > p) p += snprintf(p, e-p, args)
    mdb_index_t *ix;
    const char  *sep;
    char        *p, *e;
    int          i;

    MDB_CHECKARG(tbl && buf && len > 0, 0);

    ix = &tbl->index;

    MDB_PREREQUISITE(MDB_INDEX_DEFINED(ix), 0);

    e = (p = buf) + len;

    PRINT("index columns: ");

    for (i = 0, sep = "";   i < ix->ncolumn;   i++, sep = ",")
        PRINT("%s%02d", sep, ix->columns[i]);

    PRINT("\n    type    offset length\n    ---------------------"
          "\n    %-7s   %4d   %4d\n",
          mqi_data_type_str(ix->type), ix->offset, ix->length);

    return p - buf;

#undef PRINT
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
