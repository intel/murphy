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

#ifndef __MQI_DB_H__
#define __MQI_DB_H__

typedef struct {
    int (*create_transaction_trigger)(mqi_trigger_cb_t, void *);
    int (*create_table_trigger)(mqi_trigger_cb_t, void *);
    int (*create_row_trigger)(void *, mqi_trigger_cb_t, void *,
                              mqi_column_desc_t *);
    int (*create_column_trigger)(void *, int, mqi_trigger_cb_t, void *,
                                 mqi_column_desc_t *);
    int (*drop_transaction_trigger)(mqi_trigger_cb_t, void *);
    int (*drop_table_trigger)(mqi_trigger_cb_t, void *);
    int (*drop_row_trigger)(void *, mqi_trigger_cb_t, void *);
    int (*drop_column_trigger)(void *, int, mqi_trigger_cb_t, void *);
    uint32_t (*begin_transaction)(void);
    int (*commit_transaction)(uint32_t);
    int (*rollback_transaction)(uint32_t);
    uint32_t (*get_transaction_id)(void);
    void *(*create_table)(char *, char **, mqi_column_def_t *);
    int (*register_table_handle)(void *, mqi_handle_t);
    int (*create_index)(void *, char **);
    int (*drop_table)(void *);
    int (*describe)(void *, mqi_column_def_t *, int);
    int (*insert_into)(void *, int, mqi_column_desc_t *, void **);
    int (*select)(void *, mqi_cond_entry_t *, mqi_column_desc_t *,
                  void *, int, int);
    int (*select_by_index)(void *, mqi_variable_t *,
                           mqi_column_desc_t *, void *);
    int (*update)(void *, mqi_cond_entry_t *, mqi_column_desc_t *,void*);
    int (*delete_from)(void *, mqi_cond_entry_t *);
    void *(*find_table)(char *);
    int (*get_column_index)(void *, char *);
    int (*get_table_size)(void *);
    uint32_t (*get_table_stamp)(void *);
    char *(*get_column_name)(void *, int);
    mqi_data_type_t (*get_column_type)(void *, int);
    int (*get_column_size)(void *, int);
    int (*print_rows)(void *, char *, int);
} mqi_db_functbl_t;



#endif /* __MQI_DB_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
