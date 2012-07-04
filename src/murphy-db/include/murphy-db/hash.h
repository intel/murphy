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
