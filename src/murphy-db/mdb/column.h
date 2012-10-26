#ifndef __MDB_COLUMN_H__
#define __MDB_COLUMN_H__

#include <stdint.h>

#define MDB_COLUMN_LENGTH_MAX   1024


#include <murphy-db/mqi-types.h>

typedef struct {
    char            *name;
    mqi_data_type_t  type;
    int              length;
    int              offset;
    uint32_t         flags;
} mdb_column_t;

void mdb_column_write(mdb_column_t *, void *, mqi_column_desc_t *, void *);
void mdb_column_read(mqi_column_desc_t *, void *, mdb_column_t *, void *);
int  mdb_column_print_header(mdb_column_t *, char *, int);
int  mdb_column_print(mdb_column_t *, void *, char *, int);

#endif /* __MDB_COLUMN_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
