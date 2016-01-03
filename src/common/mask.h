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
#include <stdbool.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/mm.h>


MRP_CDECL_BEGIN

/**
 * \addtogroup MurphyCommonInfra
 *
 * @{
 *
 * @file mask.h
 *
 * @brief A trivial implementation of arbitrary-sized bitmasks.
 *
 * @mrp_mask_t is a straight-forward implementation of a data type that
 * can be used to represent bitmasks of arbitrary sizes. A bitmask is
 * a collection of bits that can be individually turned on, off, and
 * tested for. Any bit within the mask is referred to by its index. Bit
 * indices are zero-based, so a fixed-size bitmask of N bits have valid
 * indices 0 ... N - 1.
 *
 * This implementation supports both dynamically and statically sized
 * bitmasks. Mask instances can be dynamically changed from dynamic
 * to static and vice versa any time by locking and unlocking the mask.
 *
 * Additionally, masks can be inlined. An inlined mask has its storage
 * of mask bits layed out in memory directly following the mask itself.
 * Inlined masks are implicitly statically sized. Inlined masks might
 * come useful, if you need to implement your own page-based or slicing
 * object allocator. With such a setup you need to administer free and
 * taken slots in a chunk of memory. This is often accomplished by using
 * per chunk allocation bitmap where each free bit corresponds to a free
 * object slot within the chunk. Depending on the ration betweem the size
 * of objects and the size of chunks you allocate, you often end up with
 * wasted, leftover bits in chunks. If you have enough leftover bits, you
 * can use them as an inlined allocation mask to minimize wasted memory.
 *
 * The API provides functions for creating, resetting, locking, unlocking,
 * and resizing bitmasks as well as for setting, clearing, and testing
 * individual bits within a mask. There are also functions provided for
 * quickly locating the first (IOW lowest) bit set or clear in a mask.
 * These functions are usually quite efficient, whenever available they
 * use special dedicated CPU instructions for these operations.
 *
 * Finally, various convenience functions are provided that might come
 * handy in typical usage of bitmasks, for instance when implementing
 * resource usage administration of rmemory or some other allocators.
 *
 * The implementation of @mrp_mask_t is optimised primarily for speed,
 * and barely at all for memory usage. Apart from the absolutely most
 * obvious, nothing is done to reduce memory usage. Internally the masks
 * are always dense, and the starting offset of the lowest bit within a
 * mask is always 0.
 */

/**
 * Use 64-bit mask words internally.
 */
#define __MRP_MASK_64BIT__

/**
 * @brief Type used to store bits within a mask.
 *
 * This is the data type we internally used to store bits within a mask.
 * We use 64-bit masks, but an attempt is made to make it easy to switch
 * this 32-bit internal masks if it is desirable, to reduce memory usage.
 */
#ifdef __MRP_MASK_64BIT__
    typedef uint64_t _mask_t;
#else
    typedef uint32_t _mask_t;
#endif

/**
 * @brief Macro to find the first bit set in a single @_mask_t word.
 *
 * This macro is set to mrp_ffsll, or mrp_ffsl depending on whether
 * @_mask_t is 64- or 32-bit internally.
 */
#ifdef __MRP_MASK_64BIT__
#    define _ONEBIT (1ULL)
#else
#    define _ONEBIT (1U)
#endif

/**
 * @brief Bit-fiddling macros that operate on _mask_t (or any integer).
 */

/**
 * @brief Get mask for the given @bit.
 */
#define MRP_MASK_BIT(bit) (1ULL << (bit))

/**
 * @brief Set all bits to 1 below @bit.
 */
#define MRP_MASK_BELOW(bit) ((1ULL << (bit)) - 1)

/**
 * @brief Set all bits to 1 above @bit.
 */
#define MRP_MASK_ABOVE(bit) (~MRP_MASK_BELOW(bit)&~MRP_MASK_BIT(bit))

/**
 * @brief Set all bits to 1 below and including @bit.
 */
#define MRP_MASK_UPTO(bit) ((1ULL << ((bit) + 1)) - 1)

