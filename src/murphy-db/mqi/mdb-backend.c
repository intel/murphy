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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include <murphy-db/handle.h>
#include <murphy-db/mdb.h>

#include "mdb-backend.h"


static int      create_transaction_trigger(mqi_trigger_cb_t, void *);
static int      create_table_trigger(mqi_trigger_cb_t, void *);
static int      create_row_trigger(void *, mqi_trigger_cb_t, void *,
                                   mqi_column_desc_t *);
static int      create_column_trigger(void *, int, mqi_trigger_cb_t, void *,
                                      mqi_column_desc_t *);
static int      drop_transaction_trigger(mqi_trigger_cb_t, void *);
static int      drop_table_trigger(mqi_trigger_cb_t, void *);
static int      drop_row_trigger(void *, mqi_trigger_cb_t, void *);
static int      drop_column_trigger(void*, int, mqi_trigger_cb_t, void *);
static uint32_t begin_transaction(void);
static int      commit_transaction(uint32_t);
static int      rollback_transaction(uint32_t);
static uint32_t get_transaction_id(void);
static void *   create_table(char *, char **, mqi_column_def_t *);
static int      register_table_handle(void *, mqi_handle_t);
static int      create_index(void *, char **);
static int      drop_table(void *);
static int      describe(void *, mqi_column_def_t *, int);
static int      insert_into(void *, int, mqi_column_desc_t *, void **);
static int      select_general(void *, mqi_cond_entry_t *, mqi_column_desc_t *,
                               void *, int, int);
static int      select_by_index(void *, mqi_variable_t *, mqi_column_desc_t *,
                                 void *);
static int      update(void *, mqi_cond_entry_t *, mqi_column_desc_t*,void*);
static int      delete_from(void *, mqi_cond_entry_t *);
static void *   find_table(char *);
static int      get_column_index(void *, char *);
static int      get_table_size(void *);
static uint32_t get_table_stamp(void *);
static char *   get_column_name(void *, int);
static mqi_data_type_t get_column_type(void *, int);
static int      get_column_size(void *, int);
static int      print_rows(void *, char *, int);

static mqi_db_functbl_t functbl = {
    create_transaction_trigger,
    create_table_trigger,
    create_row_trigger,
    create_column_trigger,
    drop_transaction_trigger,
    drop_table_trigger,
    drop_row_trigger,
    drop_column_trigger,
    begin_transaction,
    commit_transaction,
    rollback_transaction,
    get_transaction_id,
    create_table,
    register_table_handle,
    create_index,
    drop_table,
    describe,
    insert_into,
    select_general,
    select_by_index,
    update,
    delete_from,
    find_table,
    get_column_index,
    get_table_size,
    get_table_stamp,
    get_column_name,
    get_column_type,
    get_column_size,
    print_rows
};


mqi_db_functbl_t *mdb_backend_init(void)
{
    return &functbl;
}


static int create_transaction_trigger(mqi_trigger_cb_t cb, void *data)
{
    return mdb_trigger_add_transaction_callback(cb, data);
}

static int create_table_trigger(mqi_trigger_cb_t cb, void *data)
{
    return mdb_trigger_add_table_callback(cb, data);
}

static int create_row_trigger(void *t,
                              mqi_trigger_cb_t cb,
                              void *data,
                              mqi_column_desc_t *cds)
{
    return mdb_trigger_add_row_callback((mdb_table_t *)t, cb, data, cds);
}

static int create_column_trigger(void *t,
                                 int colidx,
                                 mqi_trigger_cb_t cb,
                                 void *data,
                                 mqi_column_desc_t *cds)
{
    return mdb_trigger_add_column_callback((mdb_table_t *)t, colidx,
                                         cb, data, cds);
}

static int drop_transaction_trigger(mqi_trigger_cb_t cb, void *data)
{
    return mdb_trigger_delete_transaction_callback(cb, data);
}

static int drop_table_trigger(mqi_trigger_cb_t cb, void *data)
{
    return mdb_trigger_delete_table_callback(cb, data);
}

static int drop_row_trigger(void *t, mqi_trigger_cb_t cb, void *data)
{
    return mdb_trigger_delete_row_callback((mdb_table_t *)t, cb, data);
}

static int drop_column_trigger(void *t,
                               int colidx,
                               mqi_trigger_cb_t cb,
                               void *data)
{
    return mdb_trigger_delete_column_callback((mdb_table_t *)t,colidx,cb,data);
}

static uint32_t begin_transaction(void)
{
    uint32_t depth = mdb_transaction_begin();

    if (!depth)
        return MDB_HANDLE_INVALID;

    return depth;
}

static int commit_transaction(uint32_t depth)
{
    return mdb_transaction_commit(depth);
}

static int rollback_transaction(uint32_t depth)
{
    return mdb_transaction_rollback(depth);
}

static uint32_t get_transaction_id(void)
{
    return mdb_transaction_get_depth();
}

static void *create_table(char *name,
                          char **index_columns,
                          mqi_column_def_t *cdefs)
{
    return mdb_table_create(name, index_columns, cdefs);
}

static int register_table_handle(void *t, mqi_handle_t handle)
{
    return mdb_table_register_handle((mdb_table_t *)t, handle);
}



static int create_index(void *t, char **index_columns)
{
    return mdb_table_create_index((mdb_table_t *)t, index_columns);
}

static int drop_table(void *t)
{
    return mdb_table_drop((mdb_table_t *)t);
}

static int describe(void *t, mqi_column_def_t *defs, int len)
{
    return mdb_table_describe((mdb_table_t *)t, defs, len);
}

static int insert_into(void               *t,
                        int                 ignore,
                        mqi_column_desc_t  *cds,
                        void              **data)
{
    return mdb_table_insert((mdb_table_t *)t, ignore, cds, data);
}

static int select_general(void              *t,
                          mqi_cond_entry_t  *cond,
                          mqi_column_desc_t *cds,
                          void              *results,
                          int                size,
                          int                dim)
{
    return mdb_table_select((mdb_table_t *)t, cond, cds, results, size, dim);
}

static int select_by_index(void              *t,
                            mqi_variable_t    *idxvars,
                            mqi_column_desc_t *cds,
                            void              *result)
{
    return mdb_table_select_by_index((mdb_table_t *)t, idxvars, cds, result);
}


static int update(void              *t,
                  mqi_cond_entry_t  *cond,
                  mqi_column_desc_t *cds,
                  void              *data)
{
    return mdb_table_update((mdb_table_t *)t, cond, cds, data);
}

static int delete_from(void *t, mqi_cond_entry_t *cond)
{
    return mdb_table_delete((mdb_table_t *)t, cond);
}


static void *find_table(char *table_name)
{
    return mdb_table_find(table_name);
}


static int get_column_index(void *t, char *column_name)
{
    return mdb_table_get_column_index((mdb_table_t *)t, column_name);
}

static int get_table_size(void *t)
{
    return  mdb_table_get_size((mdb_table_t *)t);
}

static uint32_t get_table_stamp(void *t)
{
    return mdb_table_get_stamp((mdb_table_t *)t);
}

static char *get_column_name(void *t, int colidx)
{
    return  mdb_table_get_column_name((mdb_table_t *)t, colidx);
}

static mqi_data_type_t get_column_type(void *t, int colidx)
{
    return  mdb_table_get_column_type((mdb_table_t *)t, colidx);
}

static int get_column_size(void *t, int colidx)
{
    return  mdb_table_get_column_size((mdb_table_t *)t, colidx);
}

static int print_rows(void *t, char *buf, int len)
{
    return mdb_table_print_rows((mdb_table_t *)t, buf, len);
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
