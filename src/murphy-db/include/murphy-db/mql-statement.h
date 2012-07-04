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

#ifndef __MQL_STATEMENT_H__
#define __MQL_STATEMENT_H__

#include <murphy-db/mql-result.h>


typedef enum {
    mql_statement_unknown = 0,
    mql_statement_show_tables,
    mql_statement_describe,
    mql_statement_create_table,
    mql_statement_create_index,
    mql_statement_drop_table,
    mql_statement_drop_index,
    mql_statement_begin,
    mql_statement_commit,
    mql_statement_rollback,
    mql_statement_insert,
    mql_statement_update,
    mql_statement_delete,
    mql_statement_select,
    /* do not add anything after this */
    mql_statement_last
} mql_statement_type_t;

typedef struct mql_statement_s {
    mql_statement_type_t  type;
    uint8_t               data[0];
} mql_statement_t;


mql_result_t *mql_exec_statement(mql_result_type_t, mql_statement_t *);
int mql_bind_value(mql_statement_t *, int, mqi_data_type_t, ...);
void mql_statement_free(mql_statement_t *);


#endif /* __MQL_STATEMENT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