/**
 * @brief Internal helper macros to deal with @_mask_t of different sizes.
 */
/**< Number of bits per @_mask_t word. */
#define _BYTES_PER_WORD ((int)sizeof(_mask_t))

/**< Number of bytes per @_mask_t word. */
#define _BITS_PER_WORD  (8 * _BYTES_PER_WORD)

/**< Word index for @bit. */
#define _WRD_IDX(bit)   ((bit) / _BITS_PER_WORD)

/**< Bit index within word for @bit. */
#define _BIT_IDX(bit)   ((bit) % _BITS_PER_WORD)

/**< Bit mask within word for @bit. */
#define _BIT_MASK(bit)  (_ONEBIT << (bit))

/**< Total number of bits allocated for a mask that can store @bit. */
#define _BIT_NBIT(bit)  (_BITS_PER_WORD * (_WRD_IDX(bit) + 1))

/**< Total number of words allocated for a mask that can store @bit. */
#define _BIT_NWORD(bit) (_WRD_IDX(bit) + 1)

/**< Total number of bytes allocated for a mask that can store @bit. */
#define _BIT_NBYTE(bit) (_BYTES_PER_WORD * (_WRD_IDX(bit) + 1))

/**< Total number of bits allocated for a mask that can store @nbit bits. */
#define _NBIT_NBIT(nbit) (((nbit) + _BITS_PER_WORD - 1) & ~(_BITS_PER_WORD - 1))

/**< Total number of words allocated for a mask that can store @nbit bits. */
#define _NBIT_NWORD(nbit) (_NBIT_NBIT(nbit) / _BITS_PER_WORD)

/**< Total number of bytes allocated for a mask that can store @nbit bits. */
#define _NBIT_NBYTE(nbit) (_NBIT_NBIT(nbit) / 8)

/**
 * @brief Type used to represent bitmasks of arbitrary size.
 */
typedef struct {
    int nbit    : 24;                    /**< allocate for this many bits */
    int fixed   :  1;                    /**< fixed to current size */
    int inlined :  1;                    /**< whether an inlined mask */
    union {
        _mask_t  bitw;                   /**< bits when nbit <= 64 */
        _mask_t *bitp;                   /**< bits when nbit > 64 */
        _mask_t  biti[0];                /**< bits when inlined */
    };
} mrp_mask_t;

/**
 * @brief Macro to initialize an @mrp_mask_t variable to empty.
 */
#define MRP_MASK_EMPTY {                                \
            .nbit    = _BITS_PER_WORD,                  \
            .fixed   = 0,                               \
            .inlined = 0,                               \
            .bitw    = 0                                \
        }

/**
 * @brief Macro to initialize the given mask to empty.
 */
#define MRP_MASK_INIT(m) do {                    \
        (m)->nbit    = _BITS_PER_WORD;           \
        (m)->fixed   = 0;                        \
        (m)->inlined = 0;                        \
        (m)->bitw    = 0;                        \
    } while (0)

/**
 * @brief Macro to declare an initialize an empty bitmask.
 */
#define MRP_MASK(m) mrp_mask_t m = MRP_MASK_EMPTY

/**
 * @brief Check if a mask is dynamic.
 *
 * @param [in] m  pointer to mask to check
 *
 * @return Returns @true if the mask is dynamically sized, @false otherwise.
 */
static inline bool mrp_mask_dynamic(mrp_mask_t *m)
{
    return !(m->fixed || m->inlined);
}

/**
 * @brief Check if a mask has freeable memory.
 *
 * This function checks if the words of @m have been dynamically allocated.
 *
 * @param [in] m  pointer to the mask to check
 *
 * @return Returns @true if the mask words are dynamically allocated, @false
 *         otherwise.
 */
static inline bool mrp_mask_freeable(mrp_mask_t *m)
{
    return (mrp_mask_dynamic(m) && m->nbit > (int)_BITS_PER_WORD);
}

/**
 * @brief Get the address of mask words for a mask.
 *
 * This function determines the address of the first mask word of @m.
 *
 * @param [in] m      pointer to the mask to get the address for
 * @param [in,out] n  pointer to location to return word count in, or @NULL
 *
 * @return Returns the address of the first mask word for @m. Returns the
 *         number of mask words in @n if it is not @NULL.
 */
