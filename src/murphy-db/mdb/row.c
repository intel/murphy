#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include "row.h"
#include "table.h"
#include "index.h"
#include "column.h"



mdb_row_t *mdb_row_create(mdb_table_t *tbl)
{
    mdb_row_t *row;

    MDB_CHECKARG(tbl, NULL);

    if (!(row = calloc(1, sizeof(mdb_row_t) + tbl->dlgh))) {
        errno = ENOMEM;
        return NULL;
    }

    MDB_DLIST_APPEND(mdb_row_t, link, row, &tbl->rows);

    return row;
}

mdb_row_t *mdb_row_duplicate(mdb_table_t *tbl, mdb_row_t *row)
{
    mdb_row_t *dup;

    MDB_CHECKARG(tbl && row, NULL);

    if (!(dup = calloc(1, sizeof(mdb_row_t) + tbl->dlgh))) {
        errno = ENOMEM;
        return NULL;
    }

    MDB_DLIST_INIT(dup->link);
    memcpy(dup->data, row->data, tbl->dlgh);

    return dup;
}

int mdb_row_delete(mdb_table_t *tbl,
                   mdb_row_t   *row,
                   int          index_update,
                   int          free_it)
{
    int sts = 0;

    (void)tbl;

    MDB_CHECKARG(row, -1);

    if (index_update && mdb_index_delete(tbl, row) < 0)
        sts = -1;

    if (!MDB_DLIST_EMPTY(row->link))
        MDB_DLIST_UNLINK(mdb_row_t, link, row);

    if (free_it)
        free(row);
    else
        MDB_DLIST_INIT(row->link);

    return sts;
}

int mdb_row_update(mdb_table_t       *tbl,
                   mdb_row_t         *row,
                   mqi_column_desc_t *cds,
                   void              *data,
                   int                index_update,
                   mqi_bitfld_t      *cmask_ret)
{
    mdb_column_t      *columns;
    mqi_column_desc_t *source_dsc;
    int                cindex;
    mqi_bitfld_t       cmask;
    int                i;

    MDB_CHECKARG(tbl && row && cds && data, -1);

    columns = tbl->columns;

    if (index_update)
        mdb_index_delete(tbl, row);

    for (cmask = i = 0;  (cindex = (source_dsc = cds + i)->cindex) >= 0;  i++){
        cmask |= (((mqi_bitfld_t)1) << cindex);
        mdb_column_write(columns + cindex, row->data, source_dsc, data);
    }

    if (index_update)
        mdb_index_insert(tbl, row, cmask, 0);

    if (cmask_ret)
        *cmask_ret = cmask;

    return 0;
}

int mdb_row_copy_over(mdb_table_t *tbl, mdb_row_t *dst, mdb_row_t *src)
{
    MDB_CHECKARG(tbl && dst && src, -1);

    if (mdb_index_delete(tbl, dst) < 0)
        return -1;

    memcpy(dst->data, src->data, tbl->dlgh);

    if (mdb_index_insert(tbl, dst, 0, 0) < 0)
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
