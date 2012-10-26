#ifndef __MDB_HANDLE_H__
#define __MDB_HANDLE_H__

#include <stdint.h>

#define MDB_HANDLE_INVALID (~((mdb_handle_t)0))

#define MDB_HANDLE_MAP_CREATE  mdb_handle_map_create
#define MDB_HANDLE_MAP_DESTROY mdb_handle_map_destroy

typedef uint32_t  mdb_handle_t;
typedef struct mdb_handle_map_s   mdb_handle_map_t;

mdb_handle_map_t *mdb_handle_map_create(void);
int mdb_handle_map_destroy(mdb_handle_map_t *);

mdb_handle_t mdb_handle_add(mdb_handle_map_t *, void *);
void *mdb_handle_delete(mdb_handle_map_t *, mdb_handle_t);
void *mdb_handle_get_data(mdb_handle_map_t *, mdb_handle_t);
int mdb_handle_print(mdb_handle_map_t *, char *, int);


#endif /* __MDB_HANDLE_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
