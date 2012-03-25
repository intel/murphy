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

typedef struct {
    int                   cindex;
    mqi_data_type_t       type;
    int                   offset;
} column_desc_t;

typedef struct {
    int                   code;
    char                  msg[0];
} error_desc_t;

typedef struct {
    mql_result_type_t     type;
    error_desc_t          error;
} result_error_t;

typedef struct {
    mql_result_type_t     type;
    int                   ncol;
    mqi_column_def_t      cols[0];
} result_columns_t;

typedef struct {
    mql_result_type_t     type;
    int                   rowsize;
    int                   ncol;
    int                   nrow;
    void                 *data;
    column_desc_t         cols[0];
} result_rows_t;

typedef struct {
    mql_result_type_t     type;
    char                  string[0];
} result_string_t;

typedef struct {
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
        };
    }                     value;
} result_list_t;

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

    if (l > sizeof(buf))
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

    MDB_CHECKARG(n >= 0 && n < MQI_DIMENSION(nlgh) && names, NULL);

    len = sizeof(result_string_t) + 1;

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
        
    if (!(rslt = calloc(1, len))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_string;

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


    /*
     * allocate and initialize the result structure
     */
    if (!(rslt = calloc(1, sizeof(result_string_t) + linlen * (ncol+2) + 1))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_string;
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
        len = sizeof(result_string_t) + rwidth * 2 + strlen(no_rows) + 1;
    else
        len = sizeof(result_string_t) + rwidth * (nrow + 2) + 1;

    /*
     * setup the result structure
     */
    if (!(rslt = calloc(1, len))) {
        errno = ENOMEM;
        return NULL;
    }

    rslt->type = mql_result_string;

    p = rslt->string;

    /*
     * labels
     */
    for (i = 0;   i < ncol;   i++) {
        if ((cwidth = cwidths[i])) {
            if (cwidth <= (len = strlen(colnams[i]))) {
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
    int     *slen;
    char    *strpool;
    int      poollen;
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
        poollen = 0;
        datalen = sizeof(int32_t) * length;
        break;
    case mqi_unsignd:
        poollen = 0;
        datalen = sizeof(uint32_t) * length;
        break;
    case mqi_floating:
        poollen = 0;
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
            memcpy(rslt->value.generic, values, datalen);
        else {
            strs = (char **)values;
            strpool = (char *)&rslt->value.varchar[length];
            for (i = 0;  i < length;  i++) {
                rslt->value.varchar[i] = memcpy(strpool, strs[i], slen[i]);
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
            v = rslt->value.varchar[idx];
        else {
            strncpy(v, rslt->value.varchar[idx], len);
            v[len-1] = '\0';
        }
        break;

    case mqi_integer:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%d", rslt->value.integer[idx]);
        break;

    case mqi_unsignd:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%u", rslt->value.unsignd[idx]);
        break;

    case mqi_floating:
        if (!v)
            v = "";
        else
            snprintf(v, len, "%lf", rslt->value.floating[idx]);
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
    case mqi_varchar:   v = strtol(rslt->value.varchar[idx], NULL, 10);  break;
    case mqi_integer:   v = rslt->value.integer[idx];                    break;
    case mqi_unsignd:   v = rslt->value.unsignd[idx];                    break;
    case mqi_floating:  v = rslt->value.floating[idx];                   break;
    default:            v = 0;                                           break;
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
    case mqi_varchar:   v = strtoul(rslt->value.varchar[idx], NULL, 10); break;
    case mqi_integer:   v = rslt->value.integer[idx];                    break;
    case mqi_unsignd:   v = rslt->value.unsignd[idx];                    break;
    case mqi_floating:  v = rslt->value.floating[idx];                   break;
    default:            v = 0;                                           break;
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
    case mqi_varchar:   v = strtod(rslt->value.varchar[idx], NULL);  break;
    case mqi_integer:   v = rslt->value.integer[idx];                break;
    case mqi_unsignd:   v = rslt->value.unsignd[idx];                break;
    case mqi_floating:  v = rslt->value.floating[idx];               break;
    default:            v = 0;                                       break;
    }

    return v;
}

void mql_result_free(mql_result_t *r)
{
    free(r);
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
