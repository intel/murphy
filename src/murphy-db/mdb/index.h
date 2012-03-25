#ifndef __MDB_INDEX_H__
#define __MDB_INDEX_H__


typedef struct mdb_table_s mdb_table_t;
typedef struct mdb_row_s   mdb_row_t;

#include <murphy-db/mqi-types.h>
#include <murphy-db/hash.h>
#include <murphy-db/sequence.h>

#define MDB_INDEX_LENGTH_MAX 8192

#define MDB_INDEX_DEFINED(ix) ((ix)->type != mqi_unknown)

typedef struct {
    mqi_data_type_t  type;
    int              length;
    int              offset;
    mdb_hash_t      *hash;
    mdb_sequence_t  *sequence;
    int              ncolumn;
    int             *columns;   /* sorted */
} mdb_index_t;


int mdb_index_create(mdb_table_t *, char **);
void mdb_index_drop(mdb_table_t *);
void mdb_index_reset(mdb_table_t *);
int mdb_index_insert(mdb_table_t *, mdb_row_t *, int);
int mdb_index_delete(mdb_table_t *, mdb_row_t *);
mdb_row_t *mdb_index_get_row(mdb_table_t *, int, void *);
int mdb_index_print(mdb_table_t *, char *, int);


#endif /* __MDB_INDEX_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