static inline _mask_t *mrp_mask_words(mrp_mask_t *m, int *nptr)
{
    _mask_t *w;

    if (m->inlined)
        w = &m->biti[0];
    else {
        if (m->nbit > (int)_BITS_PER_WORD)
            w = m->bitp;
        else
            w = &m->bitw;
    }

    if (nptr != NULL)
        *nptr = _NBIT_NWORD(m->nbit);

    return w;
}

/**
 * @brief Function to initialize a bitmask.
 *
 * This function initializes @m to be an empty mask of _BITS_PER_WORD bits.
 * The resulting mask is non-inlined, dynamic with no bits set.
 *
 * @param [in,out] m  pointer to the mask to initialize
 *
 * @return Returns @m.
 */
static inline mrp_mask_t *mrp_mask_init(mrp_mask_t *m)
{
    MRP_MASK_INIT(m);

    return m;
}

/**
 * @brief Get the full size of an inlined mask necessry to store @nbit bits.
 *
 * You can use this function to find out how much (extra) memory you need
 * to allocate to store an inlined mask for @nbit bits.
 *
 * @param [in] nbit  nunmber of bits in mask
 *
 * @return Returns the size of an inlined mrp_mask_t structure necessary to
 *         store @nbit bits.
 */
static inline size_t mrp_mask_inlined_size(int nbit)
{
    return MRP_OFFSET(mrp_mask_t, biti[_NBIT_NWORD(nbit)]);
}

/**
 * @brief Function to initialize a bitmask with inlined storage.
 *
 * Use this function to initialize @m for inlined storage of max @nbit bits.
 * Additionally, if @base is not @NULL, this function will check that @m
 * fits within the memory area starting at @base and having @size size.
 *
 * @param [in] m     mask to initialize
 * @param [in] nbit  number of bits to initialize @m for
 * @param [in] base  optional base pointer to check @m agains
 * @param [in] size  size @m must fit into if @base is given
 *
 * @return Returns @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_init_inlined(mrp_mask_t *m, int nbit,
                                                void *base, size_t size)
{
    if (base != NULL) {
        if ((void *)m < base)
            return NULL;

        if (((void *)m) + mrp_mask_inlined_size(nbit) > base + size)
            return NULL;
        else {
            mrp_debug("inlined mask size for %d (%d) bits: %zu, %zu bytes left",
                      nbit, _NBIT_NBIT(nbit), mrp_mask_inlined_size(nbit),
                      (base + size) - ((void *)m) + mrp_mask_inlined_size(nbit));
        }
    }

    MRP_MASK_INIT(m);

    m->nbit = nbit;
    m->inlined = true;
    memset(m->biti, 0, _NBIT_NBYTE(nbit));

    return m;
}

/**
 * @brief Reset the given mask to be empty.
 *
 * This function resets @m to be empty. If the mask is dynamic, it will be
 * reset to have _BITS_PER_WORD bits. All bits in the resulting mask are
 * cleared.
 *
 * @param [in, out] m  pointer to the mask to initialize
 *
 * @return Returns @m.
 */
static inline mrp_mask_t *mrp_mask_reset(mrp_mask_t *m)
{
    _mask_t *w;
    int i, n;

    if (mrp_mask_dynamic(m) && mrp_mask_freeable(m)) {
        mrp_free(m->bitp);
        MRP_MASK_INIT(m);
    }

    w = mrp_mask_words(m, &n);
    for (i = 0; i < n; i++)
        *w = 0;

    return m;
}

