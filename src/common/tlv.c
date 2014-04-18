/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#include <errno.h>
#include <limits.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/tlv.h>

#define TLV_MIN_PREALLOC 4096
#define TLV_MIN_CHUNK      64

int mrp_tlv_setup_write(mrp_tlv_t *tlv, size_t prealloc)
{
    if (prealloc < TLV_MIN_PREALLOC)
        prealloc = TLV_MIN_PREALLOC;

    if ((tlv->buf = mrp_allocz(prealloc)) == NULL)
        return -1;

    tlv->size  = prealloc;
    tlv->p     = tlv->buf;
    tlv->write = 1;

    return 0;
}


static inline size_t tlv_space(mrp_tlv_t *tlv)
{
    if (tlv->size > 0 && tlv->write)
        return tlv->size - (tlv->p - tlv->buf);
    else
        return 0;
}


static inline size_t tlv_data(mrp_tlv_t *tlv)
{
    if (!tlv->write)
        return tlv->size - (tlv->p - tlv->buf);
    else
        return tlv->p - tlv->buf;
}


int mrp_tlv_ensure(mrp_tlv_t *tlv, size_t size)
{
    size_t left, diff;

    if (!tlv->write)
        return -1;

    if ((left = tlv_space(tlv)) < size) {
        diff = size - left;

        if (diff < TLV_MIN_CHUNK)
            diff = TLV_MIN_CHUNK;

        tlv->p -= (ptrdiff_t)tlv->buf;

        if (mrp_realloc(tlv->buf, tlv->size + diff) == NULL) {
            tlv->p += (ptrdiff_t)tlv->buf;

            return -1;
        }

        memset(tlv->buf + tlv->size, 0, diff);

        tlv->size += diff;
        tlv->p    += (ptrdiff_t)tlv->buf;
    }

    return 0;
}


void *mrp_tlv_reserve(mrp_tlv_t *tlv, size_t size, int align)
{
    void      *reserved;
    ptrdiff_t  offs, pad;
    size_t     len;

    offs = tlv->p - tlv->buf;

    if (align > 1)
        pad = align - (offs & (align - 1));
    else
        pad = 0;

    len = size + pad;

    if (mrp_tlv_ensure(tlv, len) < 0)
        return NULL;

    if (pad)
        memset(tlv->p, 0, pad);

    reserved = tlv->p + pad;
    tlv->p  += len;

    return reserved;
}


int mrp_tlv_setup_read(mrp_tlv_t *tlv, void *buf, size_t size)
{
    tlv->buf   = tlv->p = buf;
    tlv->size  = size;
    tlv->write = 0;

    return 0;
}


static void *tlv_consume(mrp_tlv_t *tlv, size_t size)
{
    char *p;

    if (tlv_data(tlv) < size)
        return NULL;

    p = tlv->p;
    tlv->p += size;

    return p;
}


static void *tlv_peek(mrp_tlv_t *tlv, size_t size)
{
    char *p;

    if (tlv_data(tlv) < size)
        return NULL;

    p = tlv->p;

    return p;
}


void mrp_tlv_trim(mrp_tlv_t *tlv)
{
    size_t left;

    if (!tlv->write)
        return;

    if ((left = tlv_space(tlv)) == 0)
        return;

    tlv->p -= (ptrdiff_t)tlv->buf;

    if (mrp_realloc(tlv->buf, tlv->size - left) != NULL) {
        tlv->size -= left;
        tlv->p    += (ptrdiff_t)tlv->buf;
    }
}


size_t mrp_tlv_offset(mrp_tlv_t *tlv)
{
    return (size_t)(tlv->p - tlv->buf);
}


void mrp_tlv_cleanup(mrp_tlv_t *tlv)
{
    if (tlv->write)
        mrp_free(tlv->buf);

    tlv->buf  = tlv->p = NULL;
    tlv->size = 0;
}


void mrp_tlv_steal(mrp_tlv_t *tlv, void **bufp, size_t *sizep)
{
    if (tlv->write) {
        *bufp  = tlv->buf;
        *sizep = tlv->p - tlv->buf;

        tlv->buf  = tlv->p = NULL;
        tlv->size = 0;
    }
    else {
        *bufp  = NULL;
        *sizep = 0;
    }
}


static inline int push_tag(mrp_tlv_t *tlv, uint32_t tag)
{
    uint32_t *tagp;

    if (tag) {
        if ((tagp = mrp_tlv_reserve(tlv, sizeof(*tagp), 1)) == NULL)
            return -1;
        else
            *tagp = htobe32(tag);
    }

    return 0;
}


