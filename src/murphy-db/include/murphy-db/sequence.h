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
