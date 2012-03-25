#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/list.h>
#include <murphy-db/handle.h>
#include <murphy-db/hash.h>
#include <murphy-db/sequence.h>
#include "column.h"
#include "index.h"
#include "table.h"

static int print_blob(uint8_t *, int data, char *, int);

void mdb_column_write(mdb_column_t      *dst_desc, void *dst_data,
                      mqi_column_desc_t *src_desc, void *src_data)
{
    int lgh;
    void *dst, *src;

    if (dst_desc && dst_data && src_desc && src_desc->offset >= 0 && src_data){
        dst = dst_data + dst_desc->offset;
        src = src_data + src_desc->offset;
        lgh = dst_desc->length;

        switch (dst_desc->type) {

        case mqi_varchar:
            memset(dst, 0, lgh);
            strncpy((char *)dst, *(const char **)src, lgh-1);
            break;
            
        case mqi_integer:
            *(int32_t *)dst = *(int32_t *)src;
            break;
            
        case mqi_unsignd:
            *(uint32_t *)dst = *(uint32_t *)src;
            break;
            
        case mqi_blob:
            memcpy(dst, src, lgh);
            break;
            
        default:
            /* we do not knopw what this is,
               so we silently ignore it */
            break;
        }
    }
}


void mdb_column_read(mqi_column_desc_t *dst_desc, void *dst_data,
                     mdb_column_t      *src_desc, void *src_data)
{
    int lgh;
    void *dst, *src;

    if (dst_desc && dst_data && src_desc && src_data) {
        dst = dst_data + dst_desc->offset;
        src = src_data + src_desc->offset;
        lgh = src_desc->length;

        switch (src_desc->type) {

        case mqi_varchar:
            *(char **)dst = (char *)src;
            break;
            
        case mqi_integer:
            *(int32_t *)dst = *(int32_t *)src;
            break;
            
        case mqi_unsignd:
            *(uint32_t *)dst = *(uint32_t *)src;
            break;
            
        case mqi_blob:
            memcpy(dst, src, lgh);
            break;
            
        default:
            /* we do not know what this is,
               so we silently ignore it */
            break;
        }
    }
}


int mdb_column_print_header(mdb_column_t *cdesc, char *buf, int len)
{
    int r;
    int l;

    if (!cdesc || !buf || len < 1)
        r = 0;
    else {
        switch (cdesc->type) {
        case mqi_varchar:  l = cdesc->length;                       break;
        case mqi_integer:  l = 11;                                  break;
        case mqi_unsignd:  l = 11;                                  break;
        case mqi_blob:     l = cdesc->length > 0 ? (cdesc->length * 3) - 1 : 0;
                                                                    break;
        default:           l = 0;                                   break;
        }

        r = (l > 0) ? snprintf(buf,len, "%*s", l, cdesc->name) : 0;
    }

    return r;
}


int mdb_column_print(mdb_column_t *cdesc, void *data, char *buf, int len)
{
    int   r;
    int   l;
    void *d;

    if (!cdesc || !data || !buf || len < 1)
        r = 0;
    else {
        d = data + cdesc->offset;
        l = cdesc->length;

        switch (cdesc->type) {
        case mqi_varchar:  r = snprintf(buf,len, "%*s", l, (char *)d);   break;
        case mqi_integer:  r = snprintf(buf,len, "%11d", *(int32_t *)d); break;
        case mqi_unsignd:  r = snprintf(buf,len, " %10u",*(uint32_t*)d); break;
        case mqi_blob:     r = print_blob(d,cdesc->length, buf,len);     break;
        default:           r = 0;                                        break;
        }
    }

    return r;
}

static int print_blob(uint8_t *data, int data_len, char *buf, int buflen)
{
    return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
