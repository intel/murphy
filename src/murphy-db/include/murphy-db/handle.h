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
