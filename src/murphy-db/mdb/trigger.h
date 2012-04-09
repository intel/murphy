#ifndef __MDB_TRIGGER_H__
#define __MDB_TRIGGER_H__

#include <murphy-db/mqi-types.h>
#include <murphy-db/list.h>



typedef struct {
    mdb_dlist_t   row_change;
    mdb_dlist_t   column_change[0];
} mdb_trigger_t;

void mdb_trigger_init(mdb_trigger_t *, int);
void mdb_trigger_reset(mdb_trigger_t *, int);

void mdb_trigger_column_change(mdb_table_t*, mqi_bitfld_t,
                               mdb_row_t *, mdb_row_t *);

void mdb_trigger_row_delete(mdb_table_t *, mdb_row_t *);
void mdb_trigger_row_insert(mdb_table_t *, mdb_row_t *);

void mdb_trigger_table_create(mdb_table_t *);
void mdb_trigger_table_drop(mdb_table_t *);

void mdb_trigger_transaction_start(void);
void mdb_trigger_transaction_end(void);

#endif /* __MDB_TRIGGER_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
