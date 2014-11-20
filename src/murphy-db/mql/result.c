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
#include <ctype.h>
#include <errno.h>

#include <murphy-db/assert.h>
#include <murphy-db/mql-result.h>
#include "mql-parser.h"

typedef struct column_desc_s           column_desc_t;
typedef struct error_desc_s            error_desc_t;
typedef struct result_error_s          result_error_t;
typedef struct result_event_s          result_event_t;
typedef struct result_event_colchg_s   result_event_colchg_t;
typedef struct result_event_rowchg_s   result_event_rowchg_t;
typedef struct result_event_table_s    result_event_table_t;
typedef struct result_event_transact_s result_event_transact_t;
typedef struct result_columns_s        result_columns_t;
typedef struct result_rows_s           result_rows_t;
typedef struct result_string_s         result_string_t;
typedef struct result_list_s           result_list_t;

struct column_desc_s {
    int                   cindex;
    mqi_data_type_t       type;
    int                   offset;
};

struct error_desc_s {
    int                   code;
    char                  msg[0];
};

struct result_error_s {
    mql_result_type_t     type;
    error_desc_t          error;
};

struct result_event_s {
    mql_result_type_t     type;
    mqi_event_type_t      event;
    uint8_t               data[0];
};

struct result_event_colchg_s {
    mql_result_type_t     type;
    mqi_event_type_t      event;
    mqi_handle_t          table;
    int                   column;
    mqi_change_value_t    value;
    mql_result_t         *select;
    uint8_t               data[0];
};

struct result_event_rowchg_s {
    mql_result_type_t     type;
    mqi_event_type_t      event;
    mqi_handle_t          table;
    mql_result_t         *select;
};

struct result_event_table_s {
    mql_result_type_t     type;
    mqi_event_type_t      event;
    mqi_handle_t          table;
};

struct result_event_transact_s {
    mql_result_type_t     type;
    mqi_event_type_t      event;
};

struct result_columns_s {
    mql_result_type_t     type;
    int                   ncol;
    mqi_column_def_t      cols[0];
};

struct result_rows_s {
    mql_result_type_t     type;
    int                   rowsize;
    int                   ncol;
    int                   nrow;
    void                 *data;
    column_desc_t         cols[0];
};

struct result_string_s {
    mql_result_type_t     type;
    int                   length;
    char                  string[0];
};

struct result_list_s {
    mql_result_type_t     type;
    int                   length;
    struct {
        mqi_data_type_t   type;
        union {
            char     *varchar[0];
            int32_t   integer[0];
            uint32_t  unsignd[0];
            double    floating[0];
            int       generic[0];
        } v;
    }                     value;
};

static inline mqi_data_type_t get_column_type(result_rows_t *, int);
static inline void *get_column_address(result_rows_t *, int, int);


int mql_result_is_success(mql_result_t *r)
{
    result_error_t *e = (result_error_t *)r;

    if (e) {
        if (e->type != mql_result_error)
            return 1;

        if (!e->error.code)
            return 1;
    }

    return 0;
}

mql_result_t *mql_result_success_create(void)
{
    return mql_result_error_create(0, "Success");
}

mql_result_t *mql_result_error_create(int code, const char *fmt, ...)
{
    va_list ap;
    result_error_t *rslt;
    char buf[1024];
    int l;

    MDB_CHECKARG(code >= 0 && fmt, NULL);

    va_start(ap, fmt);
    l = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (l > (int)sizeof(buf))
        l = sizeof(buf) - 1;

    if ((rslt = calloc(1, sizeof(result_error_t) + l + 1))) {
        rslt->type = mql_result_error;
        rslt->error.code = code;
        memcpy(rslt->error.msg, buf, l);
    }

    return (mql_result_t *)rslt;
}

int mql_result_error_get_code(mql_result_t *r)
{
    int code;

    MDB_CHECKARG(r, -1);

    if (r->type != mql_result_error)
        code = 0;
    else {
        result_error_t *rslt = (result_error_t *)r;
        code = rslt->error.code;
    }

    return code;
}

const char *mql_result_error_get_message(mql_result_t *r)
{
    const char *msg;

    MDB_CHECKARG(r, NULL);

    if (r->type != mql_result_error)
        msg = "Success";
    else {
        result_error_t *rslt = (result_error_t *)r;
        msg = rslt->error.msg;
    }

    return msg;
}

mqi_event_type_t mql_result_event_get_type(mql_result_t *r)
{
    result_event_t *ev;

    if (!r || r->type != mql_result_event)
        return mqi_event_unknown;

    ev = (result_event_t *) r;

    return ev->event;
}