/**
 * @brief Enlarge a mask to be able to accomodate a given number of bits.
 *
 * This function, if possible, grows @m to accomodate @nbit bits.
 *
 * @param [in] m     pointer to the mask to grow
 * @param [in] nbit  the number of bits to grow @m to
 *
 * @return Returns @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_grow(mrp_mask_t *m, int nbit)
{
    int o, n;

    if (m->fixed && m->nbit < nbit)
        return NULL;

    if ((o = _NBIT_NBIT(m->nbit)) >= (n = _NBIT_NBIT(nbit)))
        return m;

    if (m->fixed || m->inlined)
        return NULL;

    if (o == _BITS_PER_WORD) {
        _mask_t w = m->bitw;

        m->bitp = mrp_allocz((n / _BITS_PER_WORD) * sizeof(m->bitp[0]));

        if (m->bitp == NULL) {
            m->bitw = w;
            return NULL;
        }

        m->bitp[0] = w;
    }
    else {
        if (!mrp_reallocz(m->bitp, o / _BITS_PER_WORD, n / _BITS_PER_WORD))
            return NULL;
    }

    m->nbit = nbit;
    return m;
}

#define mrp_mask_ensure mrp_mask_grow

/**
 * @brief Shrink a mask to the given number of bits.
 *
 * This function, if possible, shrinks @m to accomodate only @nbit bits.
 *
 * @param [in] m     pointer to the mask to shrink
 * @param [in] nbit  the number of bits to shrink @m to
 *
 * @return Returns @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_shrink(mrp_mask_t *m, int nbit)
{
    int o, n;

    if ((o = _NBIT_NBIT(m->nbit)) == (n = _NBIT_NBIT(nbit)))
        return m;

    if (o < n)
        return mrp_mask_grow(m, nbit);

    if (o > n && (m->fixed || m->inlined))
        return m;

    if (n == _BITS_PER_WORD) {
        _mask_t *w = m->bitp;

        m->bitw = w[0];

        mrp_free(w);
    }
    else {
        if (!mrp_reallocz(m->bitp, o / _BITS_PER_WORD, n / _BITS_PER_WORD))
            return NULL;
    }

    m->nbit = nbit;

    return m;
}

/**
 * @brief Lock a mask to be of fixed size.
 *
 * This function locks @m to @nbit bits, or to its current size if @nbit is 0.
 *
 * @param [in] m     mask to lock
 * @param [in] nbit  number of bits to fix size, or 0 for current size
 *
 * @return Returns @m on success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_lock(mrp_mask_t *m, int nbit)
{
    if (nbit <= 0)
        nbit = m->nbit;

    if (m->nbit < nbit)
        if (!mrp_mask_grow(m, nbit))
            return NULL;

    if (m->nbit > nbit)
        if (!mrp_mask_shrink(m, nbit))
            return NULL;

    m->fixed = 1;
    return m;
}

/**
 * @brief Unlock a mask.
 *
 * This function can be used to unlock @m.
 *
 * @param [in] m  mask to unlock
 *
 * @return Returns @m on success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_unlock(mrp_mask_t *m)
{
    m->fixed = 0;

    return m;
}

/**
 * @brief Clear all bits above the given bit in a mask.
 *
 * This functions clears all bits in @m starting at @bit+1.
 *
 * @param [in] m    mask to clear bits in
 * @param [in] bit  bit to clear all bits above
 *
 * @return Returns @m upon sucess, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_clear_above(mrp_mask_t *m, int bit)
{
    int wi, bi, i, n;
    _mask_t *w;

    if (_NBIT_NBIT(m->nbit) < _BIT_NBIT(bit))
        return m;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    w[wi] &= ~MRP_MASK_ABOVE(bi);

    for (i = wi + 1; i < n; i++)
        w[i] = 0;

    return m;
}

/**
 * @brief Clear all bits below the given bit in a mask.
 *
 * This functions clears all bits in @m up to and including 0 - @bit-1.
 *
 * @param [in] m    mask to clear bits in
 * @param [in] bit  bit to clear all bits below
 *
 * @return Returns @m upon sucess, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_clear_below(mrp_mask_t *m, int bit)
{
    int wi, bi, i, n;
    _mask_t *w;

    if (bit > m->nbit - 1)
        bit = m->nbit - 1;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    for (i = 0; i < wi; i++)
        w[i] = 0;

    w[wi] &= ~MRP_MASK_BELOW(bi);

    return m;
}

/**
 * @brief Clear all bits within a range in a mask.
 *
 * This function clears bits @l - @h in @m.
 *
 * @param [in] m  mask to clear bits in
 * @param [in] l  lowest bit to clear
 * @param [in] h  highest bit to clear
 *
 * @return Return @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_clear_range(mrp_mask_t *m, int l, int h)
{
    int lw, lb, hw, hb, i, n, swp;
    _mask_t *w;

    if (l > h) {
        swp = l;
        l = h;
        h = swp;
    }

    if (h > m->nbit - 1)
        h = m->nbit - 1;

    lw = _WRD_IDX(l);
    lb = _BIT_IDX(l);
    hw = _WRD_IDX(h);
    hb = _BIT_IDX(h);
    w = mrp_mask_words(m, &n);

    if (lw != hw)
        w[lw] &= ~(MRP_MASK_BIT(lb) | MRP_MASK_ABOVE(lb));

    for (i = lw + 1; i < hw; i++)
        w[i] = 0;

    if (lw != hw)
        w[hw] &= ~(MRP_MASK_BIT(hb) | MRP_MASK_BELOW(hb));
    else
        w[hw] &= ~(MRP_MASK_BIT(lb) |
                   (MRP_MASK_ABOVE(lb) & MRP_MASK_BELOW(hb)) |
                   MRP_MASK_BIT(hb));

    return m;
}

/**
 * @brief Set all bits above the given bit in a mask.
 *
 * This functions sets all bits in @m starting at @bit+1.
 *
 * @param [in] m    mask to set bits in
 * @param [in] bit  bit to set all bits above
 *
 * @return Returns @m upon sucess, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_set_above(mrp_mask_t *m, int bit)
{
    int wi, bi, i, n;
    _mask_t *w;

    if (_NBIT_NBIT(m->nbit) < _BIT_NBIT(bit))
        return m;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    w[wi] |= MRP_MASK_ABOVE(bi);

    for (i = wi + 1; i < n; i++)
        w[i] = (_mask_t)-1;

    return m;
}

/**
 * @brief Set all bits below the given bit in a mask.
 *
 * This functions sets all bits in @m up to and including 0 - @bit-1.
 *
 * @param [in] m    mask to set bits in
 * @param [in] bit  bit to set all bits below
 *
 * @return Returns @m upon sucess, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_set_below(mrp_mask_t *m, int bit)
{
    int wi, bi, i, n;
    _mask_t *w;

    if (bit > m->nbit - 1)
        bit = m->nbit - 1;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    for (i = 0; i < wi; i++)
        w[i] = (_mask_t)-1;

    w[wi] |= MRP_MASK_BELOW(bi);


    return m;
}

/**
 * @brief Set all bits within a range in a mask.
 *
 * This function sets bits @l - @h in @m.
 *
 * @param [in] m  mask to set bits in
 * @param [in] l  lowest bit to set
 * @param [in] h  highest bit to set
 *
 * @return Return @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_set_range(mrp_mask_t *m, int l, int h)
{
    int lw, lb, hw, hb, i, n, swp;
    _mask_t *w;

    if (l > h) {
        swp = l;
        l = h;
        h = swp;
    }

    if (h > m->nbit - 1)
        h = m->nbit - 1;

    lw = _WRD_IDX(l);
    lb = _BIT_IDX(l);
    hw = _WRD_IDX(h);
    hb = _BIT_IDX(h);
    w = mrp_mask_words(m, &n);

    if (lw != hw)
        w[lw] |= (MRP_MASK_BIT(lb) | MRP_MASK_ABOVE(lb));

    for (i = lw + 1; i < hw; i++)
        w[i] = (_mask_t)-1;

    if (lw != hw)
        w[hw] |= (MRP_MASK_BIT(hb) | MRP_MASK_BELOW(hb));
    else
        w[hw] |= (MRP_MASK_BIT(lb) |
                  (MRP_MASK_ABOVE(lb) & MRP_MASK_BELOW(hb)) |
                  MRP_MASK_BIT(hb));
    return m;
}

/**
 * @brief Set a given bit in a mask.
 *
 * Set @bit to 1 in the given mask @m.
 *
 * @param [in] m    mask to set bit in
 * @param [in] bit  bit to set
 *
 * @return Returns @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_set(mrp_mask_t *m, int bit)
{
    int wi, bi;
    _mask_t *w;

    if (!mrp_mask_grow(m, bit + 1))
        return NULL;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, NULL);

    w[wi] |= MRP_MASK_BIT(bi);

    if (m->nbit < bit + 1)
        m->nbit = bit + 1;

    return m;
}

/**
 * @brief Clear a given bit in a mask.
 *
 * Clear @bit to 0 in the given mask @m.
 *
 * @param [in] m    mask to clear bit in
 * @param [in] bit  bit to clear
 *
 * @return Returns @m upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_clear(mrp_mask_t *m, int bit)
{
    int wi, bi;
    _mask_t *w;

    if (m->nbit < bit + 1)
        return m;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, NULL);

    w[wi] &= ~MRP_MASK_BIT(bi);

    return m;
}

/**
 * @brief Test the given bit in a mask.
 *
 * Return the whether @bit is set in the given mask @m.
 *
 * @param [in] m    mask to test bit in
 * @param [in] bit  bit to test
 *
 * @return Returns the value of @bit in @m.
 */
