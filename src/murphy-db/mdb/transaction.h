#ifndef __MDB_TRANSACTION_H__
#define __MDB_TRANSACTION_H__

typedef struct mdb_table_s mdb_table_t;

uint32_t mdb_transaction_begin(void);
int mdb_transaction_commit(uint32_t);
int mdb_transaction_rollback(uint32_t);
int mdb_transaction_drop_table(mdb_table_t *);
uint32_t mdb_transaction_get_depth(void);


#endif /* __MDB_TRANSACTION_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
