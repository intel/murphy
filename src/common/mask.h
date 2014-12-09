/*
 * Copyright (c) 2014, Intel Corporation
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

#ifndef __MURPHY_MASK_H__
#define __MURPHY_MASK_H__

#include <stdint.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>


MRP_CDECL_BEGIN

/** Type used to store bits in bitmasks. */
typedef uint64_t _mask_t;


/**
 * trivial representation of a bitmask of arbitrary size
 */

typedef struct {
    int          nbit;                   /* number of bits in this mask */
    union {
        _mask_t  bits;                   /* bits for nbit <= 64 */
        _mask_t *bitp;                   /* bits for nbit >  64 */
    };
} mrp_mask_t;


/** Macro to intialize a bitmask to empty. */
#define MRP_MASK_EMPTY { .nbit = 64, .bits = 0 }

/** Macro to declare a bitmask variable and initialize it. */
#define MRP_MASK(m)    mrp_mask_t m = MRP_MASK_EMPTY

/* Various bit-fiddling macros. */
#define MRP_MASK_UPTO(bit)  ((1ULL << (bit)) - 1)
#define MRP_MASK_BELOW(bit) (MRP_MASK_UPTO(bit) >> 1)


#define _BITS_PER_WORD ((int)(sizeof(_mask_t) * 8))
#define _WORD(bit)     ((bit) /  _BITS_PER_WORD)
#define _BIT(bit)      ((bit) & (_BITS_PER_WORD - 1))
#define _MASK(bit)     (0x1ULL << (bit))


/** Initialize the given mask. */
static inline void mrp_mask_init(mrp_mask_t *m)
{
    *m = (mrp_mask_t)MRP_MASK_EMPTY;
}


/** Reset the given mask. */
static inline void mrp_mask_reset(mrp_mask_t *m)
{
    if (m->nbit > _BITS_PER_WORD)
        mrp_free(m->bitp);

    mrp_mask_init(m);
}


/** Ensure the given mask to accomodate the given number of bits. */
static inline int mrp_mask_ensure(mrp_mask_t *m, int bits)
{
    _mask_t w;
    int     o, n;

    if (bits < m->nbit)
        return 0;

    if (m->nbit == _BITS_PER_WORD) {
        w = m->bits;
        n = (bits + _BITS_PER_WORD - 1) / _BITS_PER_WORD;

        m->bitp = mrp_allocz(n * sizeof(*m->bitp));

        if (m->bitp == NULL) {
            m->bits = w;

            return -1;
        }

        m->bitp[0] = w;
        m->nbit    = n * _BITS_PER_WORD;
    }
    else {
        o = m->nbit / _BITS_PER_WORD;
        n = (bits + _BITS_PER_WORD - 1) / _BITS_PER_WORD;

        if (!mrp_reallocz(m->bitp, o, n + 1))
            return -1;

        m->nbit = n * _BITS_PER_WORD;
    }

    return 0;
}


/** Set the given bit in the mask. */
static inline int mrp_mask_set(mrp_mask_t *m, int bit)
{
    int w, b;

    if (mrp_mask_ensure(m, bit) < 0)
        return -1;

    b = _BIT(bit);

    if (m->nbit == _BITS_PER_WORD)
        m->bits |= _MASK(b);
    else {
        w = _WORD(bit);
        m->bitp[w] |= _MASK(b);
    }

    return 0;
}


/** Clear the given bit in the mask. */
static inline void mrp_mask_clear(mrp_mask_t *m, int bit)
{
    int w, b;

    if (bit >= m->nbit)
        return;

    b = _BIT(bit);

    if (m->nbit == _BITS_PER_WORD)
        m->bits &= ~_MASK(b);
    else {
        w = _WORD(bit);
        m->bitp[w] &= ~_MASK(b);
    }
}


/** Test the given bit in the mask. */
static inline int mrp_mask_test(mrp_mask_t *m, int bit)
{
    int w, b;

    if (bit >= m->nbit)
        return 0;

    b = _BIT(bit);

    if (m->nbit == _BITS_PER_WORD)
        return !!(m->bits & _MASK(b));
    else {
        w = _WORD(bit);
        return !!(m->bitp[w] & _MASK(b));
    }
}


/** Copy the given mask, overwriting dst. */
static inline int mrp_mask_copy(mrp_mask_t *dst, mrp_mask_t *src)
{
    if (src->nbit == _BITS_PER_WORD)
        *dst = *src;
    else {
        dst->nbit = src->nbit;
        dst->bitp = mrp_alloc(dst->nbit * _BITS_PER_WORD);

        if (dst->bitp == NULL)
            return -1;

        memcpy(dst->bitp, src->bitp, dst->nbit * _BITS_PER_WORD);
    }

    return 0;
}


/** Find the first bit set (1-based indexing) in the given mask. */
static inline int mrp_ffsll(_mask_t bits)
{
#ifdef __GNUC__
    return __builtin_ffsll(bits);
#else
    _mask_t mask = 0xffffffff;
    int     w, n;

    if (!bits)
        return 0;

    n = 0;
    w = _BITS_PER_WORD / 2;
    while (w) {
        if (!(bits & mask)) {
            bits >>= w;
            mask >>= w / 2;
            n     += w;
            w     /= 2;
        }
        else {
            bits  &= mask;
            mask >>= w / 2;
            w     /= 2;
        }
    }

    return n + 1;
#endif
}


/** Get the first bit set starting at the given bit. */
static inline int mrp_mask_next_set(mrp_mask_t *m, int bit)
{
    _mask_t wrd, clr;
    int     w, b, n;

    while (bit < m->nbit - 1) {
        w = _WORD(bit);
        b = _BIT(bit);

        if (m->nbit == _BITS_PER_WORD)
            wrd = m->bits;
        else
            wrd = m->bitp[w];

        clr = ~(_MASK(b) - 1);
        n   = mrp_ffsll(wrd & clr);

        if (n > 0)
            return w * _BITS_PER_WORD + n - 1;

        bit = (bit + _BITS_PER_WORD) & ~(_BITS_PER_WORD - 1);
    }

    return -1;
}


/** Get the first bit cleared starting at the given bit. */
static inline int mrp_mask_next_clear(mrp_mask_t *m, int bit)
{
    _mask_t wrd, clr;
    int     w, b, n;

    while (bit < m->nbit - 1) {
        w = _WORD(bit);
        b = _BIT(bit);

        if (m->nbit == _BITS_PER_WORD)
            wrd = m->bits;
        else
            wrd = m->bitp[w];

        clr = _MASK(b) - 1;
        n   = mrp_ffsll(~(wrd | clr));

        if (n > 0)
            return w * _BITS_PER_WORD + n - 1;

        bit = (bit + _BITS_PER_WORD) & ~(_BITS_PER_WORD - 1);
    }

    return -1;
}


/** Loop through all bits set in a mask. */
#define MRP_MASK_FOREACH_SET(m, bit, start)     \
    for (bit = mrp_mask_next_set(m, start);     \
         bit >= 0;                              \
         bit = mrp_mask_next_set(m, bit + 1))


/** Loop through all bits cleared in a mask. */
#define MRP_MASK_FOREACH_CLEAR(m, bit, start)    \
    for (bit = mrp_mask_next_clear(m, start);    \
         bit >= 0;                               \
         bit = mrp_mask_next_clear(m, bit + 1))

MRP_CDECL_END

#endif /* __MURPHY_MASK_H__ */