static inline int mrp_mask_test(mrp_mask_t *m, int bit)
{
    int wi, bi;
    _mask_t *w;

    if (m->nbit < bit + 1)
        return 0;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, NULL);

    return !!(w[wi] & MRP_MASK_BIT(bi));
}

/**
 * @brief Copy a mask.
 *
 * @param [in] src   mask to copy
 * @param [out] dst  mask to copy @src into
 *
 * @return Returns @dst upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_copy(mrp_mask_t *dst, mrp_mask_t *src)
{
    _mask_t *s, *d;
    int i, n;

    if (src->nbit > dst->nbit) {
        if (!mrp_mask_grow(dst, src->nbit))
            return NULL;
    }
    else if (src->nbit < dst->nbit) {
        if (mrp_mask_shrink(dst, src->nbit))
            return NULL;
    }

    s = mrp_mask_words(src, &n);
    d = mrp_mask_words(dst, NULL);

    for (i = 0; i < n; i++)
        d[i] = s[i];

    return dst;
}

/**
 * @brief Compare two masks for equality.
 *
 * This function checks if the given two masks are equal.
 *
 * @param [in] m1  mask to compare
 * @param [in] m2  mask to compare
 *
 * @return Returns 0 is the masks are equal, non-zero otherwise.
 */
static inline int mrp_mask_cmp(mrp_mask_t *m1, mrp_mask_t *m2)
{
    _mask_t *s, *d;
    int i, n;

    if (m1->nbit != m2->nbit)
        return -1;

    s = mrp_mask_words(m1, &n);
    d = mrp_mask_words(m2, &n);

    for (i = 0; i < n; i++)
        if (s[i] != d[i])
            return -1;

    return 0;
}

