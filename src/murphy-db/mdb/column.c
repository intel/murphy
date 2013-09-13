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

int mdb_column_write(mdb_column_t      *dst_desc, void *dst_data,
                     mqi_column_desc_t *src_desc, void *src_data)
{
    int lgh;
    void *dst, *src;
    static char *empty = "";

    if (dst_desc && dst_data && src_desc && src_desc->offset >= 0 && src_data){
        dst = dst_data + dst_desc->offset;
        src = src_data + src_desc->offset;
        lgh = dst_desc->length;

        switch (dst_desc->type) {

        case mqi_varchar:
            if(__builtin_expect(*((char**)src) == NULL, 0))
                src = &empty;

            if (!**(char **)src && !*(char *)dst)
                goto identical;

            if (!strncmp(*(char **)src, dst, strlen(*(char **)src) + 1))
                goto identical;

            memset(dst, 0, lgh);
            strncpy((char *)dst, *(const char **)src, lgh-1);
            break;

        case mqi_integer:
            if (*(int32_t *)dst == *(int32_t *)src)
                goto identical;
            else
                *(int32_t *)dst = *(int32_t *)src;
            break;

        case mqi_unsignd:
            if (*(uint32_t *)dst == *(uint32_t *)src)
                goto identical;
            else
                *(uint32_t *)dst = *(uint32_t *)src;
            break;

        case mqi_floating:
            if (*(double *)dst == *(double *)src)
                goto identical;
            else
                *(double *)dst = *(double *)src;
            break;

        case mqi_blob:
            if (!memcmp(dst, src, lgh))
                goto identical;
            else
                memcpy(dst, src, lgh);
            break;

        default:
            /* we do not knopw what this is,
               so we silently ignore it */
            goto identical;
        }
    }

    return 1;

 identical:
    return 0;
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

        case mqi_floating:
            *(double *)dst = *(double *)src;
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
    MQI_UNUSED(data);
    MQI_UNUSED(data_len);
    MQI_UNUSED(buf);
    MQI_UNUSED(buflen);

    return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
