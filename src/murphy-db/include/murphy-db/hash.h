#ifndef __MDB_HASH_H__
#define __MDB_HASH_H__

#include <murphy-db/mqi-types.h>
#include <murphy-db/list.h>


#define MDB_HASH_TABLE_CREATE(type, max_entries)         \
    mdb_hash_table_create(max_entries,                   \
                          mdb_hash_function_##type,      \
                          mqi_data_compare_##type,       \
                          mqi_data_print_##type)

#define MDB_HASH_TABLE_DESTROY(h)                        \
    mdb_hash_table_destroy(h)

#define MDB_HASH_TABLE_FOR_EACH_WITH_KEY(htbl, data, key, cursor)       \
    for (cursor = NULL;                                                 \
        (data = mdb_hash_table_iterate(htbl, (void **)&key, &cursor)); )
#define MDB_HASH_TABLE_FOR_EACH_WITH_KEY_SAFE(htbl, data, key, cursor)  \
    MDB_HASH_TABLE_FOR_EACH_WITH_KEY(htbl, data, key, cursor)

#define MDB_HASH_TABLE_FOR_EACH(htbl, data, cursor)                     \
    for (cursor = NULL;  (data = mdb_hash_table_iterate(htbl, NULL, &cursor));)
#define MDB_HASH_TABLE_FOR_EACH_SAFE(htbl, data, cursor)                \
    MDB_HASH_TABLE_FOR_EACH(htbl, data, key, cursor)

typedef struct mdb_hash_s mdb_hash_t;

typedef int  (*mdb_hash_function_t)(int, int, int, void *);
typedef int  (*mdb_hash_compare_t)(int, void *, void *);
typedef int  (*mdb_hash_print_t)(void *, char *, int);


mdb_hash_t *mdb_hash_table_create(int, mdb_hash_function_t, mdb_hash_compare_t,
                                  mdb_hash_print_t);
int mdb_hash_table_destroy(mdb_hash_t *);
int mdb_hash_table_reset(mdb_hash_t *);
void *mdb_hash_table_iterate(mdb_hash_t *, void **, void **);
int mdb_hash_table_print(mdb_hash_t *, char *, int);

int mdb_hash_add(mdb_hash_t *, int, void *, void *);
void *mdb_hash_delete(mdb_hash_t *, int, void *);
void *mdb_hash_get_data(mdb_hash_t *, int, void *);

int mdb_hash_function_integer(int, int, int, void *);
int mdb_hash_function_unsignd(int, int, int, void *);
int mdb_hash_function_string(int, int, int, void *);
int mdb_hash_function_pointer(int, int, int, void *);
int mdb_hash_function_varchar(int, int, int, void *);
int mdb_hash_function_blob(int, int, int, void *);


#endif /* __MDB_HASH_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
