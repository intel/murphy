#ifndef __MDB_LOG_H__
#define __MDB_LOG_H__

#include <murphy-db/list.h>
#include "row.h"

typedef struct mdb_table_s  mdb_table_t;

#define MDB_TRANSACTION_LOG_FOR_EACH(depth, entry, curs)                 \
    for (curs = NULL;  (entry = mdb_log_transaction_iterate(depth, &curs, 0));)

#define MDB_TRANSACTION_LOG_FOR_EACH_DELETE(depth, entry, curs)       \
    for (curs = NULL;  (entry = mdb_log_transaction_iterate(depth, &curs, 1));)

#define MDB_TABLE_LOG_FOR_EACH(table, entry, curs)                 \
    for (curs = NULL;  (entry = mdb_log_table_iterate(table, &curs, 0));)

#define MDB_TABLE_LOG_FOR_EACH_DELETE(table, entry, curs)       \
    for (curs = NULL;  (entry = mdb_log_table_iterate(table, &curs, 1));)

typedef enum {
    mdb_log_unknown = 0,
    mdb_log_insert,
    mdb_log_delete,
    mdb_log_update,
} mdb_log_type_t;

typedef struct {
    mdb_table_t    *table;
    mdb_log_type_t  change;
    mdb_row_t      *before;
    mdb_row_t      *after;
} mdb_log_entry_t;


int mdb_log_create(mdb_table_t *);
int mdb_log_change(mdb_table_t *, uint32_t, mdb_log_type_t,
                   mdb_row_t *, mdb_row_t *);
mdb_log_entry_t *mdb_log_transaction_iterate(uint32_t, void **, int);
mdb_log_entry_t *mdb_log_table_iterate(mdb_table_t *, void **, int);


#endif /* __MDB_LOG_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