mql_result_t *mql_result_event_get_changed_rows(mql_result_t *r)
{
    result_event_t *ev;
    result_event_rowchg_t *rowchg_ev;

    if (!r || r->type != mql_result_event)
        return NULL;

    ev = (result_event_t *) r;

    if (ev->event != mqi_row_deleted && ev->event != mqi_row_inserted)
        return NULL;

    rowchg_ev = (result_event_rowchg_t *) ev;

    return rowchg_ev->select;
}

mql_result_t *mql_result_event_column_change_create(mqi_handle_t        table,
                                                    int                 column,
                                                    mqi_change_value_t *value,
                                                    mql_result_t       *select)
{
    result_event_colchg_t *rslt;
    int                    osiz;
    int                    nsiz;
    int                    poollen;
    void                  *poolptr;
    size_t                 size;

    MDB_CHECKARG(table != MQI_HANDLE_INVALID &&
                 column >= 0 && column < MQI_COLUMN_MAX && value &&
                 (!select || (select && select->type == mql_result_rows)),
                 NULL);

    switch (value->type) {

    case mqi_varchar:
        osiz = strlen(value->old.varchar) + 1;
        nsiz = strlen(value->new_.varchar) + 1;
        break;

    case mqi_blob:
        if ((osiz = nsiz = mqi_get_column_size(table, column)) < 0)
            return NULL;
        break;

    default:
        osiz = nsiz = 0;
        break;
    }

    poollen = osiz + nsiz;
    size = sizeof(result_event_colchg_t) + poollen;

    if (!(rslt = calloc(1, size))) {
        errno = ENOMEM;
        return NULL;
    }

    poolptr = (void *)rslt->data;

    rslt->type   = mql_result_event;
    rslt->event  = mqi_column_changed;
    rslt->table  = table;
    rslt->column = column;
    rslt->value  = *value;
    rslt->select = select;

    if (osiz > 0) {
        rslt->value.old.generic = poolptr;
        memcpy(poolptr, value->old.generic, osiz);
        poolptr += osiz;
    }

    if (nsiz > 0) {
        rslt->value.new_.generic = poolptr;
        memcpy(poolptr, value->new_.generic, nsiz);
        poolptr += nsiz;
    }

    return (mql_result_t *)rslt;
}


