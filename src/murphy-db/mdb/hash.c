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
#include <limits.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include <murphy-db/hash.h>
#include <murphy-db/list.h>

#ifndef HASH_STATISTICS
#define HASH_STATISTICS
#endif

typedef struct mdb_hash_entry_s {
    mdb_dlist_t  clink;         /* hash link, ie. chaining */
    mdb_dlist_t  elink;         /* entry link, ie. linking all entries */
    void        *key;
    void        *data;
} hash_entry_t;

typedef struct {
    mdb_dlist_t  head;
#ifdef HASH_STATISTICS
    struct {
        int curr;
        int max;
    }            entries;
#endif
} hash_chain_t;


struct mdb_hash_s {
    int                  bits;
    mdb_hash_function_t  hfunc;
    mdb_hash_compare_t   hcomp;
    mdb_hash_print_t     hprint;
    struct {
        mdb_dlist_t head;
#ifdef HASH_STATISTICS
        int         curr;
        int         max;
#endif
    }                    entries;
    int                  nchain;
    hash_chain_t         chains[0];
};


typedef struct {
    int  nchain;
    int  bits;
} table_size_t;

static table_size_t  sizes[]          = {
    {    2,  2}, {    3,  2}, {    5,  3}, {    7,  3}, {   11,  4},
    {   13,  4}, {   17,  5}, {   19,  5}, {   23,  5}, {   29,  5},
    {   31,  5}, {   37,  6}, {   41,  6}, {   43,  6}, {   47,  6},
    {   53,  6}, {   59,  6}, {   61,  6}, {   67,  7}, {   71,  7},
    {   73,  7}, {   79,  7}, {   83,  7}, {   89,  7}, {   97,  7},
    {  101,  7}, {  103,  7}, {  107,  7}, {  109,  7}, {  113,  7},
    {  127,  7}, {  131,  8}, {  137,  8}, {  139,  8}, {  149,  8},
    {  151,  8}, {  157,  8}, {  163,  8}, {  167,  8}, {  173,  8},
    {  179,  8}, {  181,  8}, {  191,  8}, {  193,  8}, {  197,  8},
    {  199,  8}, {  211,  8}, {  223,  8}, {  227,  8}, {  229,  8},
    {  233,  8}, {  239,  8}, {  241,  8}, {  251,  8}, {  257,  9},
    {  263,  9}, {  269,  9}, {  271,  9}, {  277,  9}, {  281,  9},
    {  283,  9}, {  293,  9}, {  307,  9}, {  311,  9}, {  313,  9},
    {  317,  9}, {  331,  9}, {  337,  9}, {  347,  9}, {  349,  9},
    {  353,  9}, {  359,  9}, {  367,  9}, {  373,  9}, {  379,  9},
    {  383,  9}, {  389,  9}, {  397,  9}, {  401,  9}, {  409,  9},
    {  419,  9}, {  421,  9}, {  431,  9}, {  433,  9}, {  439,  9},
    {  443,  9}, {  449,  9}, {  457,  9}, {  461,  9}, {  463,  9},
    {  467,  9}, {  479,  9}, {  487,  9}, {  491,  9}, {  499,  9},
    {  503,  9}, {  509,  9}, {  521, 10}, {  523, 10}, {  541, 10},
    {  547, 10}, {  557, 10}, {  563, 10}, {  569, 10}, {  571, 10},
    {  577, 10}, {  587, 10}, {  593, 10}, {  599, 10}, {  601, 10},
    {  607, 10}, {  613, 10}, {  617, 10}, {  619, 10}, {  631, 10},
    {  641, 10}, {  643, 10}, {  647, 10}, {  653, 10}, {  659, 10},
    {  661, 10}, {  673, 10}, {  677, 10}, {  683, 10}, {  691, 10},
    {  701, 10}, {  709, 10}, {  719, 10}, {  727, 10}, {  733, 10},
    {  739, 10}, {  743, 10}, {  751, 10}, {  757, 10}, {  761, 10},
    {  769, 10}, {  773, 10}, {  787, 10}, {  797, 10}, {  809, 10},
    {  811, 10}, {  821, 10}, {  823, 10}, {  827, 10}, {  829, 10},
    {  839, 10}, {  853, 10}, {  857, 10}, {  859, 10}, {  863, 10},
    {  877, 10}, {  881, 10}, {  883, 10}, {  887, 10}, {  907, 10},
    {  911, 10}, {  919, 10}, {  929, 10}, {  937, 10}, {  941, 10},
    {  947, 10}, {  953, 10}, {  967, 10}, {  971, 10}, {  977, 10},
    {  983, 10}, {  991, 10}, {  997, 10}, {65535, 16}
};
static uint32_t  charmap[256] = {
    /*        00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
    /* 00 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 10 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 20 */   0,  0,  0,  0,  0,  0,  0,  0, 52, 53, 54, 55, 56, 37, 40, 50,
    /* 30 */   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 41,  0, 42, 43, 44, 45,
    /* 40 */  46, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    /* 50 */  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 47, 48, 49, 51, 38,
    /* 60 */   0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    /* 70 */  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 57, 58, 59, 60,  0,
    /* 80 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 90 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* a0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* b0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* c0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* d0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* e0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* f0 */   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static void htable_reset(mdb_hash_t *, int);
static table_size_t *get_table_size(int);
static int print_chain(mdb_hash_t *, int, char *, int);


mdb_hash_t *mdb_hash_table_create(int                  max_entries,
                                  mdb_hash_function_t  hfunc,
                                  mdb_hash_compare_t   hcomp,
                                  mdb_hash_print_t     hprint)
{
    mdb_hash_t   *htbl;
    table_size_t *ts;
    size_t        size;
    int           i;

    MDB_CHECKARG(hfunc && hcomp && hprint &&
                 max_entries > 1 && max_entries < 65536, NULL);

    if ((ts = get_table_size(max_entries)) == NULL) {
        errno = EOVERFLOW;
        return NULL;
    }

    size = sizeof(mdb_hash_t) + sizeof(hash_chain_t) * ts->nchain;
    htbl = calloc(1, size);

    if (!htbl) {
        errno = ENOMEM;
        return NULL;
    }

    htbl->bits   = ts->bits;
    htbl->nchain = ts->nchain;
    htbl->hfunc  = hfunc;
    htbl->hcomp  = hcomp;
    htbl->hprint = hprint;

    MDB_DLIST_INIT(htbl->entries.head);

    for (i = 0; i < htbl->nchain; i++)
        MDB_DLIST_INIT(htbl->chains[i].head);

    return htbl;
}

int mdb_hash_table_destroy(mdb_hash_t *htbl)
{
    MDB_CHECKARG(htbl, -1);

    htable_reset(htbl, 0);
    free(htbl);

    return 0;
}

int mdb_hash_table_reset(mdb_hash_t *htbl)
{
    MDB_CHECKARG(htbl, -1);

    htable_reset(htbl, 1);

    return 0;
}

void *mdb_hash_table_iterate(mdb_hash_t *htbl,void **key_ret,void **cursor_ptr)
{
    mdb_dlist_t  *head, *link;
    hash_entry_t *entry;

    MDB_CHECKARG(htbl && cursor_ptr, NULL);

    head = &htbl->entries.head;

    if (!(link = *cursor_ptr))
        *cursor_ptr = link = head->next;

    if (link == head)
        return NULL;

    *cursor_ptr = link->next;

    entry = MDB_LIST_RELOCATE(hash_entry_t, elink, link);

    if (key_ret)
        *key_ret = entry->key;

    return entry->data;
}

int mdb_hash_table_print(mdb_hash_t *htbl, char *buf, int len)
{
    char *p, *e;
    int   i;

    MDB_CHECKARG(htbl && buf && len > 0, 0);

    e = (p = buf) + len;
    *buf = '\0';

    for (i = 0;  i < htbl->nchain;  i++) {
        if (!MDB_DLIST_EMPTY(htbl->chains[i].head)
#ifdef HASH_STATISTICS
            || htbl->chains[i].entries.max > 0
#endif
            )
            p += print_chain(htbl, i, p, e-p);
    }

    return p - buf;
}

int mdb_hash_add(mdb_hash_t *htbl, int klen, void *key, void *data)
{
    hash_entry_t *entry;
    hash_chain_t *chain;
    int           index;

    MDB_CHECKARG(htbl && key && klen >= 0 && data, -1);

    index = htbl->hfunc(htbl->bits, htbl->nchain, klen, key);
    chain = htbl->chains + index;

    MDB_DLIST_FOR_EACH(hash_entry_t, clink, entry, &chain->head) {
        if (htbl->hcomp(klen, key, entry->key) == 0) {
            if (data == entry->data)
                return 0;
            else {
                errno = EEXIST;
                return -1;
            }
        }
    }

    if (!(entry = calloc(1, sizeof(hash_entry_t)))) {
        errno = ENOMEM;
        return -1;
    }
    entry->key  = key;
    entry->data = data;

    MDB_DLIST_APPEND(hash_entry_t, clink, entry, &chain->head);
    MDB_DLIST_APPEND(hash_entry_t, elink, entry, &htbl->entries.head);

#ifdef HASH_STATISTICS
    if (++chain->entries.curr > chain->entries.max)
        chain->entries.max = chain->entries.curr;

    if (++htbl->entries.curr > htbl->entries.max)
        htbl->entries.max = htbl->entries.curr;
#endif

    return 0;
}

void *mdb_hash_delete(mdb_hash_t *htbl, int klen, void *key)
{
    hash_entry_t *entry;
    hash_entry_t *n;
    hash_chain_t *chain;
    int           index;
    void         *data;

    MDB_CHECKARG(htbl && klen >= 0 && key, NULL);

    index = htbl->hfunc(htbl->bits, htbl->nchain, klen, key);
    chain = htbl->chains + index;

    MDB_DLIST_FOR_EACH_SAFE(hash_entry_t, clink, entry,n, &chain->head) {
        if (htbl->hcomp(klen, key, entry->key) == 0) {
            if (!(data = entry->data))
                break;

            MDB_DLIST_UNLINK(hash_entry_t, clink, entry);
            MDB_DLIST_UNLINK(hash_entry_t, elink, entry);
            free(entry);

#ifdef HASH_STATISTICS
            if (--chain->entries.curr < 0)
                chain->entries.curr = 0;

            if (--htbl->entries.curr < 0)
                htbl->entries.curr = 0;
#endif
            return data;
        }
    }

    errno = ENOENT;
    return NULL;
}

void *mdb_hash_get_data(mdb_hash_t *htbl, int klen, void *key)
{
    hash_entry_t *entry;
    hash_chain_t *chain;
    int           index;

    MDB_CHECKARG(htbl && klen >= 0 && key, NULL);

    index = htbl->hfunc(htbl->bits, htbl->nchain, klen, key);
    chain = htbl->chains + index;

    MDB_DLIST_FOR_EACH(hash_entry_t, clink, entry, &chain->head) {
        if (htbl->hcomp(klen, key, entry->key) == 0)
            return entry->data;
    }

    errno = ENOENT;
    return NULL;
}


int mdb_hash_function_integer(int bits, int nchain, int klen, void *key)
{
    return mdb_hash_function_unsignd(bits, nchain, klen, key);
}


int mdb_hash_function_unsignd(int bits, int nchain, int klen, void *key)
{
    uint32_t unsignd;

    if (klen != sizeof(unsignd) || !key ||
        bits < 1 || bits > 16 ||
        nchain < (1 << (bits-1)) || nchain >= (1 << bits))
        return 0;

    unsignd = *(uint32_t *)key;

    return (int)(unsignd % nchain);
}


int mdb_hash_function_string(int bits, int nchain, int klen, void *key)
{
    typedef union {
        uint64_t wide;
        uint8_t  narrow[8];
    } hash_t;

    (void)klen;

    uint8_t *varchar = (uint8_t *)key;
    int      hashval = 0;
    hash_t   h;
    int      shift;

    if (varchar && bits >= 1 && bits <= 16 &&
        nchain > (1 << (bits-1)) && nchain < (1 << bits))
    {
        uint8_t s;
        int     i;

        for (h.wide = 0; (s = *varchar); varchar++)
            h.wide = 33ULL * h.wide + (uint64_t)charmap[s];

        if (bits <= 8) {
            hashval = h.narrow[0] ^ h.narrow[1] ^ h.narrow[2] ^ h.narrow[3] ^
                      h.narrow[4] ^ h.narrow[5] ^ h.narrow[6] ^ h.narrow[7];
        }
        else {
            shift = (nchain + 7) / 8;
            for (hashval = h.narrow[0], i = 1;   i < 8;  i++)
                hashval ^= h.narrow[i] << (i * shift);
        }

        hashval %= nchain;
    }

    return hashval;
}

int mdb_hash_function_pointer(int bits, int nchain, int klen, void *key)
{
#define MASK(t)  ((((uint##t##_t)1) << (sizeof(int) * 8 - 3)) - 1)
    int hash;

    MQI_UNUSED(bits);
    MQI_UNUSED(klen);

#if __SIZEOF_POINTER__ == 8
    hash = (int)(((uint64_t)key >> 2) & MASK(64)) % nchain;
#else
    hash = ((int)key >> 2) & MASK(32) % nchain;
#endif

    return hash;
#undef MASK
}

int mdb_hash_function_varchar(int bits, int nchain, int klen, void *key)
{
    return mdb_hash_function_string(bits, nchain, klen, key);
}

int mdb_hash_function_blob(int bits, int nchain, int klen, void *key)
{
    typedef union {
        uint64_t wide;
        uint8_t  narrow[8];
    } hash_t;

    uint8_t *data  = (uint8_t *)key;
    int      hashval = 0;
    hash_t   h;
    int      shift;
    int      i;

    if (klen > 0 && data && bits >= 1 && bits <= 16 &&
        nchain > (1 << (bits-1)) && nchain < (1 << bits))
    {
        for (i = 0, h.wide = 0;   i < klen;   i++)
            h.wide = 33ULL * h.wide + (uint64_t)data[i];

        if (bits <= 8) {
            hashval = h.narrow[0] ^ h.narrow[1] ^ h.narrow[2] ^ h.narrow[3] ^
                      h.narrow[4] ^ h.narrow[5] ^ h.narrow[6] ^ h.narrow[7];
        }
        else {
            shift = (nchain + 7) / 8;
            for (hashval = h.narrow[0], i = 1;   i < 8;  i++)
                hashval ^= h.narrow[i] << (i * shift);
        }

        hashval %= nchain;
    }

    return hashval;
}



static void htable_reset(mdb_hash_t *htbl, int do_chain_statistics)
{
    hash_entry_t *entry;
    hash_entry_t *n;
#ifdef HASH_STATISTICS
    int i;
#else
    (void)do_statistics;
#endif

    MDB_DLIST_FOR_EACH_SAFE(hash_entry_t, elink, entry,n, &htbl->entries.head){
        MDB_DLIST_UNLINK(hash_entry_t, clink, entry);
        MDB_DLIST_UNLINK(hash_entry_t, elink, entry);
        free(entry);
    }

#ifdef HASH_STATISTICS
    if (do_chain_statistics) {
        for (i = 0;   i < htbl->nchain;   i++) {
            htbl->chains[i].entries.curr = 0;
        }
    }

    htbl->entries.curr = 0;
#endif
}

static table_size_t *get_table_size(int max_entries)
{
    int dim = sizeof(sizes)/sizeof(sizes[0]);
    int min = 0;
    int max = dim - 1;
    int idx;
#ifdef DEBUG
    int iterations = 0;
#endif

    for (;;) {
#ifdef DEBUG
        iterations++;
#endif
        idx = (min + max) / 2;

        if (max_entries == sizes[idx].nchain)
            break;

        if (idx == min) {
            idx = max;
            break;
        }

        if (max_entries < sizes[idx].nchain)
            max = idx;
        else
            min = idx;
    }

#ifdef DEBUG
    printf("%s(%d) => {%d,%d} @ %d\n", __FUNCTION__, max_entries,
           sizes[idx].nchain, sizes[idx].bits, iterations);
#endif

    return sizes + idx;
}

static int print_chain(mdb_hash_t *htbl, int index, char *buf, int len)
{
    hash_chain_t *chain = htbl->chains + index;
    hash_entry_t *entry;
    char *p, *e;
    char key[256];

    e = (p = buf) + len;

#ifdef HASH_STATISTICS
    p += snprintf(p, e-p, "   %05d: %d/%d\n",
                  index, chain->entries.curr, chain->entries.max);
#else
    p += snprintf(p, e-p, "   %05d\n", index);
#endif

    MDB_DLIST_FOR_EACH(hash_entry_t, clink, entry, &chain->head) {
        if (p >= e)
            break;

        htbl->hprint(entry->key, key, sizeof(key));

        p += snprintf(p, e-p, "      '%s' / %p\n", key, entry->data);
    }

    return p - buf;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