/**
 * @brief Take a bitwise OR of two masks.
 *
 * This functions make @dst be a bitwise OR of @src and @dst. IOW, calling
 * this functions results in @dst |= src.
 *
 * @param [in] src      mask to OR into @dst
 * @param [in,out] dst  mask to OR @src into
 *
 * @return Returns @dst upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_or(mrp_mask_t *dst, mrp_mask_t *src)
{
    _mask_t *s, *d;
    int i, sn, dn, n;

    if (src->nbit > dst->nbit)
        if (!mrp_mask_grow(dst, src->nbit))
            return NULL;

    s = mrp_mask_words(src, &sn);
    d = mrp_mask_words(dst, &dn);
    n = MRP_MAX(sn, dn);

    for (i = 0; i < n; i++)
        d[i] |= s[i];

    return dst;
}

/**
 * @brief Take a bitwise AND of two masks.
 *
 * This functions make @dst be a bitwise AND of @src and @dst. IOW, calling
 * this functions results in @dst &= src.
 *
 * @param [in] src      mask to AND into @dst
 * @param [in,out] dst  mask to AND @src into
 *
 * @return Returns @dst upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_and(mrp_mask_t *dst, mrp_mask_t *src)
{
    _mask_t *s, *d;
    int i, sn, dn, n;

    if (src->nbit < dst->nbit)
        if (!mrp_mask_shrink(dst, src->nbit))
            return NULL;

    s = mrp_mask_words(src, &sn);
    d = mrp_mask_words(dst, &dn);
    n = MRP_MIN(sn, dn);

    for (i = 0; i < n; i++)
        d[i] &= s[i];

    return dst;
}

/**
 * @brief Take a bitwise XOR of two masks.
 *
 * This functions make @dst be a bitwise XOR of @src and @dst. IOW, calling
 * this functions results in @dst ^= src.
 *
 * @param [in] src      mask to XOR into @dst
 * @param [in,out] dst  mask to XOR @src into
 *
 * @return Returns @dst upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_xor(mrp_mask_t *dst, mrp_mask_t *src)
{
    _mask_t *s, *d;
    int i, sn, dn, n;

    if (src->nbit > dst->nbit)
        if (!mrp_mask_grow(dst, src->nbit))
            return NULL;

    s = mrp_mask_words(src, &sn);
    d = mrp_mask_words(dst, &dn);
    n = MRP_MAX(sn, dn);

    for (i = 0; i < n; i++)
        d[i] ^= s[i];

    return dst;
}

/**
 * @brief Negate a given mask.
 *
 * This function negates a gvien mask either in place or into another
 * mask if given.
 *
 * @param [in]  src  mask to negate
 * @param [out] dst  mask to produce output into, or @NULL to negate in place
 *
 * @return Returns @dst or @src if @dst is @NULL upon success, @NULL otherwise.
 */
