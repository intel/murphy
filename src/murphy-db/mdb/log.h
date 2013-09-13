/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MDB_LOG_H__
#define __MDB_LOG_H__

#include <murphy-db/list.h>
#include <murphy-db/mdb.h>
#include "row.h"

typedef struct {
    uint32_t stamp;
    uint32_t inserts;
    uint32_t deletes;
    uint32_t updates;
} mdb_opcnt_t;

#define MDB_FORWARD  true
#define MDB_BACKWARD false

#define MDB_TRANSACTION_LOG_FOR_EACH(depth, entry, fw, curs)            \
    for (curs = NULL; (entry = mdb_log_transaction_iterate(depth,&curs,fw,0));)

#define MDB_TRANSACTION_LOG_FOR_EACH_DELETE(depth, entry, fw, curs)      \
    for (curs = NULL; (entry = mdb_log_transaction_iterate(depth,&curs,fw,1));)

#define MDB_TABLE_LOG_FOR_EACH(table, entry, curs)                 \
    for (curs = NULL;  (entry = mdb_log_table_iterate(table, &curs, 0));)

#define MDB_TABLE_LOG_FOR_EACH_DELETE(table, entry, curs)       \
    for (curs = NULL;  (entry = mdb_log_table_iterate(table, &curs, 1));)

typedef enum {
    mdb_log_unknown = 0,
    mdb_log_insert,
    mdb_log_delete,
    mdb_log_update,
    mdb_log_start
} mdb_log_type_t;

typedef struct {
    mdb_table_t    *table;
    mdb_log_type_t  change;
    mqi_bitfld_t    colmask;
    union {
        mdb_row_t   *before;
        mdb_opcnt_t *cnt;
    };
    mdb_row_t      *after;
} mdb_log_entry_t;


int mdb_log_create(mdb_table_t *);
int mdb_log_change(mdb_table_t *, uint32_t, mdb_log_type_t,
                   mqi_bitfld_t, mdb_row_t *, mdb_row_t *);
mdb_log_entry_t *mdb_log_transaction_iterate(uint32_t, void **, bool, int);
mdb_log_entry_t *mdb_log_table_iterate(mdb_table_t *, void **, int);


#endif /* __MDB_LOG_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
