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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define __USE_GNU
#include <string.h>

#include <murphy-db/assert.h>
#include <murphy-db/handle.h>

#define HANDLE_INDEX_INVALID -1
#define HANDLE_USEID_INVALID -1

#define HANDLE_USEID_BITS 16
#define HANDLE_INDEX_BITS ((sizeof(mdb_handle_t) * 8) - HANDLE_USEID_BITS)
#define HANDLE_USEID_MAX  (((mdb_handle_t)1) << HANDLE_USEID_BITS)
#define HANDLE_INDEX_MAX  (((mdb_handle_t)1) << HANDLE_INDEX_BITS)
#define HANDLE_USEID_MASK (HANDLE_USEID_MAX - 1)
#define HANDLE_INDEX_MASK (HANDLE_INDEX_MAX - 1)

#define HANDLE_MAKE(useid, index) (                                      \
    (((mdb_handle_t)(useid) & HANDLE_USEID_MASK) << HANDLE_INDEX_BITS) | \
    (((mdb_handle_t)(index) & HANDLE_INDEX_MASK))                        \
)

#define HANDLE_USEID(h) (((h) >> HANDLE_INDEX_BITS) & HANDLE_USEID_MASK)
#define HANDLE_INDEX(h) ((h) & HANDLE_INDEX_MASK)


typedef long long int bucket_t;


typedef struct {
    int       nbucket;
    bucket_t *buckets;
} freemap_t;

typedef struct {
    uint32_t   useid;
    void      *data;
} indextbl_entry_t;


typedef struct {
    int               nentry;
    indextbl_entry_t *entries;
} indextbl_t;

struct mdb_handle_map_s {
    freemap_t   freemap;
    indextbl_t  indextbl;
};


static mdb_handle_t index_alloc(indextbl_t *, int, void *);
static void *index_realloc(indextbl_t *, uint32_t, int, void *);
static void *index_free(indextbl_t *, uint32_t, int);
static int freemap_alloc(freemap_t *);
static int freemap_free(freemap_t *, int);

static bucket_t  empty_bucket    = ~((bucket_t)0);
static int       bits_per_bucket = sizeof(bucket_t) * 8;