int mrp_tlv_push_int8(mrp_tlv_t *tlv, uint32_t tag, int8_t v)
{
    int8_t *p;

    mrp_debug("<0x%x>%d", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = v;

        return 0;
    }

    return -1;
}


int mrp_tlv_push_uint8(mrp_tlv_t *tlv, uint32_t tag, uint8_t v)
{
    uint8_t *p;

    mrp_debug("<0x%x>%u", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = v;

        return 0;
    }

    return -1;
}


int mrp_tlv_push_int16(mrp_tlv_t *tlv, uint32_t tag, int16_t v)
{
    int16_t *p;

    mrp_debug("<0x%x>%d", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe16(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_uint16(mrp_tlv_t *tlv, uint32_t tag, uint16_t v)
{
    uint16_t *p;

    mrp_debug("<0x%x>%u", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe16(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_int32(mrp_tlv_t *tlv, uint32_t tag, int32_t v)
{
    int32_t *p;

    mrp_debug("<0x%x>%d", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe32(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_uint32(mrp_tlv_t *tlv, uint32_t tag, uint32_t v)
{
    uint32_t *p;

    mrp_debug("<0x%x>%u", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe32(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_int64(mrp_tlv_t *tlv, uint32_t tag, int64_t v)
{
    int64_t *p;

    mrp_debug("<0x%x>%lld", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_uint64(mrp_tlv_t *tlv, uint32_t tag, uint64_t v)
{
    uint64_t *p;

    mrp_debug("<0x%x>%llu", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64(v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_float(mrp_tlv_t *tlv, uint32_t tag, float v)
{
    float *p;

    mrp_debug("<0x%x>%f", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = v;

        return 0;
    }

    return -1;
}


int mrp_tlv_push_double(mrp_tlv_t *tlv, uint32_t tag, double v)
{
    double *p;

    mrp_debug("<0x%x>%f", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = v;

        return 0;
    }

    return -1;
}


int mrp_tlv_push_bool(mrp_tlv_t *tlv, uint32_t tag, bool v)
{
    bool *p;

    mrp_debug("<0x%x>%s", tag, v ? "true" : "false");

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = v;

        return 0;
    }

    return -1;
}


int mrp_tlv_push_string(mrp_tlv_t *tlv, uint32_t tag, const char *str)
{
    uint32_t *sizep;
    char     *strp;
    size_t    len = str ? strlen(str) + 1 : 0;

    mrp_debug("<0x%x>'%s'", tag, str ? str : "");

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((sizep = mrp_tlv_reserve(tlv, sizeof(*sizep), 1)) == NULL)
        return -1;

    *sizep = htobe32((uint32_t)len);

    if (len > 0) {
        if ((strp = mrp_tlv_reserve(tlv, len, 1)) == NULL)
            return -1;

        strcpy(strp, str);
    }

    return 0;
}


int mrp_tlv_push_short(mrp_tlv_t *tlv, uint32_t tag, short v)
{
    int16_t *p;

    mrp_debug("<0x%x>%d", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe16((int16_t)v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_ushort(mrp_tlv_t *tlv, uint32_t tag, unsigned short v)
{
    uint16_t *p;

    mrp_debug("<0x%x>%u", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe16((uint16_t)v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_int(mrp_tlv_t *tlv, uint32_t tag, int v)
{
    int32_t *p;

    mrp_debug("<0x%x>%d", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe32((int32_t)v);

        return 0;
    }

    return -1;
}


int mrp_tlv_push_uint(mrp_tlv_t *tlv, uint32_t tag, unsigned int v)
{
    uint32_t *p;

    mrp_debug("<0x%x>%u", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe32((uint32_t)v);

        return 0;
    }

    return -1;

}


int mrp_tlv_push_long(mrp_tlv_t *tlv, uint32_t tag, long v)
{
    int64_t *p;

    mrp_debug("<0x%x>%ld", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64((int64_t)v);

        return 0;
    }

    return -1;

}


int mrp_tlv_push_ulong(mrp_tlv_t *tlv, uint32_t tag, unsigned long v)
{
    uint64_t *p;

    mrp_debug("<0x%x>%lu", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64((uint64_t)v);

        return 0;
    }

    return -1;

}


int mrp_tlv_push_ssize(mrp_tlv_t *tlv, uint32_t tag, ssize_t v)
{
    int64_t *p;

    mrp_debug("<0x%x>%zd", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64((int64_t)v);

        return 0;
    }

    return -1;

}


int mrp_tlv_push_size(mrp_tlv_t *tlv, uint32_t tag, size_t v)
{
    uint64_t *p;

    mrp_debug("<0x%x>%zu", tag, v);

    if (push_tag(tlv, tag) < 0)
        return -1;

    if ((p = mrp_tlv_reserve(tlv, sizeof(*p), 1)) != NULL) {
        *p = htobe64((uint64_t)v);

        return 0;
    }

    return -1;

}


int mrp_tlv_peek_tag(mrp_tlv_t *tlv, uint32_t *tag)
{
    uint32_t *tagp;

    if ((tagp = tlv_peek(tlv, sizeof(*tagp))) == NULL)
        return -1;

    *tag = be32toh(*tagp);

    return 0;
}


int pull_tag(mrp_tlv_t *tlv, uint32_t tag)
{
    uint32_t *tagp;

    if (tag) {
        if ((tagp = tlv_peek(tlv, sizeof(*tagp))) == NULL)
            return -1;

        if (be32toh(*tagp) != tag)
            return -1;
        else
            tlv_consume(tlv, sizeof(*tagp));
    }

    return 0;
}


int mrp_tlv_pull_int8(mrp_tlv_t *tlv, uint32_t tag, int8_t *v)
{
    int8_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = *p;

    return 0;
}


int mrp_tlv_pull_uint8(mrp_tlv_t *tlv, uint32_t tag, uint8_t *v)
{
    uint8_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = *p;

    return 0;
}


int mrp_tlv_pull_int16(mrp_tlv_t *tlv, uint32_t tag, int16_t *v)
{
    int16_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be16toh(*p);

    return 0;
}


int mrp_tlv_pull_uint16(mrp_tlv_t *tlv, uint32_t tag, uint16_t *v)
{
    uint16_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be16toh(*p);

    return 0;
}


int mrp_tlv_pull_int32(mrp_tlv_t *tlv, uint32_t tag, int32_t *v)
{
    int32_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be32toh(*p);

    return 0;
}


int mrp_tlv_pull_uint32(mrp_tlv_t *tlv, uint32_t tag, uint32_t *v)
{
    uint32_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be32toh(*p);

    return 0;
}


int mrp_tlv_pull_int64(mrp_tlv_t *tlv, uint32_t tag, int64_t *v)
{
    int64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be64toh(*p);

    return 0;
}


int mrp_tlv_pull_uint64(mrp_tlv_t *tlv, uint32_t tag, uint64_t *v)
{
    uint64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be64toh(*p);

    return 0;
}


int mrp_tlv_pull_float(mrp_tlv_t *tlv, uint32_t tag, float *v)
{
    float *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = *p;

    return 0;
}


int mrp_tlv_pull_double(mrp_tlv_t *tlv, uint32_t tag, double *v)
{
    double *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = *p;

    return 0;
}


int mrp_tlv_pull_bool(mrp_tlv_t *tlv, uint32_t tag, bool *v)
{
    bool *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = *p;

    return 0;
}


int mrp_tlv_pull_string(mrp_tlv_t *tlv, uint32_t tag, char **v, size_t max,
                        void *(alloc)(size_t, void *), void *alloc_data)
{
    uint32_t *sizep, size;
    char     *str;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((sizep = tlv_consume(tlv, sizeof(*sizep))) == NULL)
        return -1;

    size = be32toh(*sizep);

    if (max != (size_t)-1 && max < size) {
        errno = EOVERFLOW;
        return -1;
    }

    if (size > 0) {
        if ((str = tlv_consume(tlv, size)) == NULL)
            return -1;

        if (*v == NULL)
            if ((*v = alloc(size, alloc_data)) == NULL)
                return -1;

        strncpy(*v, str, size - 1);
        (*v)[size - 1] = '\0';
    }
    else
        *v = NULL;

    return 0;
}


int mrp_tlv_pull_short(mrp_tlv_t *tlv, uint32_t tag, short *v)
{
    int16_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be16toh(*p);

    return 0;
}


int mrp_tlv_pull_ushort(mrp_tlv_t *tlv, uint32_t tag, unsigned short *v)
{
    uint16_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be16toh(*p);

    return 0;
}


int mrp_tlv_pull_int(mrp_tlv_t *tlv, uint32_t tag, int *v)
{
    int32_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be32toh(*p);

    return 0;
}


int mrp_tlv_pull_uint(mrp_tlv_t *tlv, uint32_t tag, unsigned int *v)
{
    uint16_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    *v = be32toh(*p);

    return 0;
}


int mrp_tlv_pull_long(mrp_tlv_t *tlv, uint32_t tag, long *v)
{
    int64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    if (*p > LONG_MAX) {
        errno = ERANGE;
        return -1;
    }

    *v = (long)be64toh(*p);

    return 0;
}


int mrp_tlv_pull_ulong(mrp_tlv_t *tlv, uint32_t tag, unsigned long *v)
{
    uint64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    if (*p > ULONG_MAX) {
        errno = ERANGE;
        return -1;
    }

    *v = (unsigned long)be64toh(*p);

    return 0;
}


int mrp_tlv_pull_ssize(mrp_tlv_t *tlv, uint32_t tag, ssize_t *v)
{
    int64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    if (*p > SSIZE_MAX) {
        errno = ERANGE;
        return -1;
    }

    *v = (ssize_t)be64toh(*p);

    return 0;
}


int mrp_tlv_pull_size(mrp_tlv_t *tlv, uint32_t tag, size_t *v)
{
    uint64_t *p;

    if (pull_tag(tlv, tag) < 0)
        return -1;

    if ((p = tlv_consume(tlv, sizeof(*p))) == NULL)
        return -1;

    if (*p > SIZE_MAX) {
        errno = ERANGE;
        return -1;
    }

    *v = (size_t)be64toh(*p);

    return 0;
}


static inline int8_t int8v(int8_t v)
{
    return v;
}

static inline float floatv(float v)
{
    return v;
}

static inline double doublev(double v)
{
    return v;
}

static inline bool boolv(bool v)
{
    return v;
}


#define TYPE_PEEKER(_type_name, _type, _endconv)                      \
int mrp_tlv_peek_##_type_name(mrp_tlv_t *tlv, uint32_t tag, _type *v) \
{                                                                     \
    uint32_t *tagp;                                                   \
    _type    *p;                                                      \
                                                                      \
    if ((tagp = tlv_peek(tlv, sizeof(*tagp) + sizeof(*p))) == NULL) { \
        errno = ENODATA;                                              \
        return -1;                                                    \
    }                                                                 \
                                                                      \
    if (be32toh(*tagp) == tag) {                                      \
        p = (void *)tagp + sizeof(*tagp);                             \
        *v = (_type)_endconv(*p);                                     \
        return 1;                                                     \
    }                                                                 \
    else                                                              \
        return 0;                                                     \
}


#define HOST_PEEKER(_type_name, _htype, _type, _endconv)               \
int mrp_tlv_peek_##_type_name(mrp_tlv_t *tlv, uint32_t tag, _htype *v) \
{                                                                     \
    uint32_t *tagp;                                                   \
    _type    *p;                                                      \
                                                                      \
    if ((tagp = tlv_peek(tlv, sizeof(*tagp) + sizeof(*p))) == NULL) { \
        errno = ENODATA;                                              \
        return -1;                                                    \
    }                                                                 \
                                                                      \
    if (be32toh(*tagp) == tag) {                                      \
        p = (void *)tagp + sizeof(*tagp);                             \
        *v = (_htype)_endconv(*p);                                    \
        return 1;                                                     \
    }                                                                 \
    else                                                              \
        return 0;                                                     \
}


TYPE_PEEKER(int8  , int8_t        , int8v  );
TYPE_PEEKER(uint8 , uint8_t       , int8v  );
TYPE_PEEKER(int16 , int16_t       , be16toh);
TYPE_PEEKER(uint16, uint16_t      , be16toh);
TYPE_PEEKER(int32 , int32_t       , be32toh);
TYPE_PEEKER(uint32, uint32_t      , be32toh);
TYPE_PEEKER(int64 , int64_t       , be64toh);
TYPE_PEEKER(uint64, uint64_t      , be64toh);
TYPE_PEEKER(float , float         , floatv );
TYPE_PEEKER(double, double        , doublev);
TYPE_PEEKER(bool  , bool          , boolv  );

HOST_PEEKER(short , short         , int16_t  , be16toh);
HOST_PEEKER(ushort, unsigned short, uint16_t , be16toh);
HOST_PEEKER(int   , int           , int32_t  , be32toh);
HOST_PEEKER(uint  , unsigned int  , uint32_t , be32toh);
HOST_PEEKER(long  , long          , int64_t  , be64toh);
HOST_PEEKER(ulong , unsigned long , uint64_t , be64toh);
HOST_PEEKER(ssize , ssize_t       , int64_t  , be64toh);
HOST_PEEKER(size  , size_t        , uint64_t , be64toh);
