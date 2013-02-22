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
#include <murphy-db/mql.h>
#include "mql-parser.h"

typedef struct {
    mqi_data_type_t  type;
    union {
        const char  *varchar;
        int32_t      integer;
        uint32_t     unsignd;
        double       floating;
        void        *generic;
    } v;
} value_t;

typedef struct {
    mql_statement_type_t type;
    uint32_t             flags;
} shtable_statement_t;

typedef struct {
    mql_statement_type_t type;
    mqi_handle_t         table;
} describe_statement_t;

typedef struct {
    mql_statement_type_t type;
    char                 trnam[0];
} transact_statement_t;

typedef struct {
    mql_statement_type_t type;
    mqi_handle_t         table;
    int                  ignore;
    int                  ncolumn;
    mqi_column_desc_t   *columns;
    void                *rows[2];
    int                  nbind;
    value_t              values[0];
} insert_statement_t;

typedef struct {
    mql_statement_type_t type;
    mqi_handle_t         table;
    mqi_column_desc_t   *columns;
    mqi_cond_entry_t    *cond;
    int                  nbind;
    value_t              values[0];
} update_statement_t;

typedef struct {
    mql_statement_type_t  type;
    mqi_handle_t          table;
    mqi_cond_entry_t     *cond;
    int                   nbind;
    value_t               values[0];
} delete_statement_t;

typedef struct {
    mql_statement_type_t type;
    mqi_handle_t         table;
    int                  rowsize;
    int                  ncolumn;
    mqi_column_desc_t   *columns;
    char               **colnames;
    mqi_data_type_t     *coltypes;
    int                 *colsizes;
    mqi_cond_entry_t    *cond;
    int                  nbind;
    value_t              values[0];
} select_statement_t;



static void count_condition_values(int, mqi_cond_entry_t *, int*, int*, int*);
static void count_column_values(mqi_column_desc_t *, mqi_data_type_t*, void*,
                                int *, int *, int *);
static void copy_column_values(int, mqi_data_type_t *, mqi_column_desc_t *,
                               mqi_column_desc_t *, value_t **, value_t **,
                               char **, void *, void *);
static void copy_conditions_and_values(int,mqi_cond_entry_t*,mqi_cond_entry_t*,
                                       value_t**, value_t**, char**);

static mql_result_t *exec_show_tables(mql_result_type_t, shtable_statement_t*);
static mql_result_t *exec_describe(mql_result_type_t, describe_statement_t *);
static mql_result_t *exec_begin(transact_statement_t *);
static mql_result_t *exec_commit(transact_statement_t *);
static mql_result_t *exec_rollback(transact_statement_t *);
static mql_result_t *exec_insert(insert_statement_t *);
static mql_result_t *exec_update(update_statement_t *);
static mql_result_t *exec_delete(delete_statement_t *);
static mql_result_t *exec_select(mql_result_type_t, select_statement_t *);

static int bind_update_value(update_statement_t *,int,mqi_data_type_t,va_list);
static int bind_delete_value(delete_statement_t *,int,mqi_data_type_t,va_list);
static int bind_select_value(select_statement_t *,int,mqi_data_type_t,va_list);
static int bind_value(value_t *, mqi_data_type_t, va_list);

