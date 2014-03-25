/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#ifndef __MURPHY_RESOURCE_MASK_H__
#define __MURPHY_RESOURCE_MASK_H__

#include <stdint.h>

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/resource/data-types.h>

#define MRP_BITS_PER_MASK (sizeof(uint32_t) * 8)

struct mrp_resource_mask_s {
    uint32_t *w;                         /* bitmask words */
    int       n;                         /* number of words */
};

#define MRP_RESOURCE_MASK_EMPTY_INIT { .w = NULL, .n = 0 }


static inline bool mrp_resource_mask_init(mrp_resource_mask_t *m, int nbit)
{
    int       nword;
    uint32_t *words;

    nword = (nbit + MRP_BITS_PER_MASK - 1) / MRP_BITS_PER_MASK;
    words = mrp_allocz(nword * sizeof(*words));

    if (words != NULL) {
        m->w = words;
        m->n = nword;

        return true;
    }
    else {
        m->w = NULL;
        m->n = 0;

        return false;
    }
}


static inline void mrp_resource_mask_adopt(mrp_resource_mask_t *m,
                                           uint32_t *w, int n)
{
    m->w = w;
    m->n = n;
}


static inline void mrp_resource_mask_cleanup(mrp_resource_mask_t *m)
{
    if (m != NULL) {
        mrp_free(m->w);
        m->w = NULL;
        m->n = 0;
    }
}


static inline void mrp_resource_mask_reset(mrp_resource_mask_t *m)
{
    if (MRP_UNLIKELY(m == NULL || m->n == 0))
        return;

    memset(m->w, 0, m->n * sizeof(m->w[0]));
}


static inline bool mrp_resource_mask_empty(mrp_resource_mask_t *m)
{
    int i;

    if (m == NULL || m->n == 0)
        return true;

    for (i = 0; i < m->n; i++)
        if (m->w[i])
            return false;

    return true;
}


static inline bool mrp_resource_mask_set_bit(mrp_resource_mask_t *m,
                                             uint32_t bit)
{
    int idx, mask;

    if (MRP_UNLIKELY(m == NULL || bit >= m->n * MRP_BITS_PER_MASK))
        return false;

    idx  = bit / MRP_BITS_PER_MASK;
    mask = 1 << (bit % MRP_BITS_PER_MASK);

    m->w[idx] |= mask;

    return true;
}


static inline bool mrp_resource_mask_clear_bit(mrp_resource_mask_t *m,
                                               uint32_t bit)
{
    int idx, mask;

    if (MRP_UNLIKELY(m == NULL || bit >= m->n * MRP_BITS_PER_MASK))
        return false;

    idx  = bit / MRP_BITS_PER_MASK;
    mask = 1 << (bit % MRP_BITS_PER_MASK);

    m->w[idx] &= ~mask;

    return true;
}


static inline bool mrp_resource_mask_test_bit(mrp_resource_mask_t *m,
                                              uint32_t bit)
{
    int idx, mask;

    if (MRP_UNLIKELY(m == NULL || bit >= m->n * MRP_BITS_PER_MASK))
        return false;

    idx  = bit / MRP_BITS_PER_MASK;
    mask = 1 << (bit % MRP_BITS_PER_MASK);

    return ((m->w[idx] & mask) ? true : false);
}


static inline bool mrp_resource_mask_test_mask(mrp_resource_mask_t *m,
                                               mrp_resource_mask_t *t)
{
    int i;

    if (MRP_UNLIKELY(m == NULL || t == NULL || m->n != t->n))
        return false;

    for (i = 0; i < m->n; i++)
        if ((m->w[i] & t->w[i]) != m->w[i])
            return false;

    return true;
}


static inline bool mrp_resource_mask_same(mrp_resource_mask_t *m,
                                          mrp_resource_mask_t *t)
{
    if (MRP_UNLIKELY(m == NULL || t == NULL || m->n != t->n))
        return false;

    if (!memcmp(m->w, t->w, m->n * sizeof(m->w[0])))
        return true;
    else
        return false;
}


static inline bool mrp_resource_mask_set_mask(mrp_resource_mask_t *m,
                                              mrp_resource_mask_t *t)
{
    int i;

    if (MRP_UNLIKELY(m == NULL || t == NULL || m->n != t->n))
        return false;

    for (i = 0; i < m->n; i++)
        m->w[i] |= t->w[i];

    return true;
}


static inline bool mrp_resource_mask_copy(mrp_resource_mask_t *d,
                                          mrp_resource_mask_t *s)
{
    if (MRP_UNLIKELY(s == NULL || d == NULL || s->n != d->n))
        return false;

    memcpy(d->w, s->w, s->n * sizeof(s->w[0]));

    return true;
}


static inline bool mrp_resource_mask_clear_mask(mrp_resource_mask_t *m,
                                                mrp_resource_mask_t *t)
{
    int i;

    if (MRP_UNLIKELY(m == NULL || t == NULL || m->n != t->n))
        return false;

    for (i = 0; i < m->n; i++)
        m->w[i] &= ~t->w[i];

    return true;
}


#endif /* __MURPHY_RESOURCE_MASK_H__ */
