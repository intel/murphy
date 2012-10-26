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

#ifndef __MDB_MDB_H__
#define __MDB_MDB_H__

#include <murphy-db/mqi-types.h>

typedef struct mdb_table_s mdb_table_t;


int mdb_trigger_add_column_callback(mdb_table_t *, int, mqi_trigger_cb_t,
                                  void *, mqi_column_desc_t *);
int mdb_trigger_delete_column_callback(mdb_table_t *, int,
                                     mqi_trigger_cb_t, void *);
int mdb_trigger_add_row_callback(mdb_table_t *, mqi_trigger_cb_t, void *,
                               mqi_column_desc_t *);
int mdb_trigger_delete_row_callback(mdb_table_t *, mqi_trigger_cb_t, void *);
int mdb_trigger_add_table_callback(mqi_trigger_cb_t, void *);
int mdb_trigger_delete_table_callback(mqi_trigger_cb_t, void *);
int mdb_trigger_add_transaction_callback(mqi_trigger_cb_t, void *);
int mdb_trigger_delete_transaction_callback(mqi_trigger_cb_t, void *);

uint32_t mdb_transaction_begin(void);
int mdb_transaction_commit(uint32_t);
int mdb_transaction_rollback(uint32_t);
uint32_t mdb_transaction_get_depth(void);


mdb_table_t *mdb_table_create(char *, char **, mqi_column_def_t *);
int mdb_table_register_handle(mdb_table_t *, mqi_handle_t);
int mdb_table_drop(mdb_table_t *);
int mdb_table_create_index(mdb_table_t *, char **);
int mdb_table_describe(mdb_table_t *, mqi_column_def_t *, int);
int mdb_table_insert(mdb_table_t *, int, mqi_column_desc_t *, void **);
int mdb_table_select(mdb_table_t *, mqi_cond_entry_t *,
                     mqi_column_desc_t *, void *, int, int);
int mdb_table_select_by_index(mdb_table_t *, mqi_variable_t *,
                              mqi_column_desc_t *, void *);
int mdb_table_update(mdb_table_t *, mqi_cond_entry_t *,
                     mqi_column_desc_t *, void *);
int mdb_table_delete(mdb_table_t *, mqi_cond_entry_t *);


mdb_table_t *mdb_table_find(char *);
int mdb_table_get_column_index(mdb_table_t *, char *);
int mdb_table_get_size(mdb_table_t *);
char *mdb_table_get_column_name(mdb_table_t *, int);
mqi_data_type_t mdb_table_get_column_type(mdb_table_t *, int);
int mdb_table_get_column_size(mdb_table_t *, int);
uint32_t mdb_table_get_stamp(mdb_table_t *);
int mdb_table_print_rows(mdb_table_t *, char *, int);


#endif /* __MDB_MDB_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
