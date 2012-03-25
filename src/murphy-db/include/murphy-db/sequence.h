#ifndef __MDB_SEQUENCE_H__
#define __MDB_SEQUENCE_H__

#include <murphy-db/mqi-types.h>


#define MDB_SEQUENCE_TABLE_CREATE(type, alloc)              \
    mdb_sequence_table_create(alloc,                        \
                              mqi_data_compare_##type,      \
                              mqi_data_print_##type)

#define MDB_SEQUENCE_FOR_EACH(seq, data, cursor)            \
    for (cursor = NULL;  (data = mdb_sequence_iterate(seq, &cursor)); )

#define MDB_SEQUENCE_FOR_EACH_SAFE(seq, data, cursor)       \
    MDB_SEQUENCE_FOR_EACH(seq, data, cursor)


typedef struct mdb_sequence_s mdb_sequence_t;

typedef int  (*mdb_sequence_compare_t)(int, void *, void *);
typedef int  (*mdb_sequence_print_t)(void *, char *, int);


mdb_sequence_t *mdb_sequence_table_create(int, mdb_sequence_compare_t,
                                          mdb_sequence_print_t);
int mdb_sequence_table_destroy(mdb_sequence_t *);
int mdb_sequence_table_get_size(mdb_sequence_t *);
int mdb_sequence_table_reset(mdb_sequence_t *);
int mdb_sequence_table_print(mdb_sequence_t *, char *, int);

int mdb_sequence_add(mdb_sequence_t *, int, void *, void *);
void *mdb_sequence_delete(mdb_sequence_t *, int, void *);
void *mdb_sequence_iterate(mdb_sequence_t *, void **);
void mdb_sequence_cursor_destroy(mdb_sequence_t *, void **);



#endif /* __MDB_SEQUENCE_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
