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

#ifndef __MQL_RESULT_H__
#define __MQL_RESULT_H__

#include <murphy-db/mqi-types.h>


/** types of mql_result_t structure */
enum mql_result_type_e {
    mql_result_error = -1, /**< error code + error message */
    mql_result_unknown = 0,
    mql_result_dontcare = mql_result_unknown, /**< will default */
    mql_result_event,
    mql_result_columns,    /**< column description of a table */
    mql_result_rows,       /**< select'ed rows */
    mql_result_string,     /**< zero terminated ASCII string  */
    mql_result_list,       /**< array of basic types, (integer, string, etc) */
};

typedef enum mql_result_type_e  mql_result_type_t;
typedef struct mql_result_s     mql_result_t;


/**
 * @brief generic return type of MQL opertaions.
 *
 * mql_result_type_t is the generic return type  of mql_exec_string()
 * and mql_exec_statement(). It is either the return status of the
 * MQL operation or the resulting data. For instance, executing an
 * insert statement will return a status (ie. mql_result_error type),
 * while the execution of a select statement will return the selected
 * rows (ie. mql_result_rows or mql_result_string depending on what
 * type was requested by mql_exec_string() or mql_exec_statement())
 *
 * To access the opaque data use the mql_result_xxx() functions
 */
struct mql_result_s {
    mql_result_type_t  type;       /**< type of the result */
    uint8_t            data[0];    /**< opaque result data */
};



int              mql_result_is_success(mql_result_t *);

int              mql_result_error_get_code(mql_result_t *);
const char      *mql_result_error_get_message(mql_result_t *);

int              mql_result_columns_get_column_count(mql_result_t *);
const char      *mql_result_columns_get_name(mql_result_t *, int);
mqi_data_type_t  mql_result_columns_get_type(mql_result_t *, int);
int              mql_result_columns_get_length(mql_result_t *, int);

int              mql_result_rows_get_row_column_count(mql_result_t *);
mqi_data_type_t  mql_result_rows_get_row_column_type(mql_result_t *, int);
int              mql_result_rows_get_row_column_index(mql_result_t *, int);
int              mql_result_rows_get_row_count(mql_result_t *);
const char      *mql_result_rows_get_string(mql_result_t*, int,int, char*,int);
int32_t          mql_result_rows_get_integer(mql_result_t *, int,int);
uint32_t         mql_result_rows_get_unsigned(mql_result_t *, int,int);
double           mql_result_rows_get_floating(mql_result_t *, int,int);

const char      *mql_result_string_get(mql_result_t *);

int              mql_result_list_get_length(mql_result_t *);
const char      *mql_result_list_get_string(mql_result_t *, int, char *, int);
int32_t          mql_result_list_get_integer(mql_result_t *, int);
int32_t          mql_result_list_get_unsigned(mql_result_t *, int);
double           mql_result_list_get_floating(mql_result_t *, int);

mqi_event_type_t mql_result_event_get_type(mql_result_t *);
mql_result_t    *mql_result_event_get_changed_rows(mql_result_t *);

void             mql_result_free(mql_result_t *);


#endif /* __MQL_RESULT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