mql_result_t *mql_result_event_row_change_create(mqi_event_type_t    event,
                                                 mqi_handle_t        table,
                                                 mql_result_t       *select)
{
    result_event_rowchg_t *rslt;

    MDB_CHECKARG((event == mqi_row_inserted || event == mqi_row_deleted) &&
                 table != MQI_HANDLE_INVALID &&
                 select && select->type == mql_result_rows, NULL);

    if (!(rslt = calloc(1, sizeof(result_event_rowchg_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type   = mql_result_event;
    rslt->event  = event;
    rslt->table  = table;
    rslt->select = select;

    return (mql_result_t *)rslt;
}


mql_result_t *mql_result_event_table_create(mqi_event_type_t  event,
                                            mqi_handle_t      table)
{
    result_event_table_t *rslt;

    MDB_CHECKARG((event == mqi_table_created || event == mqi_table_dropped) &&
                 table != MQI_HANDLE_INVALID, NULL);

    if (!(rslt = calloc(1, sizeof(result_event_table_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type   = mql_result_event;
    rslt->event  = event;
    rslt->table  = table;

    return (mql_result_t *)rslt;
}


mql_result_t *mql_result_event_transaction_create(mqi_event_type_t event)
{
    result_event_transact_t *rslt;

    MDB_CHECKARG(event == mqi_transaction_start ||
                 event == mqi_transaction_end, NULL);

    if (!(rslt = calloc(1, sizeof(result_event_transact_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type  = mql_result_event;
    rslt->event = event;

    return (mql_result_t *)rslt;
}



mql_result_t *mql_result_columns_create(int ncol, mqi_column_def_t *defs)
{
    result_columns_t *rslt;
    mqi_column_def_t *col;
    size_t            poollen;
    size_t            dlgh;
    int               namlen[MQI_COLUMN_MAX];
    char             *strpool;
    int               i;

    MDB_CHECKARG(ncol > 0 && ncol < MQI_COLUMN_MAX && defs, NULL);

    for (poollen = 0, i = 0;  i < ncol;  i++)
        poollen += (namlen[i] = strlen(defs[i].name) + 1);

    dlgh = sizeof(mqi_column_def_t) * ncol;

    if (!(rslt = calloc(1, sizeof(result_columns_t) + dlgh + poollen))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_columns;
    rslt->ncol = ncol;

    memcpy(rslt->cols, defs, dlgh);

    strpool = (char *)(rslt->cols + ncol);

    for (i = 0;    i < ncol;    i++) {
        col = rslt->cols + i;
        col->name = memcpy(strpool, col->name, namlen[i]);
        strpool += namlen[i];
    }

    return (mql_result_t *)rslt;
}

int mql_result_columns_get_column_count(mql_result_t *r)
{
    result_columns_t *rslt = (result_columns_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_columns, -1);

    return rslt->ncol;
}

const char *mql_result_columns_get_name(mql_result_t *r, int colidx)
{
    result_columns_t *rslt = (result_columns_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_columns &&
                 colidx >= 0 && colidx < rslt->ncol, NULL);

    return rslt->cols[colidx].name;
}

mqi_data_type_t mql_result_columns_get_type(mql_result_t *r, int colidx)
{
    result_columns_t *rslt = (result_columns_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_columns &&
                 colidx >= 0 && colidx < rslt->ncol, -1);

    return rslt->cols[colidx].type;
}

int mql_result_columns_get_length(mql_result_t *r, int colidx)
{
    result_columns_t *rslt = (result_columns_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_columns &&
                 colidx >= 0 && colidx < rslt->ncol, -1);

    return rslt->cols[colidx].length;
}


uint32_t mql_result_columns_get_flags(mql_result_t *r, int colidx)
{
    result_columns_t *rslt = (result_columns_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_columns &&
                 colidx >= 0 && colidx < rslt->ncol, -1);

    return rslt->cols[colidx].flags;
}


mql_result_t *mql_result_rows_create(int                ncol,
                                     mqi_column_desc_t *coldescs,
                                     mqi_data_type_t   *coltypes,
                                     int               *colsizes,
                                     int                nrow,
                                     int                rowsize,
                                     void              *rows)
{
    result_rows_t     *rslt;
    column_desc_t     *col;
    mqi_column_desc_t *cd;
    int                offs;
    size_t             size;
    size_t             dlgh;
    int                i;

    MDB_CHECKARG(ncol >  0 && coldescs && coltypes && colsizes &&
                 nrow >= 0 && rowsize > 0 && rows, NULL);

    offs = sizeof(column_desc_t) * ncol;
    dlgh = rowsize * nrow;
    size = sizeof(result_rows_t) + offs + dlgh;


    if (!(rslt = calloc(1, size))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type    = mql_result_rows;
    rslt->rowsize = rowsize;
    rslt->ncol    = ncol;
    rslt->nrow    = nrow;
    rslt->data    = rslt->cols + ncol;

    for (i = 0;   i < ncol;  i++) {
        col = rslt->cols + i;
        cd  = coldescs + i;

        col->cindex = cd->cindex;
        col->type   = coltypes[i];
        col->offset = cd->offset;
    }

    if (dlgh > 0)
        memcpy(rslt->data, rows, dlgh);

    return (mql_result_t *)rslt;
}


int mql_result_rows_get_row_column_count(mql_result_t *r)
{
    result_rows_t *rslt = (result_rows_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows, -1);

    return rslt->ncol;
}

mqi_data_type_t mql_result_rows_get_row_column_type(mql_result_t *r, int colidx)
{
    result_rows_t *rslt = (result_rows_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 rslt->ncol > colidx, -1);

    return rslt->cols[colidx].type;
}

int mql_result_rows_get_row_column_index(mql_result_t *r, int colidx)
{
    result_rows_t *rslt = (result_rows_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 rslt->ncol > colidx, -1);

    return rslt->cols[colidx].cindex;
}

int mql_result_rows_get_row_count(mql_result_t *r)
{
    result_rows_t *rslt = (result_rows_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows, -1);

    return rslt->nrow;
}

const char *mql_result_rows_get_string(mql_result_t *r, int colidx, int rowidx,
                                       char *buf, int len)
{
    result_rows_t *rslt = (result_rows_t *)r;
    void *addr;
    char *v = NULL;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 colidx >= 0 && colidx < rslt->ncol &&
                 rowidx >= 0 && rowidx < rslt->nrow &&
                 (!buf || (buf && len > 0)), NULL);

    if ((v = buf))
        *v = '\0';

    if ((addr = get_column_address(rslt, colidx, rowidx))) {
        switch (get_column_type(rslt, colidx)) {
        case mqi_varchar:
            if (!v)
                v = *(char **)addr;
            else {
                strncpy(v, *(char **)addr, len);
                v[len-1] = '\0';
            }
            break;

        case mqi_integer:
            if (!v)
                v = "";
            else
                snprintf(v, len, "%d", *(int32_t *)addr);
            break;

        case mqi_unsignd:
            if (!v)
                v = "";
            else
                snprintf(v, len, "%u", *(uint32_t *)addr);
            break;

        case mqi_floating:
            if (!v)
                v = "";
            else
                snprintf(v, len, "%lf", *(double *)addr);
            break;

        default:
            v = "";
            break;
        }
    }

    return v;
}

int32_t mql_result_rows_get_integer(mql_result_t *r, int colidx, int rowidx)
{
    result_rows_t *rslt = (result_rows_t *)r;
    void *addr;
    int32_t v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 colidx >= 0 && colidx < rslt->ncol &&
                 rowidx >= 0 && rowidx < rslt->nrow, 0);

    if ((addr = get_column_address(rslt, colidx, rowidx))) {
        switch (get_column_type(rslt, colidx)) {
        case mqi_varchar:    v = strtol(*(char **)addr, NULL, 10);   break;
        case mqi_integer:    v = *(int32_t *)addr;                   break;
        case mqi_unsignd:    v = *(uint32_t *)addr;                  break;
        case mqi_floating:   v = *(double *)addr;                    break;
        default:             v = 0;                                  break;
        }
    }

    return v;
}

uint32_t mql_result_rows_get_unsigned(mql_result_t *r, int colidx, int rowidx)
{
    result_rows_t *rslt = (result_rows_t *)r;
    void *addr;
    uint32_t v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 colidx >= 0 && colidx < rslt->ncol &&
                 rowidx >= 0 && rowidx < rslt->nrow, 0);

    if ((addr = get_column_address(rslt, colidx, rowidx))) {
        switch (get_column_type(rslt, colidx)) {
        case mqi_varchar:    v = strtoul(*(char **)addr, NULL, 10);  break;
        case mqi_integer:    v = *(int32_t *)addr;                   break;
        case mqi_unsignd:    v = *(uint32_t *)addr;                  break;
        case mqi_floating:   v = *(double *)addr;                    break;
        default:             v = 0;                                  break;
        }
    }

    return v;
}

double mql_result_rows_get_floating(mql_result_t *r, int colidx, int rowidx)
{
    result_rows_t *rslt = (result_rows_t *)r;
    void *addr;
    double v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_rows &&
                 colidx >= 0 && colidx < rslt->ncol &&
                 rowidx >= 0 && rowidx < rslt->nrow, 0.0);

    if ((addr = get_column_address(rslt, colidx, rowidx))) {
        switch (get_column_type(rslt, colidx)) {
        case mqi_varchar:    v = strtod(*(char **)addr, NULL);    break;
        case mqi_integer:    v = *(int32_t *)addr;                break;
        case mqi_unsignd:    v = *(uint32_t *)addr;               break;
        case mqi_floating:   v = *(double *)addr;                 break;
        default:             v = 0;                               break;
        }
    }

    return v;
}


mql_result_t *mql_result_string_create_table_list(int n, char **names)
{
    static const char *no_tables = "no tables\n";
    result_string_t *rslt;
    int    nlgh[4096];
    char  *name;
    char   first_letter, upper;
    int    len;
    char  *p;
    int    i;

    MDB_CHECKARG(n >= 0 && n < (int)MQI_DIMENSION(nlgh) && names, NULL);

    len = 1;  /* zero terminator */

    if (!n)
        len += strlen(no_tables) + 1;
    else {
        for (i = 0, first_letter = 0;    i < n;    i++) {
            name  = names[i];
            upper = toupper(name[0]);

            if (upper != first_letter) {
                first_letter = upper;
                len += 3; /* \n[A-Z]: */
            }

            len += (nlgh[i] = strlen(name)) + 1;
        }
    }

    if (!(rslt = calloc(1, sizeof(result_string_t) + len))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_string;
    rslt->length = len;

    if (!n)
        strcpy(rslt->string, no_tables);
    else {
        for (p = rslt->string, first_letter = 0, i = 0;     i < n;     i++) {
            name  = names[i];
            len   = nlgh[i];
            upper = toupper(name[0]);

            if (upper != first_letter) {
                if (first_letter)
                    *p++ = '\n';

                first_letter = upper;

                *p++ = upper;
                *p++ = ':';
            }

            *p++ = ' ';

            memcpy(p, name, len);
            p += len;
        }

        *p++ = '\n';
    }

    return (mql_result_t *)rslt;
}

mql_result_t *mql_result_string_create_column_change(const char         *table,
                                                     const char         *col,
                                                     mqi_change_value_t *value,
                                                     mql_result_t       *rsel)
{
#define EVENT   0
#define TABLE   1
#define COLUMN  2
#define VALUE   3
#define FLDS    4

    static const char *hstr[FLDS]  = {" event", "table" , "column", "change"};
    static int         hlen[FLDS]  = {    6   ,    5    ,     6   ,     6   };


    result_string_t *rs = (result_string_t *)rsel;
    result_string_t *rslt;

    char             buf[1024];
    int              l;
    int              len[FLDS];
    const char      *cstr[FLDS];
    int              cw[FLDS];
    int              linlen;
    size_t           esiz;
    size_t           ssiz;
    size_t           size;
    char            *p;
    int              i;

    MDB_CHECKARG(table && col && value &&
                 (!rsel || (rsel && rsel->type == mql_result_string)), NULL);

    cstr[EVENT]  = "'column changed'";
    cstr[TABLE]  = table;
    cstr[COLUMN] = col;
    cstr[VALUE]  = buf;

    for (i = 0;  i < VALUE;  i++)
        len[i] = strlen(cstr[i]);

#define PRINT_VALUE(fmt,t) \
    snprintf(buf, sizeof(buf), fmt " => " fmt, value->old.t, value->new_.t)
#define PRINT_UNKNOWN \
    snprintf(buf, sizeof(buf), "<unknown> => <unknown>");

    switch (value->type) {
    case mqi_varchar:    l = PRINT_VALUE("'%s'" , varchar );    break;
    case mqi_integer:    l = PRINT_VALUE("%d"   , integer );    break;
    case mqi_unsignd:    l = PRINT_VALUE("%u"   , unsignd );    break;
    case mqi_floating:   l = PRINT_VALUE("%.2lf", floating);    break;
    default:             l = PRINT_UNKNOWN;                     break;
    }

#undef PRINT

    len[VALUE] = l;


    for (linlen = (FLDS-1) * 2 + 1,  i = 0;   i < FLDS;   i++)
        linlen += (cw[i] = len[i] > hlen[i] ?  len[i] : hlen[i]);

    esiz = linlen * 3;
    ssiz = rs ? rs->length : 0;
    size = esiz + ssiz + 2;

    if (!(rslt = calloc(1, sizeof(result_string_t) + size))) {
        errno = ENOMEM;
        return NULL;
    }

    p = rslt->string;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i],  hstr[i], i == FLDS-1 ? "\n" : "  ");

    memset(p, '-', linlen-1);
    p[linlen-1] = '\n';
    p += linlen;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i], cstr[i], i == FLDS-1 ? "\n" : "  ");


    if (ssiz > 0) {
        *p++ = '\n';
        memcpy(p, rs->string, ssiz);
    }

    rslt->type = mql_result_string;
    rslt->length = p - rslt->string;

    return (mql_result_t *)rslt;

#undef FLDS
#undef VALUE
#undef COLUMN
#undef TABLE
#undef EVENT
}

mql_result_t *mql_result_string_create_row_change(mqi_event_type_t    event,
                                                  const char         *table,
                                                  mql_result_t       *rsel)
{
#define EVENT   0
#define TABLE   1
#define FLDS    2

    static const char *hstr[FLDS]  = {" event", "table"};
    static int         hlen[FLDS]  = {    6   ,    5   };


    result_string_t *rs = (result_string_t *)rsel;
    result_string_t *rslt;

    int              len[FLDS];
    const char      *cstr[FLDS];
    int              cw[FLDS];
    int              linlen;
    size_t           esiz;
    size_t           ssiz;
    size_t           size;
    char            *p;
    int              i;

    MDB_CHECKARG((event == mqi_row_inserted || event == mqi_row_deleted) &&
                 table && rsel && rsel->type == mql_result_string, NULL);

    cstr[EVENT] = (event==mqi_row_inserted) ? "'row inserted'":"'row deleted'";
    cstr[TABLE] = table;

    for (i = 0;  i < FLDS;  i++)
        len[i] = strlen(cstr[i]);

    for (linlen = (FLDS-1) * 2 + 1,  i = 0;   i < FLDS;   i++)
        linlen += (cw[i] = len[i] > hlen[i] ?  len[i] : hlen[i]);

    esiz = linlen * 3;
    ssiz = rs ? rs->length : 0;
    size = esiz + ssiz + 2;

    if (!(rslt = calloc(1, sizeof(result_string_t) + size))) {
        errno = ENOMEM;
        return NULL;
    }

    p = rslt->string;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i],  hstr[i], i == FLDS-1 ? "\n" : "  ");

    memset(p, '-', linlen-1);
    p[linlen-1] = '\n';
    p += linlen;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i], cstr[i], i == FLDS-1 ? "\n" : "  ");

    *p++ = '\n';
    memcpy(p, rs->string, ssiz);

    rslt->type = mql_result_string;
    rslt->length = p - rslt->string;

    return (mql_result_t *)rslt;

#undef FLDS
#undef TABLE
#undef EVENT
}

mql_result_t *mql_result_string_create_table_change(mqi_event_type_t    event,
                                                    const char         *table)
{
#define EVENT   0
#define TABLE   1
#define FLDS    2

    static const char *hstr[FLDS]  = {" event", "table"};
    static int         hlen[FLDS]  = {    6   ,    5   };

    result_string_t *rslt;

    int              len[FLDS];
    const char      *cstr[FLDS];
    int              cw[FLDS];
    int              linlen;
    char            *p;
    int              i;

    MDB_CHECKARG((event == mqi_table_created || event == mqi_table_dropped) &&
                 table, NULL);

    cstr[EVENT] = (event == mqi_table_created) ?
                  "'table created'" : "'table dropped'";
    cstr[TABLE] = table;

    for (i = 0;  i < FLDS;  i++)
        len[i] = strlen(cstr[i]);

    for (linlen = (FLDS-1) * 2 + 1,  i = 0;   i < FLDS;   i++)
        linlen += (cw[i] = len[i] > hlen[i] ?  len[i] : hlen[i]);


    if (!(rslt = calloc(1, sizeof(result_string_t) + (linlen*3 + 1)))) {
        errno = ENOMEM;
        return NULL;
    }

    p = rslt->string;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i],  hstr[i], i == FLDS-1 ? "\n" : "  ");

    memset(p, '-', linlen-1);
    p[linlen-1] = '\n';
    p += linlen;

    for (i = 0;  i < FLDS;  i++)
        p += sprintf(p, "%-*s%s", cw[i], cstr[i], i == FLDS-1 ? "\n" : "  ");

    rslt->type = mql_result_string;
    rslt->length = p - rslt->string;

    return (mql_result_t *)rslt;

#undef FLDS
#undef TABLE
#undef EVENT
}

mql_result_t *mql_result_string_create_transaction_change(mqi_event_type_t evt)
{
    static const char *hstr  = " event";
    static int         hlen  = 6;

    result_string_t *rslt;

    int         len;
    const char *cstr;
    int         cw;
    int         linlen;
    char       *p;

    MDB_CHECKARG(evt == mqi_transaction_start || evt == mqi_transaction_end,
                 NULL);

    cstr   = (evt == mqi_transaction_start) ?
             "'transaction started'" : "'transaction ended'";
    len    = strlen(cstr);
    cw     = len > hlen ? len : hlen;
    linlen = cw + 1;

    if (!(rslt = calloc(1, sizeof(result_string_t) + (linlen*3 + 1)))) {
        errno = ENOMEM;
        return NULL;
    }

    p  = rslt->string;
    p += sprintf(p, "%-*s\n", cw,  hstr);

    memset(p, '-', cw);
    p[cw] = '\n';
    p += linlen;

    p += sprintf(p, "%-*s", cw, cstr);

    rslt->type = mql_result_string;
    rslt->length = p - rslt->string;

    return (mql_result_t *)rslt;
}

mql_result_t *mql_result_string_create_column_list(int               ncol,
                                                   mqi_column_def_t *defs)
{
#define INDEX   0
#define NAME    1
#define TYPE    2
#define LENGTH  3
#define FLDS    4

    static const char *hstr[FLDS]  = {"index", " name" , "type", "length"};
    static int         hlen[FLDS]  = {   5   ,     5   ,    4  ,     6   };
    static int         align[FLDS] = {  +1   ,    -1   ,   -1  ,    +1   };

    result_string_t  *rslt;
    mqi_column_def_t *def;
    const char       *typstr[MQI_COLUMN_MAX];
    int               namlen[MQI_COLUMN_MAX];
    int               typlen[MQI_COLUMN_MAX];
    int               fldlen[FLDS];
    int               linlen;
    int               len;
    int               offs;
    char             *p, *q;
    int               i, j;

    MDB_CHECKARG(ncol > 0 && ncol < MQI_COLUMN_MAX && defs, NULL);

    memcpy(fldlen, hlen, sizeof(fldlen));

    for (i = 0;  i < ncol;  i++) {
        def = defs + i;

        typstr[i] = mqi_data_type_str(def->type);

        if ((len = (namlen[i] = strlen(def->name)) + 1) > fldlen[NAME])
            fldlen[NAME] = len;

        if ((len = (typlen[i] = strlen(typstr[i]))) > fldlen[TYPE])
            fldlen[TYPE] = len;
    }

    for (linlen = FLDS, i = 0;  i < FLDS;  linlen += fldlen[i++])
        ;

    len = linlen * (ncol+2) + 1;

    /*
     * allocate and initialize the result structure
     */
    if (!(rslt = calloc(1, sizeof(result_string_t) + len))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_string;
    rslt->length = len;

    p = rslt->string;
    memset(p, ' ', linlen * (ncol+2));

    /*
     * labels
     */
    for (q = p, j = 0;   j < FLDS;   q += (fldlen[j++] + 1)) {
        offs = (align[j] < 0) ? 0 : fldlen[j] - hlen[j];
        memcpy(q + offs, hstr[j], hlen[j]);
    }

    p += linlen;
    *(p-1) = '\n';

    /*
     * separator line
     */
    memset(p, '-', linlen);
    p += linlen;
    *(p-1) = '\n';


    /*
     * data lines
     */
    for (i = 0;  i < ncol;  i++, p += linlen) {
        def = defs + i;
        snprintf(p, linlen+1, "%*d %s%-*s %-*s %*d\n",
                 fldlen[INDEX], i,
                 def->flags&MQI_COLUMN_KEY ? "*":" ", fldlen[NAME]-1,def->name,
                 fldlen[TYPE], typstr[i],
                 fldlen[LENGTH], def->length);
    }

    return (mql_result_t *)rslt;

#undef FLDS
#undef LENGTH
#undef TYPE
#undef NAME
#undef INDEX
}

mql_result_t *mql_result_string_create_row_list(int                 ncol,
                                                char              **colnams,
                                                mqi_column_desc_t  *coldescs,
                                                mqi_data_type_t    *coltypes,
                                                int                *colsizes,
                                                int                 nrow,
                                                int                 rowsize,
                                                void               *rows)
{
    static const char *no_rows = "no rows\n";

    result_string_t *rslt;
    size_t  len;
    int     rwidth;
    int     cwidth;
    int     cwidths[MQI_COLUMN_MAX + 1];
    void   *row;
    void   *column;
    int     i,j;
    char   *p;


    MDB_CHECKARG(ncol >  0 && coldescs && coltypes && colsizes &&
                 nrow >= 0 && rowsize > 0 && rows, NULL);

    /*
     * calculate column widths and row width
     */
    rwidth = ncol;  /* for each column a separating space or a \n at the end */

    for (i = 0;   i < ncol;   i++) {
        switch (coltypes[i]) {
        case mqi_varchar:   cwidth = colsizes[i] - 1;  break;
        case mqi_integer:   cwidth = 11;               break;
        case mqi_unsignd:   cwidth = 10;               break;
        case mqi_floating:  cwidth = 10;               break;
        default:            cwidth = 0;                break;
        }

        rwidth += (cwidths[i] = cwidth);
    }

    if (!nrow)
        len = rwidth * 2 + strlen(no_rows) + 1;
    else
        len = rwidth * (nrow + 2) + 1;

    /*
     * setup the result structure
     */
    if (!(rslt = calloc(1, sizeof(result_string_t) + len))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type   = mql_result_string;
    rslt->length = len;

    p = rslt->string;

    /*
     * labels
     */
    for (i = 0;   i < ncol;   i++) {
        if ((cwidth = cwidths[i])) {
            if (cwidth <= (int)(len = strlen(colnams[i]))) {
                /* truncate */
                memcpy(p, colnams[i], cwidth);
                p[cwidth] = ' ';
            }
            else {
                if (coltypes[i] == mqi_varchar) {
                    /* left align */
                    memcpy(p, colnams[i], len);
                    memset(p + len, ' ', (cwidth + 1) - len);
                }
                else {
                    /* right align */
                    memset(p, ' ', cwidth - len);
                    memcpy(p + (cwidth - len), colnams[i], len);
                    p[cwidth] = ' ';
                }
            }
            p += cwidth + 1;
        }
    } /* for */

    *(p-1) = '\n';

    /*
     * separator line
     */
    memset(p, '-', rwidth-1);
    p[rwidth-1] = '\n';
    p += rwidth;

    /*
     * data lines
     */
#define SPRINT(t,f) snprintf(p, cwidth+2, f " ", cwidth, *(t *)column)

    if (!nrow)
        strcpy(p, no_rows);
    else {
        for (i = 0, row = rows;  i < nrow;  i++, row += rowsize) {
            for (j = 0;  j < ncol; j++) {
                column = row + coldescs[j].offset;
                cwidth = cwidths[j];

                switch (coltypes[j]) {
                case mqi_varchar:     SPRINT( char *  , "%-*s"   );     break;
                case mqi_integer:     SPRINT( int32_t , "%*d"    );     break;
                case mqi_unsignd:     SPRINT( uint32_t, "%*u"    );     break;
                case mqi_floating:    SPRINT( double  , "%*.2lf" );     break;
                default:              memset(p, ' ', cwidth+1    );     break;
                }

                p += cwidth+1;
            }

            *(p-1) = '\n';
        }
        *p = '\0';
    }

#undef SPRINT

    return (mql_result_t *)rslt;
}

const char *mql_result_string_get(mql_result_t *r)
{
    result_string_t *rslt = (result_string_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_string, "");

    return rslt->string;
}


mql_result_t *mql_result_list_create(mqi_data_type_t type,
                                     int             length,
                                     void           *values)
{
    result_list_t *rslt;
    size_t   datalen;
    char   **strs;
    int     *slen = NULL;
    char    *strpool;
    int      poollen = 0;
    int      i;

    MDB_CHECKARG(length > 0 && values, NULL);

    switch (type) {
    case mqi_varchar:
        slen = alloca(sizeof(int) * length);
        for (strs = (char **)values, i = 0;  i < length;  i++)
            poollen += (slen[i] = strlen(strs[i]) + 1);
        datalen = sizeof(char *) * length + poollen;
        break;
    case mqi_integer:
        datalen = sizeof(int32_t) * length;
        break;
    case mqi_unsignd:
        datalen = sizeof(uint32_t) * length;
        break;
    case mqi_floating:
        datalen = sizeof(double) * length;
        break;
    default:
        errno = EINVAL;
        return NULL;
    }

    if ((rslt = calloc(1, sizeof(result_list_t) + datalen))) {
        rslt->type   = mql_result_list;
        rslt->length = length;
        rslt->value.type = type;

        if (type != mqi_varchar)
            memcpy(rslt->value.v.generic, values, datalen);
        else {
            strs = (char **)values;
            strpool = (char *)&rslt->value.v.varchar[length];
            for (i = 0;  i < length;  i++) {
                rslt->value.v.varchar[i] = memcpy(strpool, strs[i], slen[i]);
                strpool += slen[i];
            }
        }
    }

    return (mql_result_t *)rslt;
}

int mql_result_list_get_length(mql_result_t *r)
{
    result_list_t *rslt = (result_list_t *)r;

    MDB_CHECKARG(rslt && rslt->type == mql_result_list, -1);

    return rslt->length;
}

const char *mql_result_list_get_string(mql_result_t *r, int idx,
                                       char *buf, int len)
{
    result_list_t *rslt = (result_list_t *)r;
    char *v = NULL;

    MDB_CHECKARG(rslt && rslt->type == mql_result_list &&
                 idx >= 0 && idx < rslt->length, NULL);

    if ((v = buf))
        *v = '\0';

    switch (rslt->value.type) {

    case mqi_varchar:
        if (!v)
            v = rslt->value.v.varchar[idx];
        else {
            strncpy(v, rslt->value.v.varchar[idx], len);
            v[len-1] = '\0';
        }
        break;

    case mqi_integer:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%d", rslt->value.v.integer[idx]);
        break;

    case mqi_unsignd:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%u", rslt->value.v.unsignd[idx]);
        break;

    case mqi_floating:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%lf", rslt->value.v.floating[idx]);
        break;

    default:
        v = "";
        break;
    }

    return v;
}

int32_t mql_result_list_get_integer(mql_result_t *r, int idx)
{
    result_list_t *rslt = (result_list_t *)r;
    int32_t v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_list &&
                 idx >= 0 && idx < rslt->length, 0);

    switch (rslt->value.type) {
    case mqi_varchar:  v = strtol(rslt->value.v.varchar[idx], NULL, 10); break;
    case mqi_integer:  v = rslt->value.v.integer[idx];                   break;
    case mqi_unsignd:  v = rslt->value.v.unsignd[idx];                   break;
    case mqi_floating: v = rslt->value.v.floating[idx];                  break;
    default:           v = 0;                                            break;
    }

    return v;
}

int32_t mql_result_list_get_unsigned(mql_result_t *r, int idx)
{
    result_list_t *rslt = (result_list_t *)r;
    uint32_t v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_list &&
                 idx >= 0 && idx < rslt->length, 0);

    switch (rslt->value.type) {
    case mqi_varchar:  v = strtoul(rslt->value.v.varchar[idx], NULL, 10);break;
    case mqi_integer:  v = rslt->value.v.integer[idx];                   break;
    case mqi_unsignd:  v = rslt->value.v.unsignd[idx];                   break;
    case mqi_floating: v = rslt->value.v.floating[idx];                  break;
    default:           v = 0;                                            break;
    }

    return v;
}

double mql_result_list_get_floating(mql_result_t *r, int idx)
{
    result_list_t *rslt = (result_list_t *)r;
    double v = 0;

    MDB_CHECKARG(rslt && rslt->type == mql_result_list &&
                 idx >= 0 && idx < rslt->length, 0);

    switch (rslt->value.type) {
    case mqi_varchar:   v = strtod(rslt->value.v.varchar[idx], NULL);  break;
    case mqi_integer:   v = rslt->value.v.integer[idx];                break;
    case mqi_unsignd:   v = rslt->value.v.unsignd[idx];                break;
    case mqi_floating:  v = rslt->value.v.floating[idx];               break;
    default:            v = 0;                                         break;
    }

    return v;
}

void mql_result_free(mql_result_t *r)
{
    result_event_colchg_t *colchg = (result_event_colchg_t *)r;
    mql_result_t          *select;

    if (r) {
        if (r->type == mql_result_event) {
            if (colchg->event == mqi_column_changed) {
                select = colchg->select;

                if (select && select->type == mql_result_rows)
                    mql_result_free(colchg->select);
            }
        }

        free(r);
    }
}


static mqi_data_type_t get_column_type(result_rows_t *rslt, int cx)
{
    return rslt->cols[cx].type;
}

static void *get_column_address(result_rows_t *rslt, int cx, int rx)
{
    return rslt->data + (rslt->rowsize * rx + rslt->cols[cx].offset);
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