mql_statement_t *mql_make_show_tables_statement(uint32_t flags)
{
    shtable_statement_t *st;

    if (!(st = calloc(1, sizeof(shtable_statement_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    st->type  = mql_statement_show_tables;
    st->flags = flags;

    return (mql_statement_t *)st;
}

mql_statement_t *mql_make_describe_statement(mqi_handle_t table)
{
    describe_statement_t *dis;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID, NULL);

    if (!(dis = calloc(1, sizeof(describe_statement_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    dis->type  = mql_statement_describe;
    dis->table = table;

    return (mql_statement_t *)dis;
}

mql_statement_t *mql_make_transaction_statement(mql_statement_type_t  type,
                                                char                 *trnam)
{
    transact_statement_t *tra;

    MDB_CHECKARG((type == mql_statement_begin  ||
                  type == mql_statement_commit ||
                  type == mql_statement_rollback)
                 && trnam && trnam[0], NULL);

    if (!(tra = calloc(1, sizeof(transact_statement_t) + strlen(trnam) + 1))) {
        errno = ENOMEM;
        return NULL;
    }

    tra->type = type;
    strcpy(tra->trnam, trnam);

    return (mql_statement_t *)tra;
}


mql_statement_t *mql_make_insert_statement(mqi_handle_t       table,
                                           int                ignore,
                                           int                ncolumn,
                                           mqi_data_type_t   *coltypes,
                                           mqi_column_desc_t *columns,
                                           void              *data)
{
    insert_statement_t *ins;
    value_t *bindv;
    value_t *constv;
    char    *strpool;
    int      vallgh;
    int      cdsclgh;
    int      datalgh;
    int      nbind   = 0;
    int      nconst  = 0;
    int      poollen = 0;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID &&
                 ncolumn > 0 && columns && data, NULL);

    /*
     * calculate the number of constant and bindable values
     */
    count_column_values(columns, coltypes, data, &nbind,&nconst,&poollen);

    /*
     * set up the statement structure
     */
    vallgh  = sizeof(     value_t     ) * (nbind + nconst);
    cdsclgh = sizeof(mqi_column_desc_t) * (ncolumn + 1);

    datalgh = vallgh + cdsclgh + poollen;

    if (!(ins = calloc(1, sizeof(insert_statement_t) + datalgh))) {
        errno = ENOMEM;
        return NULL;
    }

    ins->type    = mql_statement_insert;
    ins->table   = table;
    ins->ignore  = ignore;
    ins->columns = (mqi_column_desc_t *)(ins->values + (nbind + nconst));
    ins->rows[0] = ins->values;
    ins->nbind   = nbind;

    strpool = (void *)(ins->columns + (ncolumn + 1));

    /*
     * copy column values
     */
    bindv  = ins->values;
    constv = bindv + nbind;

    copy_column_values(ncolumn, coltypes, columns, ins->columns,
                       &bindv, &constv, &strpool, data, ins->values);

    return (mql_statement_t *)ins;
}


mql_statement_t *mql_make_update_statement(mqi_handle_t       table,
                                           int                ncond,
                                           mqi_cond_entry_t  *conds,
                                           int                ncolumn,
                                           mqi_data_type_t   *coltypes,
                                           mqi_column_desc_t *columns,
                                           void              *data)
{
    update_statement_t *upd;
    value_t *bindv;
    value_t *constv;
    char    *strpool;
    int      vallgh;
    int      cdsclgh;
    int      cndlgh;
    int      datalgh;
    int      nbind   = 0;
    int      nconst  = 0;
    int      poollen = 0;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID &&
                 (!ncond || (ncond > 0 && conds)) &&
                 ncolumn > 0 && coltypes && columns && data, NULL);

    /*
     * calculate the number of constant and bindable values
     */
    count_column_values(columns, coltypes, data, &nbind,&nconst,&poollen);
    count_condition_values(ncond, conds, &nbind, &nconst, &poollen);

    /*
     * set up the statement structure
     */
    vallgh  = sizeof(     value_t     ) * (nbind + nconst);
    cdsclgh = sizeof(mqi_column_desc_t) * (ncolumn + 1);
    cndlgh  = sizeof(mqi_cond_entry_t ) *  ncond;

    datalgh = vallgh + cdsclgh + cndlgh + poollen;

    if (!(upd = calloc(1, sizeof(update_statement_t) + datalgh))) {
        errno = ENOMEM;
        return NULL;
    }

    upd->type    = mql_statement_update;
    upd->table   = table;
    upd->columns = (mqi_column_desc_t *)(upd->values + (nbind + nconst));
    upd->cond    = (mqi_cond_entry_t *)(upd->columns + (ncolumn + 1));
    upd->nbind   = nbind;

    strpool = (void *)(upd->cond + ncond);

    /*
     * copy column values, conditions and their values
     */
    bindv  = upd->values;
    constv = bindv + nbind;

    copy_column_values(ncolumn, coltypes, columns, upd->columns,
                       &bindv, &constv, &strpool, data, upd->values);

    copy_conditions_and_values(ncond, conds, upd->cond,
                               &bindv, &constv, &strpool);

    return (mql_statement_t *)upd;
}


mql_statement_t *mql_make_delete_statement(mqi_handle_t       table,
                                           int                ncond,
                                           mqi_cond_entry_t  *conds)
{
    delete_statement_t *del;
    value_t *bindv;
    value_t *constv;
    char    *strpool;
    int      vallgh;
    int      cndlgh;
    int      datalgh;
    int      nbind   = 0;
    int      nconst  = 0;
    int      poollen = 0;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID &&
                 (!ncond || (ncond > 0 && conds)), NULL);

    /*
     * calculate the number of constant and bindable values
     */
    count_condition_values(ncond, conds, &nbind, &nconst, &poollen);

    /*
     * set up the statement structure
     */
    vallgh  = sizeof(     value_t     ) * (nbind + nconst);
    cndlgh  = sizeof(mqi_cond_entry_t ) *  ncond;

    datalgh = vallgh + cndlgh + poollen;

    if (!(del = calloc(1, sizeof(delete_statement_t) + datalgh))) {
        errno = ENOMEM;
        return NULL;
    }

    del->type  = mql_statement_delete;
    del->table = table;
    del->cond  = (mqi_cond_entry_t *)(del->values + (nbind + nconst));
    del->nbind = nbind;

    strpool = (void *)(del->cond + ncond);

    /*
     * copy column values, conditions and their values
     */
    bindv  = del->values;
    constv = bindv + nbind;

    copy_conditions_and_values(ncond, conds, del->cond,
                               &bindv, &constv, &strpool);

    return (mql_statement_t *)del;
}

mql_statement_t *mql_make_select_statement(mqi_handle_t       table,
                                           int                rowsize,
                                           int                ncond,
                                           mqi_cond_entry_t  *conds,
                                           int                ncolumn,
                                           char             **colnames,
                                           mqi_data_type_t   *coltypes,
                                           int               *colsizes,
                                           mqi_column_desc_t *columns)
{
    select_statement_t *sel;
    value_t *bindv;
    value_t *constv;
    char    *strpool;
    int      vallgh;
    int      cdsclgh;
    int      cnamlgh;
    int      ctyplgh;
    int      csizlgh;
    int      cndlgh;
    int      datalgh;
    int      colnamlgh[MQI_COLUMN_MAX];
    int      nbind   = 0;
    int      nconst  = 0;
    int      poollen = 0;
    int      i;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID &&
                 (!ncond || (ncond >= 0 && conds)) &&
                 ncolumn > 0 && columns, NULL);

    /*
     * calculate the number of constant and bindable values
     */
    count_condition_values(ncond, conds, &nbind, &nconst, &poollen);

    for (i = 0;   i < ncolumn;   i++)
        poollen += (colnamlgh[i] = strlen(colnames[i]) + 1);


    /*
     * set up the statement structure
     */
    vallgh  = sizeof(     value_t     ) * (nbind + nconst);
    cdsclgh = sizeof(mqi_column_desc_t) * (ncolumn + 1);
    cnamlgh = sizeof(      char *     ) *  ncolumn;
    ctyplgh = sizeof( mqi_data_type_t ) *  ncolumn;
    csizlgh = sizeof(       int       ) *  ncolumn;
    cndlgh  = sizeof(mqi_cond_entry_t ) *  ncond;

    datalgh = vallgh + cdsclgh + cnamlgh + ctyplgh + csizlgh + cndlgh +poollen;

    if (!(sel = calloc(1, sizeof(select_statement_t) + datalgh))) {
        errno = ENOMEM;
        return NULL;
    }

    sel->type     = mql_statement_select;
    sel->table    = table;
    sel->rowsize  = rowsize;
    sel->ncolumn  = ncolumn;
    sel->columns  = (mqi_column_desc_t *)(sel->values + (nbind + nconst));
    sel->colnames = (char **)(sel->columns + (ncolumn+1));
    sel->coltypes = (mqi_data_type_t *)(sel->colnames + ncolumn);
    sel->colsizes = (int *)(sel->coltypes + ncolumn);
    sel->cond     = (mqi_cond_entry_t *)(sel->colsizes + ncolumn);
    sel->nbind    = nbind;

    strpool = (char *)(sel->cond + ncond);

    if (!ncond)
        sel->cond = NULL;

    /*
     * copy conditions and values
     */
    bindv  = sel->values;
    constv = bindv + nbind;

    copy_conditions_and_values(ncond, conds, sel->cond,
                               &bindv, &constv, &strpool);
    /*
     * copy column descriptors, types and sizes
     */
    memcpy(sel->columns,  columns,  cdsclgh);
    memcpy(sel->coltypes, coltypes, ctyplgh);
    memcpy(sel->colsizes, colsizes, csizlgh);

    /*
     * copy column names
     */
    for (i = 0;   i < ncolumn;   i++) {
        sel->colnames[i] = (char*)memcpy(strpool, colnames[i], colnamlgh[i]);
        strpool += colnamlgh[i];
    }

    return (mql_statement_t *)sel;
}

int
mql_bind_value(mql_statement_t *s, int id, mqi_data_type_t type, ...)
{
    int idx = id - 1;
    va_list data;
    int sts;

    MDB_CHECKARG(s && id > 0, -1);

    va_start(data, type);

    switch (s->type) {

    case mql_statement_update:
        sts = bind_update_value((update_statement_t *)s, idx, type, data);
        break;

    case mql_statement_delete:
        sts = bind_delete_value((delete_statement_t *)s, idx, type, data);
        break;

    case mql_statement_select:
        sts = bind_select_value((select_statement_t *)s, idx, type, data);
        break;

    default:
        errno = EBADRQC;
        sts   = -1;
        break;
    }

    va_end(data);

    return sts;
}

mql_result_t *mql_exec_statement(mql_result_type_t type, mql_statement_t *s)
{
    mql_result_t *result;

    MDB_CHECKARG(s, NULL);

    switch (s->type) {

    case mql_statement_show_tables:
        result = exec_show_tables(type, (shtable_statement_t *)s);
        break;

    case mql_statement_describe:
        result = exec_describe(type, (describe_statement_t *)s);
        break;

    case mql_statement_begin:
        result = exec_begin((transact_statement_t *)s);
        break;

    case mql_statement_commit:
        result = exec_commit((transact_statement_t *)s);
        break;

    case mql_statement_rollback:
        result = exec_rollback((transact_statement_t *)s);
        break;

    case mql_statement_insert:
        result = exec_insert((insert_statement_t *)s);
        break;

    case mql_statement_update:
        result = exec_update((update_statement_t *)s);
        break;

    case mql_statement_delete:
        result = exec_delete((delete_statement_t *)s);
        break;

    case mql_statement_select:
        result = exec_select(type, (select_statement_t *)s);
        break;

    default:
        result = mql_result_error_create(EBADRQC, "statement execution failed:"
                                         " %s", strerror(EBADRQC));
        break;
    }

    return result;
}


void mql_statement_free(mql_statement_t *s)
{
    free(s);
}


static void count_column_values(mqi_column_desc_t *cds,
                                mqi_data_type_t   *coltypes,
                                void              *data,
                                int               *nbind_ret,
                                int               *nconst_ret,
                                int               *poollen_ret)
{
    mqi_column_desc_t *cd;
    int   offs;
    int   bidx;
    char *str;
    int   nbind   = *nbind_ret;
    int   nconst  = 0;
    int   poollen = 0;
    int   i;

    for (i = 0;   (cd = cds + i)->cindex >= 0;   i++) {

        if ((offs = cd->offset) >= 0) {
            if (coltypes[i] == mqi_varchar && (str = *(char**)(data + offs)))
                poollen += strlen(str) + 1;
            nconst++;
        }
        else {
            if ((bidx = -(cd->offset - 1)) + 1 > nbind)
                nbind = bidx + 1;
        }
    }

    *nbind_ret    = nbind;
    *nconst_ret  += nconst;
    *poollen_ret += poollen;
}

static void count_condition_values(int               ncond,
                                   mqi_cond_entry_t *conds,
                                   int              *nbind_ret,
                                   int              *nconst_ret,
                                   int              *poollen_ret)
{
    mqi_cond_entry_t *ce;
    mqi_variable_t   *var;
    uint32_t flags;
    int bidx;
    char *str;
    int nbind   = *nbind_ret;
    int nconst  = 0;
    int poollen = 0;
    int i;

    for (i = 0;    i < ncond;    i++) {
        ce = conds + i;

        if (ce->type == mqi_variable) {
            var   = &ce->u.variable;
            flags = var->flags;

            if (!(flags & MQL_BINDABLE)) {
                if (var->type == mqi_varchar && (str = *(var->v.varchar)))
                    poollen += strlen(str) + 1;
                nconst++;
            }
            else {
                if ((bidx = MQL_BIND_INDEX(flags)) + 1 > nbind)
                    nbind = bidx + 1;
            }

        }
    }

    *nbind_ret    = nbind;
    *nconst_ret  += nconst;
    *poollen_ret += poollen;
}

static void copy_column_values(int                 ncol,
                               mqi_data_type_t    *coltypes,
                               mqi_column_desc_t  *src_cols,
                               mqi_column_desc_t  *dst_cols,
                               value_t           **bindv_ptr,
                               value_t           **constv_ptr,
                               char              **strpool_ptr,
                               void               *data,
                               void               *values)
{
    value_t           *bindv   = *bindv_ptr;
    value_t           *constv  = *constv_ptr;
    void              *strpool = *strpool_ptr;
    mqi_column_desc_t *col;
    value_t           *val;
    mqi_data_type_t    type;
    int                offs;
    void              *vptr;
    char              *str;
    int                len;
    int                i;

    for (i = 0;  i < ncol;  i++) {
        type = coltypes[i];
        *(col = dst_cols + i) = src_cols[i];

        if ((offs = col->offset) < 0)
            val  = bindv - (offs + 1);
        else {
            val  = constv++;
            vptr = data + offs;

            switch (type) {
            case mqi_varchar:
                str = *(char **)vptr;
                len = strlen(str) + 1;
                val->v.varchar = (char *)memcpy(strpool, str, len);
                strpool += len;
                break;
            case mqi_integer:
                val->v.integer = *(int32_t *)vptr;
                break;
            case mqi_unsignd:
                val->v.unsignd  = *(uint32_t *)vptr;
                break;
            case mqi_floating:
                val->v.floating = *(double *)vptr;
                break;
            default:
                break;
            }
        }

        val->type   = type;
        col->offset = (void *)&val->v.generic - values;
    }

    col = dst_cols + i;
    col->cindex = -1;
    col->offset = -1;

    *bindv_ptr   = bindv;
    *constv_ptr  = constv;
    *strpool_ptr = strpool;
}

static void copy_conditions_and_values(int                ncond,
                                       mqi_cond_entry_t  *src_conds,
                                       mqi_cond_entry_t  *dst_conds,
                                       value_t          **bindv_ptr,
                                       value_t          **constv_ptr,
                                       char             **strpool_ptr)
{
    value_t          *bindv   = *bindv_ptr;
    value_t          *constv  = *constv_ptr;
    char             *strpool = *strpool_ptr;
    mqi_cond_entry_t *cond;
    mqi_variable_t   *var;
    mqi_data_type_t   type;
    uint32_t          flags;
    value_t          *val;
    char             *str;
    int               len;
    int               i;

    for (i = 0;   i < ncond;   i++) {
        *(cond = dst_conds + i) = src_conds[i];

        if (cond->type == mqi_variable) {
            var   = &cond->u.variable;
            type  = var->type;
            flags = var->flags;

            if ((flags & MQL_BINDABLE))
                val = bindv + MQL_BIND_INDEX(flags);
            else {
                val = constv++;

                switch (type) {
                case mqi_varchar:
                    str = *(var->v.varchar);
                    len = strlen(str) + 1;
                    val->v.varchar = (char *)memcpy(strpool, str, len);
                    strpool += len;
                    break;
                case mqi_integer:
                    val->v.integer = *(var->v.integer);
                    break;
                case mqi_unsignd:
                    val->v.unsignd = *(var->v.unsignd);
                    break;
                case mqi_floating:
                    val->v.floating = *(var->v.floating);
                    break;
                default:
                    break;
                }
            }

            val->type = type;
            var->v.generic = (void *)&val->v.generic;
        }
    }

    *bindv_ptr   = bindv;
    *constv_ptr  = constv;
    *strpool_ptr = strpool;
}

static mql_result_t *exec_show_tables(mql_result_type_t type,
                                      shtable_statement_t *st)
{
    mql_result_t *rslt;
    char         *names[4096];
    int           n;

    MQI_UNUSED(type);

    if ((n = mqi_show_tables(st->flags, names, MQI_DIMENSION(names))) < 0) {
        rslt = mql_result_error_create(errno, "can't show tables: %s",
                                       strerror(errno));
    }
    else {
        if (!n)
            rslt = mql_result_error_create(0, "no tables");
        else
            rslt = mql_result_list_create(mqi_string, n, names);
    }

    return rslt;
}

static mql_result_t *exec_describe(mql_result_type_t type,
                                   describe_statement_t *d)
{
    mql_result_t     *rslt;
    mqi_column_def_t  defs[MQI_COLUMN_MAX];
    int               n;

    if ((n = mqi_describe(d->table, defs, MQI_COLUMN_MAX)) < 0) {
        rslt = mql_result_error_create(errno, "describe failed: %s",
                                       strerror(errno));
    }
    else {
        switch (type) {
        case mql_result_columns:
            rslt = mql_result_columns_create(n, defs);
            break;
        case mql_result_string:
            rslt = mql_result_string_create_column_list(n, defs);
            break;
        default:
            rslt = mql_result_error_create(EINVAL, "describe failed: invalid"
                                           " result type %d", type);
            break;
        }
    }

    return rslt;
}

static mql_result_t *exec_begin(transact_statement_t *b)
{
    mql_result_t *rslt;

    if (mql_begin_transaction(b->trnam) == 0)
        rslt = mql_result_success_create();
    else {
        rslt = mql_result_error_create(errno, "begin failed: %s",
                                       strerror(errno));
    }

    return rslt;
}


static mql_result_t *exec_commit(transact_statement_t *c)
{
    mql_result_t *rslt;

    if (mql_commit_transaction(c->trnam) == 0)
        rslt = mql_result_success_create();
    else {
        rslt = mql_result_error_create(errno, "commit failed: %s",
                                       strerror(errno));
    }

    return rslt;
}


static mql_result_t *exec_rollback(transact_statement_t *r)
{
    mql_result_t *rslt;

    if (mql_rollback_transaction(r->trnam) == 0)
        rslt = mql_result_success_create();
    else {
        rslt = mql_result_error_create(errno, "rollback failed: %s",
                                       strerror(errno));
    }

    return rslt;
}


static mql_result_t *exec_insert(insert_statement_t *i)
{
    mql_result_t *rslt;
    int           n;

    if ((n = mqi_insert_into(i->table, i->ignore, i->columns, i->rows)) >= 0)
        rslt = mql_result_error_create(0, "inserted %d rows", n);
    else {
        rslt = mql_result_error_create(errno, "insert error: %s",
                                       strerror(errno));
    }

    return rslt;
}

static mql_result_t *exec_update(update_statement_t *u)
{
    mql_result_t *rslt;
    int           n;

    if ((n = mqi_update(u->table, u->cond, u->columns, u->values)) >= 0)
        rslt = mql_result_error_create(0, "updated %d rows", n);
    else {
        rslt = mql_result_error_create(errno, "update error: %s",
                                       strerror(errno));
    }

    return rslt;
}

static mql_result_t *exec_delete(delete_statement_t *d)
{
    mql_result_t *rslt;
    int           n;

    if ((n = mqi_delete_from(d->table, d->cond)) >= 0)
        rslt = mql_result_error_create(0, "deleted %d rows", n);
    else {
        rslt = mql_result_error_create(errno, "delete error: %s",
                                       strerror(errno));
    }

    return rslt;
}

static mql_result_t *exec_select(mql_result_type_t type, select_statement_t *s)
{
    mql_result_t *rslt;
    int           maxrow;
    int           nrow;
    void         *rows;

    if ((maxrow = mqi_get_table_size(s->table)) < 0)
        rslt = mql_result_error_create(ENOENT, "can't access table");
    else {
        if (!maxrow) {
            rows = alloca(s->rowsize);
            nrow = 0;
        }
        else {
            rows = alloca(maxrow * s->rowsize);
            nrow = mqi_select(s->table, s->cond, s->columns,
                              rows, s->rowsize, maxrow);
        }

       if (nrow < 0) {
           rslt = mql_result_error_create(errno, "select error: %s",
                                          strerror(errno));
        }
        else {
            switch (type) {
            case mql_result_rows:
                rslt = mql_result_rows_create(s->ncolumn, s->columns,
                                              s->coltypes, s->colsizes,
                                              nrow, s->rowsize, rows);
                break;
            case mql_result_string:
                rslt = mql_result_string_create_row_list(
                                         s->ncolumn, s->colnames, s->columns,
                                         s->coltypes, s->colsizes,
                                         nrow, s->rowsize, rows);
                break;
            default:
                rslt = mql_result_error_create(EINVAL, "select failed: invalid"
                                               " result type %d", type);
                break;
            }
        }
    }

    return rslt;
}

static int bind_update_value(update_statement_t *u,
                             int                 idx,
                             mqi_data_type_t     type,
                             va_list             data)
{
    if (idx >= u->nbind) {
        errno = EINVAL;
        return -1;
    }

    return bind_value(u->values + idx, type, data);
}


static int bind_delete_value(delete_statement_t *d,
                             int                 idx,
                             mqi_data_type_t     type,
                             va_list             data)
{
    if (idx >= d->nbind) {
        errno = EINVAL;
        return -1;
    }

    return bind_value(d->values + idx, type, data);
}


static int bind_select_value(select_statement_t *s,
                             int                 idx,
                             mqi_data_type_t     type,
                             va_list             data)
{
    if (idx >= s->nbind) {
        errno = EINVAL;
        return -1;
    }

    return bind_value(s->values + idx, type, data);
}


static int bind_value(value_t *v, mqi_data_type_t type, va_list data)
{
    if (type == v->type) {
        switch (type) {
        case mqi_varchar:   v->v.varchar  = va_arg(data, char *);     return 0;
        case mqi_integer:   v->v.integer  = va_arg(data, int32_t);    return 0;
        case mqi_unsignd:   v->v.unsignd  = va_arg(data, uint32_t);   return 0;
        case mqi_floating:  v->v.floating = va_arg(data, double);     return 0;
        default:                                                      break;
        }
    }

    errno = EINVAL;
    return -1;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
