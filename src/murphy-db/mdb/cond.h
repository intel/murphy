#ifndef __MDB_COND_H__
#define __MDB_COND_H__

#include <murphy-db/mqi-types.h>
#include <murphy-db/mdb.h>


int mdb_cond_evaluate(mdb_table_t *, mqi_cond_entry_t **, void *);


#endif /* __MDB_COND_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
