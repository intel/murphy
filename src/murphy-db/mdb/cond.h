#ifndef __MDB_COND_H__
#define __MDB_COND_H__

#include <murphy-db/mqi-types.h>

typedef struct mdb_table_s  mdb_table_t;

int mdb_cond_evaluate(mdb_table_t *, mqi_cond_entry_t **, void *);


#endif /* __MDB_COND_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
