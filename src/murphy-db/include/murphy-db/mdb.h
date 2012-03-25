#ifndef __MDB_MDB_H__
#define __MDB_MDB_H__

#include <murphy-db/mqi-types.h>

typedef struct mdb_table_s mdb_table_t;


uint32_t mdb_transaction_begin(void);
int mdb_transaction_commit(uint32_t);
int mdb_transaction_rollback(uint32_t);
uint32_t mdb_transaction_get_depth(void);


mdb_table_t *mdb_table_create(char *, char **, mqi_column_def_t *);
int mdb_table_drop(mdb_table_t *);
int mdb_table_create_index(mdb_table_t *, char **);
int mdb_table_describe(mdb_table_t *, mqi_column_def_t *, int);
int mdb_table_insert(mdb_table_t *, int, mqi_column_desc_t *, void **);
int mdb_table_select(mdb_table_t *, mqi_cond_entry_t *,
                     mqi_column_desc_t *, void *, int, int);
int mdb_table_select_by_index(mdb_table_t *, mqi_variable_t *,
                              mqi_column_desc_t *, void *);
int mdb_table_update(mdb_table_t *, mqi_cond_entry_t *,
                     mqi_column_desc_t *, void *);
int mdb_table_delete(mdb_table_t *, mqi_cond_entry_t *);

mdb_table_t *mdb_table_find(char *);
int mdb_table_get_column_index(mdb_table_t *, char *);
int mdb_table_get_size(mdb_table_t *);
char *mdb_table_get_column_name(mdb_table_t *, int);
mqi_data_type_t mdb_table_get_column_type(mdb_table_t *, int);
int mdb_table_get_column_size(mdb_table_t *, int);
int mdb_table_print_rows(mdb_table_t *, char *, int);


#endif /* __MDB_MDB_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
