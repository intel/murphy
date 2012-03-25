#ifndef __MDB_TABLE_H__
#define __MDB_TABLE_H__

#include <murphy-db/mdb.h>
#include <murphy-db/hash.h>
#include <murphy-db/list.h>
#include "index.h"
#include "column.h"
#include "log.h"

#define MDB_TABLE_HAS_INDEX(t)  MDB_INDEX_DEFINED(&t->index)

typedef struct mdb_table_s {
    char         *name;
    mdb_index_t   index;
    mdb_hash_t   *chash;         /* hash table for column names */
    int           ncolumn;
    mdb_column_t *columns;
    int           dlgh;          /* length of row data */
    int           nrow;
    mdb_dlist_t   rows;
    mdb_dlist_t   logs;         /* transaction logs */
} mdb_table_t;


#endif /* __MDB_TABLE_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