static inline mrp_mask_t *mrp_mask_not(mrp_mask_t *dst, mrp_mask_t *src)
{
    _mask_t *s, *d;
    int n, i;

    if (src == NULL)
        src = dst;
    else {
        if (src->nbit > dst->nbit) {
            if (!mrp_mask_grow(dst, src->nbit))
                return NULL;
        }
        else {
            if (!mrp_mask_shrink(dst, src->nbit))
                return NULL;
        }
    }

    s = mrp_mask_words(src, &n);
    d = mrp_mask_words(dst, NULL);

    for (i = 0; i < n; i++)
        d[i] = ~s[i];

    return dst;
}

#define mrp_mask_neg(m) mrp_mask_not(m, NULL)

/**
 * @brief Find the first bit set in the given mask.
 *
 * This function returns the index of the first bit set in the
 * given mask.
 *
 * @param [in] m  the mask to find the first bit set in
 *
 * @return Returns the index of the first bit set, or -1 if no bits are set.
 */
static inline int mrp_ffs(_mask_t bits)
{
#ifdef __GNUC__
#    ifdef __MRP_MASK_64BIT__
    return __builtin_ffsll(bits) - 1;
#    else
    return __builtin_ffsl(bits) - 1;
#    endif
#else
    _mask_t mask = (_mask_t)-1;
    int w, n;

    if (!bits)
        return -1;

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

    return n;
#endif
}

/**
 * @brief Get the first bit set in a mask starting at a given bit.
 *
 * This function find the first bit set in @m not lower than @bit.
 *
 * @param [in] m   the mask to search
 * @param in] bit  lowest set bit to accept
 *
 * @return Return the lowest bit set in @m above or equal to @bit, or -1
 *         if all such bit are cleared.
 */
static inline int mrp_mask_next_set(mrp_mask_t *m, int bit)
{
    _mask_t *w, clr;
    int i, n, wi, bi, b;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    clr = ~MRP_MASK_BELOW(bi);
    b   = mrp_ffs(w[wi] & clr);

    if (b >= 0)
        return wi * _BITS_PER_WORD + b;

    for (i = wi + 1; i < n; i++) {
        if ((b = mrp_ffs(w[i])) >= 0)
            return i * _BITS_PER_WORD + b;
    }

    return -1;
}

