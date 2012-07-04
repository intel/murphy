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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <alloca.h>
#include <errno.h>

#include <murphy-db/assert.h>
#include <murphy-db/mqi.h>
#include <murphy-db/hash.h>
#include "mql-parser.h"

/* Note: HANDLE_TO_PTR(MQI_HANDLE_INVALID) == NULL */
#define HANDLE_TO_PTR(h)   (((void *)1) + (h))
#define PTR_TO_HANDLE(p)   (mqi_handle_t)((p) - ((void *)1))

static mdb_hash_t *transact_handles;

static int init(void);
static int add_handle(char *, mqi_handle_t);
static mqi_handle_t delete_handle(char *);


int mql_begin_transaction(char *name)
{
    mqi_handle_t h;

    MDB_CHECKARG(name, -1);

    if ((h = mqi_begin_transaction()) == MQI_HANDLE_INVALID)
        return -1;

    if (add_handle(name, h) < 0) {
        mqi_rollback_transaction(h);
        return -1;
    }

    return 0;
}


int mql_rollback_transaction(char *name)
{
    mqi_handle_t h;

    MDB_CHECKARG(name, -1);

    if ((h = delete_handle(name)) == MQI_HANDLE_INVALID)
        return -1;

    if (mqi_rollback_transaction(h) < 0)
        return -1;

    return 0;
}

int mql_commit_transaction(char *name)
{
    mqi_handle_t h;

    MDB_CHECKARG(name, -1);

    if ((h = delete_handle(name)) == MQI_HANDLE_INVALID)
        return -1;

    if (mqi_commit_transaction(h) < 0)
        return -1;

    return 0;
}


static int init(void)
{
    static bool done = false;

    int sts = 0;

    if (!done) {
        if (!(transact_handles = MDB_HASH_TABLE_CREATE(string, 16)))
            sts = -1;

        done = true;
    }

    return sts;
}

static int add_handle(char *name, mqi_handle_t handle)
{
    if (init() < 0)
        return -1;

    if (mdb_hash_add(transact_handles, 0,name, HANDLE_TO_PTR(handle)) < 0)
        return -1;

    return 0;
}

static mqi_handle_t delete_handle(char *name)
{
    void *ptr;

    if (init() < 0)
        return MQI_HANDLE_INVALID;

    if (!(ptr = mdb_hash_delete(transact_handles, 0,name)))
        return MQI_HANDLE_INVALID;

    return PTR_TO_HANDLE(ptr);
}




/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