mdb_handle_map_t *mdb_handle_map_create(void)
{
    mdb_handle_map_t *hmap;

    if (!(hmap = calloc(1, sizeof(mdb_handle_map_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    return hmap;
}

int mdb_handle_map_destroy(mdb_handle_map_t *hmap)
{
    MDB_CHECKARG(hmap, -1);

    free(hmap->freemap.buckets);
    free(hmap->indextbl.entries);

    free(hmap);

    return 0;
}


mdb_handle_t mdb_handle_add(mdb_handle_map_t *hmap, void *data)
{
    int index;

    MDB_CHECKARG(hmap && data, MDB_HANDLE_INVALID);

    if ((index = freemap_alloc(&hmap->freemap)) == HANDLE_INDEX_INVALID) {
        return MDB_HANDLE_INVALID;
    }

    return index_alloc(&hmap->indextbl, index, data);
}

void *mdb_handle_delete(mdb_handle_map_t *hmap, mdb_handle_t h)
{
    uint32_t  useid = HANDLE_USEID(h);
    int       index = HANDLE_INDEX(h);
    void     *old_data;

    MDB_CHECKARG(hmap && h != MDB_HANDLE_INVALID, NULL);


    if (!(old_data = index_free(&hmap->indextbl, useid,index))) {
        /* errno has been set by index_free() */
        return NULL;
    }

    if (freemap_free(&hmap->freemap, index) < 0) {
        /* errno has been set by freemap_free() */
        return NULL;
    }

    return old_data;
}

void *mdb_handle_get_data(mdb_handle_map_t *hmap, mdb_handle_t h)
{
    uint32_t          useid = HANDLE_USEID(h);
    int               index = HANDLE_INDEX(h);
    indextbl_t       *indextbl;
    indextbl_entry_t *entry;

    MDB_CHECKARG(hmap && h != MDB_HANDLE_INVALID, NULL);

    indextbl = &hmap->indextbl;

    if (index >= indextbl->nentry) {
        errno = EKEYREJECTED;
        return NULL;
    }

    entry = indextbl->entries + index;

    if (entry->useid != useid) {
        errno = ENOANO;
        return NULL;
    }

    if (!entry->data)
        errno = ENODATA;

    return entry->data;
}


int mdb_handle_print(mdb_handle_map_t *hmap, char *buf, int len)
{
    indextbl_t *it;
    char *p, *e;
    int i;

    MDB_CHECKARG(hmap && buf && len > 0, -1);

    it = &hmap->indextbl;
    e = (p = buf) + len;

    p += snprintf(p, e-p, "   useid index data\n");

    for (i = 0;   i < it->nentry && e > p;   i++) {
        indextbl_entry_t *en = it->entries + i;

        if (en->data)
            p += snprintf(p, e-p, "   %5u %5d %p\n",en->useid, i, en->data);
    }

    return p - buf;
}


static mdb_handle_t index_alloc(indextbl_t *indextbl, int index, void *data)
{
#define ALIGN(i,a) ((((i) + (a)-1) / a) * a)

    mdb_handle_t handle;
    int nentry;
    indextbl_entry_t *entries, *entry;
    size_t size;

    MDB_CHECKARG(index >= 0 && (mdb_handle_t)index < HANDLE_INDEX_MAX && data,
                 MDB_HANDLE_INVALID);

    if (index >= indextbl->nentry) {
        nentry  = ALIGN(index + 1, bits_per_bucket);
        size    = sizeof(indextbl_entry_t) * nentry;
        entries = realloc(indextbl->entries, size);

        if (!entries) {
            errno = ENOMEM;
            return MDB_HANDLE_INVALID;
        }

        size = sizeof(indextbl_entry_t) * (nentry - indextbl->nentry);
        memset(entries + indextbl->nentry, 0, size);

        indextbl->nentry  = nentry;
        indextbl->entries = entries;
    }

    entry = indextbl->entries + index;

    if (entry->data && entry->data != data) {
        errno = EBUSY;
        return MDB_HANDLE_INVALID;
    }

    entry->useid += 1;
    entry->data   = data;

    handle = HANDLE_MAKE(entry->useid, index);

    return handle;

#undef ALIGN
}


static void *index_realloc(indextbl_t *indextbl,
                           uint32_t    useid,
                           int         index,
                           void       *data)
{
    indextbl_entry_t *entry;
    void *old_data;

    MDB_CHECKARG(indextbl, NULL);

    if (index < 0 || index >= indextbl->nentry) {
        errno = EKEYREJECTED;
        return NULL;
    }

    entry = indextbl->entries + index;

    if (entry->useid != useid) {
        errno = ENOKEY;
        return NULL;
    }

    if (!(old_data = entry->data)) {
        errno = ENOENT;
        return NULL;
    }


    entry->data = data;

    return old_data;
}

static void *index_free(indextbl_t *indextbl, uint32_t useid, int index)
{
    return index_realloc(indextbl, useid,index, NULL);
}

static int freemap_alloc(freemap_t *freemap)
{
    bucket_t  mask;
    bucket_t *bucket;
    int       nbucket;
    bucket_t *buckets;
    int       bucket_idx;
    int       bit_idx;
    int       index;
    size_t    size;

    for (bucket_idx = 0;   bucket_idx < freemap->nbucket;   bucket_idx++) {
        bucket = freemap->buckets + bucket_idx;

        if (*bucket && (bit_idx = ffsll(*bucket) - 1) >= 0) {
            index = bucket_idx * bits_per_bucket + bit_idx;
            mask  = ~(((bucket_t)1) << bit_idx);
            *bucket &= mask;
            return index;
        }
    }

    index   = bucket_idx * bits_per_bucket;
    nbucket = bucket_idx + 1;
    size    = sizeof(bucket_t) * nbucket;
    buckets = realloc(freemap->buckets, size);

    if (!buckets) {
        errno = ENOMEM;
        return HANDLE_INDEX_INVALID;
    }

    buckets[bucket_idx] = ~((bucket_t)1);

    freemap->nbucket = nbucket;
    freemap->buckets = buckets;

    return index;
}


static int freemap_free(freemap_t *freemap, int index)
{
    int       bucket_idx = index / bits_per_bucket;
    int       bit_idx    = index % bits_per_bucket;
    int       nbucket;
    bucket_t *buckets;
    size_t    size;

    if (freemap && index >= 0 && bucket_idx < freemap->nbucket) {
        freemap->buckets[bucket_idx] |= ((bucket_t)1) << bit_idx;

        if ((bucket_idx + 1 == freemap->nbucket) &&
            freemap->buckets[bucket_idx] == empty_bucket) {
            if (freemap->nbucket == 1) {
                free(freemap->buckets);
                freemap->nbucket = 0;
                freemap->buckets = NULL;
            }
            else {
                nbucket = bucket_idx;
                size    = sizeof(bucket_t) * nbucket;
                buckets = realloc(freemap->buckets, size);

                if (!buckets) {
                    errno = ENOMEM;
                    return -1;
                }

                freemap->buckets = buckets;
            }
        }

        return 0;
    }

    errno = EINVAL;
    return -1;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