/**
 * @brief Get the first bit clear in a mask starting at a given bit.
 *
 * This function find the first bit clear in @m not lower than @bit.
 *
 * @param [in] m   the mask to search
 * @param in] bit  lowest clear bit to accept
 *
 * @return Return the lowest bit clear in @m above or equal to @bit, or -1
 *         if all such bit are set.
 */
static inline int mrp_mask_next_clear(mrp_mask_t *m, int bit)
{
    _mask_t *w, set;
    int i, n, wi, bi, b;

    wi = _WRD_IDX(bit);
    bi = _BIT_IDX(bit);
    w = mrp_mask_words(m, &n);

    set = MRP_MASK_BELOW(bi);
    b   = mrp_ffs(~(w[wi] | set));

    if (b >= 0)
        return wi * _BITS_PER_WORD + b;

    for (i = wi + 1; i < n; i++) {
        if ((b = mrp_ffs(~w[i])) >= 0)
            return i * _BITS_PER_WORD + b;
    }

    return -1;
}

/**
 * @brief Get the first cleared bit in a mask and set it to 1.
 *
 * This function finds the first clear bit in @m and sets it to 1. It is
 * a convenience function designed to be used to implement allocators.
 *
 * @param [in] m  bitmask to allocate lowest bit from
 *
 * @return Returns the index of free bit found and allocated in @m or -1
 *         if no free bits were found.
 */
static inline int mrp_mask_alloc(mrp_mask_t *m)
{
    _mask_t *w;
    int i, n, b;

    w = mrp_mask_words(m, &n);

    for (i = 0; i < n; i++) {
        if ((b = mrp_ffs(~w[i])) >= 0) {
            w[i] |= MRP_MASK_BIT(b);
            return i * _BITS_PER_WORD + b;
        }
    }

    return -1;
}

/**
 * @brief Macro to loop through all bits set in a mask.
 *
 * Loop thourgh all bits set in @m starting at @start, setting @bit to
 * the currently found set bit.
 *
 * @param m [in]      mask to loop through
 * @param start [in]  bit index to start at
 * @param bit [out]   variable to set to currently found bit index
 */
#define MRP_MASK_FOREACH_SET(m, bit, start)     \
    for (bit = mrp_mask_next_set(m, start);     \
         bit >= 0;                              \
         bit = mrp_mask_next_set(m, bit + 1))   \

/**
 * @brief Macro to loop through all bits clear in a mask.
 *
 * Loop thourgh all bits clear in @m starting at @start, setting @bit to
 * the currently found clear bit.
 *
 * @param m [in]      mask to loop through
 * @param start [in]  bit index to start at
 * @param bit [out]   variable to set to currently found bit index
 */
#define MRP_MASK_FOREACH_CLEAR(m, bit, start)   \
    for (bit = mrp_mask_next_clear(m, start);   \
         bit >= 0;                              \
         bit = mrp_mask_next_clear(m, bit + 1)) \

/**
 * @brief Function to print/dump a bitmask.
 *
 * This function prints the given bitmask into the string buffer.
 *
 * @param [in] buf   the buffer to dump the bitmask into
 * @param [in] size  size of the buffer
 * @param [in] m     mask to dump
 *
 * @return Returns the dumped buffer.
 */
static inline char *mrp_mask_dump(char *buf, size_t size, mrp_mask_t *m)
{
    char *p, *t;
    int l, n, i;

    p = buf;
    l = size;

    n = snprintf(p, l, "{");
    t = "";

    if (n >= l) {
    overflow:
        errno = ENOBUFS;
        return (char *)"<mrp_mask_dump(): insufficient buffer space>";
    }

    p += n;
    l -= n;

    for (i = 0; i < m->nbit; i++) {
        if (mrp_mask_test(m, i)) {
            n = snprintf(p, l, "%s%d", t, i);
            t = ",";
            if (n >= l)
                goto overflow;

            p += n;
            l -= n;
        }
    }

    n = snprintf(p, l, "}");

    if (n >= l)
        goto overflow;

    p += n;
    *p = '\0';

    return buf;
}


/**
 * @}
 */

MRP_CDECL_END

#endif /* __MURPHY_MASK_H__ */
