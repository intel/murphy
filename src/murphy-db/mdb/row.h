#ifndef __MDB_ROW_H__
#define __MDB_ROW_H__

#include <murphy-db/mqi-types.h>
#include <murphy-db/list.h>

typedef struct mdb_table_s  mdb_table_t;

typedef struct mdb_row_s {
    mdb_dlist_t  link;
    uint8_t      data[0];
} mdb_row_t;

mdb_row_t *mdb_row_create(mdb_table_t *);
mdb_row_t *mdb_row_duplicate(mdb_table_t *, mdb_row_t *);
int mdb_row_delete(mdb_table_t *, mdb_row_t *, int, int);
int mdb_row_update(mdb_table_t *, mdb_row_t *, mqi_column_desc_t *,void *,int);
int mdb_row_copy_over(mdb_table_t *, mdb_row_t *, mdb_row_t *);

#endif /* __MDB_ROW_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
