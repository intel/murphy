#include <errno.h>

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>

#include <murphy-db/mqi.h>

#include "table.h"


/*
 * client-side tables, common table routines
 */

static void purge_pep_table(mrp_pep_table_t *t)
{
    int i;

    if (t != NULL) {
        mrp_free((char *)t->name);

        for (i = 0; i < t->ncolumn; i++)
            mrp_free((char *)t->columns[i].name);

        mrp_free(t->columns);
    }
}


static void free_column_definitions(mqi_column_def_t *columns, int ncolumn)
{
    int i;

    if (columns != NULL) {
        for (i = 0; i < ncolumn; i++)
            mrp_free((char *)columns[i].name);

        mrp_free(columns);
    }
}


static int copy_column_definitions(mqi_column_def_t *src, int nsrc,
                                   mqi_column_def_t **dstcol, int *ndst)
{
    mqi_column_def_t *dst;
    int               n, i;

    if (nsrc > 0) {
        if (src[nsrc - 1].name == NULL)
            n = nsrc - 1;
        else
            n = nsrc;

        dst = mrp_allocz_array(mqi_column_def_t, n + 1);

        if (dst == NULL)
            return FALSE;

        for (i = 0; i < n; i++) {
            dst[i].type   = src[i].type;
            dst[i].length = src[i].length;
            dst[i].name   = mrp_strdup(src[i].name);

            if (dst[i].name == NULL)
                goto fail;
        }

        *dstcol = dst;
        *ndst   = n;

        return TRUE;
    }
    else {
        *dstcol = NULL;
        *ndst   = 0;

        return FALSE;
    }

 fail:
    free_column_definitions(dst, n);
    return FALSE;
}


static void free_column_descriptors(mqi_column_desc_t *coldesc)
{
    mrp_free(coldesc);
}


static int setup_column_descriptors(mqi_column_def_t *columns, int ncolumn,
                                    mqi_column_desc_t **coldesc)
{
#define SETUP_TYPE(type, member)                                        \
                case mqi_##type:                                        \
                    desc->cindex = i;                                   \
                    desc->offset = (void *)&col->member - (void *)NULL; \
                    break;

    mqi_column_def_t  *def;
    mqi_column_desc_t *desc;
    mrp_pep_value_t   *col;
    int                i;

    *coldesc = mrp_allocz_array(mqi_column_desc_t, ncolumn + 1);

    if (coldesc != NULL) {
        def  = columns;
        desc = *coldesc;
        col  = NULL;

        for (i = 0; i < ncolumn; i++) {
            switch (def->type) {
                SETUP_TYPE(integer , s32);
                SETUP_TYPE(unsignd , u32);
                SETUP_TYPE(floating, dbl);
                SETUP_TYPE(string  , str);

            default:
            case mqi_blob:
                goto fail;
            }

            def++;
            desc++;
            col++;
        }

        desc->cindex = -1;
        desc->offset = 1;

        return TRUE;
    }

 fail:
    free_column_descriptors(*coldesc);
    *coldesc = NULL;

    return FALSE;

#undef SETUP_TYPE
}


static int check_columns(mqi_column_def_t *p, int np,
                         mqi_column_def_t *q, int nq)
{
    int i;

    if  (np == nq) {
        for (i = 0; i < np; i++, p++, q++) {
            if (p->type != q->type || p->length != q->length)
                return FALSE;
            if (strcmp(p->name, q->name))
                return FALSE;
        }

        return TRUE;
    }
    else
        return FALSE;
}


int copy_pep_table(mrp_pep_table_t *src, mrp_pep_table_t *dst)
{
    int i, ncolumn;

    dst->name = mrp_strdup(src->name);
    if (dst->name == NULL)
        return FALSE;

    if (src->columns[src->ncolumn - 1].name != NULL)
        ncolumn = src->ncolumn;
    else
        ncolumn = src->ncolumn - 1;

    dst->columns = mrp_allocz_array(typeof(*dst->columns), ncolumn + 1);
    if (dst->columns == NULL) {
        mrp_free((char *)dst->name);

        return FALSE;
    }

    dst->ncolumn = ncolumn;
    dst->idx_col = -1;

    for (i = 0; i < ncolumn; i++) {
        dst->columns[i].type   = src->columns[i].type;
        dst->columns[i].length = src->columns[i].length;
        dst->columns[i].name   = mrp_strdup(src->columns[i].name);

        if (dst->columns[i].name == NULL)
            goto fail;

        if (src->columns[i].flags != 0) {
            if (dst->idx_col == -1)
                dst->idx_col = i;
            else
                goto fail;
        }

        dst->columns[i].flags = 0;
    }

    return TRUE;

 fail:
    purge_pep_table(dst);

    return FALSE;
}


int copy_pep_tables(mrp_pep_table_t *src, mrp_pep_table_t *dst, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (!copy_pep_table(src + i, dst + i)) {
            for (i--; i >= 0; i--)
                purge_pep_table(dst + i);

            return FALSE;
        }
    }

    return TRUE;
}


void free_pep_table(mrp_pep_table_t *t)
{
    purge_pep_table(t);
    mrp_free(t);
}


void free_pep_tables(mrp_pep_table_t *tables, int n)
{
    int i;

    for (i = 0; i < n; i++)
        purge_pep_table(tables + i);

    mrp_free(tables);
}
