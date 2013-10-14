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

#ifndef __MQI_MQI_H__
#define __MQI_MQI_H__

#include <murphy-db/mqi-types.h>

#define MQI_ALL               NULL
#define MQI_NO_INDEX          NULL

/* table flags */
#define MQI_PERSISTENT        (1 << 0)
#define MQI_TEMPORARY         (1 << 1)
#define MQI_ANY               (MQI_PERSISTENT | MQI_TEMPORARY)
#define MQI_TABLE_TYPE_MASK   (MQI_PERSISTENT | MQI_TEMPORARY)


#define MQI_COLUMN_DEFINITION(name, type...)  \
    {name, type, 0}

#define MQI_COLUMN_SELECTOR(column_index, result_structure, result_member) \
    {column_index, MQI_OFFSET(result_structure, result_member)}

#define MQI_VARCHAR(s)    mqi_varchar, s
#define MQI_INTEGER       mqi_integer, 0
#define MQI_UNSIGNED      mqi_unsignd, 0
#define MQI_BLOB(s)       mqi_blob,    s

#define MQI_COLUMN(column_index)  \
    {.type=mqi_column, .u.column=column_index}

#define MQI_VALUE(typ, val)  \
    {.type=mqi_##typ, .v.typ=val}

#define MQI_VARIABLE(typ, val) \
    {.type=mqi_variable, .u.variable=MQI_VALUE(typ, val)}

#define MQI_OPERATOR(op) \
    {.type=mqi_operator, .u.operator_=mqi_##op}


#define MQI_EXPRESSION(seq)        MQI_OPERATOR(begin), seq, MQI_OPERATOR(end),


#define MQI_STRING_VAL(val)        MQI_VALUE(varchar, (char **)&val),
#define MQI_INTEGER_VAL(val)       MQI_VALUE(integer, (int32_t *)&val),
#define MQI_UNSIGNED_VAL(val)      MQI_VALUE(unsignd, (uint32_t *)&val),
#define MQI_BLOB_VAL(val)          MQI_VALUE(blob,    (void **)&val),

#define MQI_STRING_VAR(val)        MQI_VARIABLE(varchar, (char **)&val)
#define MQI_INTEGER_VAR(val)       MQI_VARIABLE(integer, (int32_t *)&val)
#define MQI_UNSIGNED_VAR(val)      MQI_VARIABLE(unsignd, (uint32_t *)&val)
#define MQI_BLOB_VAR(val)          MQI_VARIABLE(blob,    (void **)&val)


#define MQI_AND                    MQI_OPERATOR(and),
#define MQI_OR                     MQI_OPERATOR(or),

#define MQI_LESS(a,b)              a, MQI_OPERATOR(less), b,
#define MQI_LESS_OR_EQUAL(a,b)     a, MQI_OPERATOR(leq),  b,
#define MQI_EQUAL(a,b)             a, MQI_OPERATOR(eq),   b,
#define MQI_GREATER_OR_EQUAL(a,b)  a, MQI_OPERATOR(geq),  b,
#define MQI_GREATER(a,b)           a, MQI_OPERATOR(gt),   b,

#define MQI_NOT(val)               MQI_OPERATOR(not), val,

#define MQI_COLUMN_DEFINITION_LIST(name, columns...)            \
    static mqi_column_def_t name[] = {                          \
        columns,                                                \
        {NULL, mqi_unknown, 0, 0}                               \
    }

#define MQI_INDEX_COLUMN(column_name)  column_name,

#define MQI_INDEX_DEFINITION(name, column_names...)             \
    static char *name[] = {                                     \
        column_names                                            \
        NULL                                                    \
    }

#define MQI_INDEX_VALUE(name, varlist...)                       \
    static mqi_variable_t name[] = {varlist}

#define MQI_COLUMN_SELECTION_LIST(name, columns...)             \
    static mqi_column_desc_t name[] = {                         \
        columns,                                                \
        {-1, 1}                                                 \
    }
#define MQI_WHERE_CLAUSE(name, seq...)                          \
    static mqi_cond_entry_t  name[] = {                         \
        seq                                                     \
        MQI_OPERATOR(end)                                       \
    }

#define MQI_BEGIN                                               \
    mqi_begin_transaction()

#define MQI_COMMIT(id)                                          \
    mqi_commit_transaction(id)

#define MQI_ROLLBACK(id)                                        \
    mqi_rollback_transaction(id)

#define MQI_CREATE_TABLE(name, type, column_defs, index_def)    \
    mqi_create_table(name, type, index_def, column_defs)

#define MQI_DESCRIBE(table, coldefs)                            \
    mqi_describe(table, coldefs, MQI_DIMENSION(coldefs))

#define MQI_INSERT_INTO(table, column_descs, data)              \
    mqi_insert_into(table, 0, column_descs, (void **)data)

#define MQI_REPLACE(table, column_descs, data)                  \
    mqi_insert_into(table, 1, column_descs, (void **)data)

#define MQI_SELECT(columns, table, where, result)               \
    mqi_select(table, where, columns, result,                   \
               sizeof(result[0]), MQI_DIMENSION(result))

#define MQI_SELECT_BY_INDEX(columns, table, idxvars, result)    \
    mqi_select_by_index(table, idxvars, columns, result)

#define MQI_UPDATE(table, column_descs, data, where)            \
    mqi_update(table, where, column_descs, data)

#define MQI_DELETE(table, where)                                \
    mqi_delete_from(table, where)



int mqi_open(void);
int mqi_close(void);

int mqi_show_tables(uint32_t, char **, int);

int mqi_create_transaction_trigger(mqi_trigger_cb_t, void *);
int mqi_create_table_trigger(mqi_trigger_cb_t, void *);
int mqi_create_row_trigger(mqi_handle_t, mqi_trigger_cb_t, void *,
                           mqi_column_desc_t *);
int mqi_create_column_trigger(mqi_handle_t, int, mqi_trigger_cb_t, void *,
                              mqi_column_desc_t *);
int mqi_drop_transaction_trigger(mqi_trigger_cb_t, void *);
int mqi_drop_table_trigger(mqi_trigger_cb_t, void *);
int mqi_drop_row_trigger(mqi_handle_t, mqi_trigger_cb_t,void *);
int mqi_drop_column_trigger(mqi_handle_t, int, mqi_trigger_cb_t, void *);
mqi_handle_t mqi_begin_transaction(void);
int mqi_commit_transaction(mqi_handle_t);
int mqi_rollback_transaction(mqi_handle_t);
mqi_handle_t mqi_get_transaction_handle(void);
uint32_t mqi_get_transaction_depth(void);
mqi_handle_t mqi_create_table(char *, uint32_t, char **, mqi_column_def_t *);
int mqi_create_index(mqi_handle_t, char **);
int mqi_drop_table(mqi_handle_t);
int mqi_describe(mqi_handle_t, mqi_column_def_t *, int);
int mqi_insert_into(mqi_handle_t, int, mqi_column_desc_t *, void **);
int mqi_delete_from(mqi_handle_t, mqi_cond_entry_t *);
int mqi_update(mqi_handle_t, mqi_cond_entry_t *, mqi_column_desc_t *, void *);
int mqi_select(mqi_handle_t, mqi_cond_entry_t *, mqi_column_desc_t *,
               void *, int, int);
int mqi_select_by_index(mqi_handle_t, mqi_variable_t *,
                        mqi_column_desc_t *, void *);

mqi_handle_t mqi_get_table_handle(char *);
int mqi_get_column_index(mqi_handle_t, char *);
int mqi_get_table_size(mqi_handle_t);
char *mqi_get_column_name(mqi_handle_t, int);
mqi_data_type_t mqi_get_column_type(mqi_handle_t, int);
int mqi_get_column_size(mqi_handle_t, int);
uint32_t mqi_get_table_stamp(mqi_handle_t);
int mqi_print_rows(mqi_handle_t, char *, int);


#endif /* __MQI_MQI_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
