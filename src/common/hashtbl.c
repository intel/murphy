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

#include "murphy/common/mm.h"
#include "murphy/common/list.h"
#include "murphy/common/hashtbl.h"

#define MIN_NBUCKET   8
#define MAX_NBUCKET 128

typedef struct {                        /* a hash bucket */
    mrp_list_hook_t entries;            /* hook to hash table entries */
    mrp_list_hook_t used;               /* hook to list of buckets in use */
} bucket_t;

typedef struct {                        /* a hash table entry */
    mrp_list_hook_t  hook;              /* hook to bucket chain */
    void            *key;               /* key for this entry */
    void            *obj;               /* object for this entry */
} entry_t;

typedef struct {                        /* iterator state */
    mrp_list_hook_t *bp, *bn;           /* current bucket hook pointers */
    mrp_list_hook_t *ep, *en;           /* current entry hook pointers */
    entry_t         *entry;             /* current entry */
    int              verdict;           /* remove-from-cb verdict */
} iter_t;

struct mrp_htbl_s {
    bucket_t           *buckets;        /* hash table buckets */
    size_t              nbucket;        /* this many of them */
    mrp_list_hook_t     used;           /* buckets in use */
    mrp_htbl_comp_fn_t  comp;           /* key comparison function */
    mrp_htbl_hash_fn_t  hash;           /* key hash function */
    mrp_htbl_free_fn_t  free;           /* function to free an entry */
    iter_t             *iter;           /* active iterator state */
};


static size_t calc_buckets(size_t nbucket)
{
    size_t n;

    if (nbucket < MIN_NBUCKET)
        nbucket = MIN_NBUCKET;
    if (nbucket > MAX_NBUCKET)
        nbucket = MAX_NBUCKET;

    for (n = MIN_NBUCKET; n < nbucket; n <<= 1)
        ;

    return n;
}


mrp_htbl_t *mrp_htbl_create(mrp_htbl_config_t *cfg)
{
    mrp_htbl_t *ht;
    size_t     i, nbucket;

    if (cfg->comp && cfg->hash) {
        if ((ht = mrp_allocz(sizeof(*ht))) != NULL) {
            if (cfg->nbucket != 0)
                nbucket = cfg->nbucket;
            else {
                if (cfg->nentry != 0)
                    nbucket = cfg->nentry / 4;
                else
                    nbucket = 4 * MIN_NBUCKET;
            }

            ht->nbucket = calc_buckets(nbucket);
            ht->comp    = cfg->comp;
            ht->hash    = cfg->hash;
            ht->free    = cfg->free;

            mrp_list_init(&ht->used);

            ht->buckets = mrp_allocz(sizeof(*ht->buckets) * ht->nbucket);
            if (ht->buckets != NULL) {
                for (i = 0; i < ht->nbucket; i++) {
                    mrp_list_init(&ht->buckets[i].entries);
                    mrp_list_init(&ht->buckets[i].used);
                }

                return ht;
            }
            else {
                mrp_free(ht);
                ht = NULL;
            }
        }
    }

    return NULL;
}


void mrp_htbl_destroy(mrp_htbl_t *ht, int free)
{
    if (ht != NULL) {
        if (free)
            mrp_htbl_reset(ht, free);

        mrp_free(ht->buckets);
        mrp_free(ht);
    }
}


static inline void free_entry(mrp_htbl_t *ht, entry_t *entry, int free)
{
    if (free && ht->free)
        ht->free(entry->key, entry->obj);
    mrp_free(entry);
}


void mrp_htbl_reset(mrp_htbl_t *ht, int free)
{
    mrp_list_hook_t *bp, *bn, *ep, *en;
    bucket_t        *bucket;
    entry_t         *entry;

    mrp_list_foreach(&ht->used, bp, bn) {
        bucket = mrp_list_entry(bp, bucket_t, used);

        mrp_list_foreach(&bucket->entries, ep, en) {
            entry = mrp_list_entry(ep, entry_t, hook);
            mrp_list_delete(ep);
            free_entry(ht, entry, free);
        }

        mrp_list_delete(&bucket->used);
    }
}


int mrp_htbl_insert(mrp_htbl_t *ht, void *key, void *object)
{
    uint32_t  idx    = ht->hash(key) & (ht->nbucket - 1);
    bucket_t *bucket = ht->buckets + idx;
    int       first  = mrp_list_empty(&bucket->entries);
    entry_t  *entry;

    if ((entry = mrp_allocz(sizeof(*entry))) != NULL) {
        entry->key = key;
        entry->obj = object;
        mrp_list_append(&bucket->entries, &entry->hook);
        if (first)
            mrp_list_append(&ht->used, &bucket->used);

        return TRUE;
    }
    else
        return FALSE;
}


static inline entry_t *lookup(mrp_htbl_t *ht, void *key, bucket_t **bucketp)
{
    uint32_t        idx    = ht->hash(key) & (ht->nbucket - 1);
    bucket_t       *bucket = ht->buckets + idx;
    mrp_list_hook_t *p, *n;
    entry_t        *entry;

    mrp_list_foreach(&bucket->entries, p, n) {
        entry = mrp_list_entry(p, entry_t, hook);

        if (!ht->comp(entry->key, key)) {
            if (bucketp != NULL)
                *bucketp = bucket;
            return entry;
        }
    }

    return NULL;
}


void *mrp_htbl_lookup(mrp_htbl_t *ht, void *key)
{
    entry_t *entry;

    entry = lookup(ht, key, NULL);
    if (entry != NULL)
        return entry->obj;
    else
        return NULL;
}


static void delete_from_bucket(mrp_htbl_t *ht, bucket_t *bucket, entry_t *entry)
{
    mrp_list_hook_t *eh = &entry->hook;


    /*
     * If there is an iterator active and this entry would
     * have been the next one to iterate over, we need to
     * update the iterator to skip to the next entry instead
     * as this one will be removed. Failing to update the
     * iterator could crash mrp_htbl_foreach or drive it into
     * an infinite loop.
     */

    if (ht->iter != NULL && ht->iter->en == eh)
        ht->iter->en = eh->next;

    mrp_list_delete(eh);


    /*
     * If the bucket became empty, unlink it from the used list.
     * If also there is an iterator active and this bucket would
     * have been the next one to iterate over, we need to
     * update the iterator to skip to the next bucket instead
     * as this one just became empty and will be removed from
     * the used bucket list. Failing to update the iterator
     * could drive mrp_htbl_foreach into an infinite loop
     * because of the unexpected hop from the used bucket list
     * (to a single empty bucket).
     */

    if (mrp_list_empty(&bucket->entries)) {
        if (ht->iter != NULL && ht->iter->bn == &bucket->used)
                ht->iter->bn = bucket->used.next;

        mrp_list_delete(&bucket->used);
    }
}


void *mrp_htbl_remove(mrp_htbl_t *ht, void *key, int free)
{
    bucket_t *bucket;
    entry_t  *entry;
    void     *object;

    /*
     * We need to check the found entry and its hash-bucket
     * against any potentially active iterator. Special care
     * needs to be taken if the entry is being iterated over
     * or if the bucket becomes empty and it would be the next
     * bucket to iterate over. The former is taken care of
     * here while the latter is handled in delete_from_bucket.
     */
    if ((entry = lookup(ht, key, &bucket)) != NULL) {
        delete_from_bucket(ht, bucket, entry);
        object = entry->obj;

        if (ht->iter != NULL && entry == ht->iter->entry) /* being iterated */
            ht->iter->verdict = free ? MRP_HTBL_ITER_DELETE : 0;
        else {
            free_entry(ht, entry, free);
        }
    }
    else
        object = NULL;

    return object;
}


int mrp_htbl_foreach(mrp_htbl_t *ht, mrp_htbl_iter_cb_t cb, void *user_data)
{
    iter_t    iter;
    bucket_t *bucket;
    entry_t  *entry;
    int       cb_verdict, ht_verdict;

    /*
     * Now we can only handle a single callback-based iterator.
     * If there is already one we're busy so just bail out.
     */
    if (ht->iter != NULL)
        return FALSE;

    mrp_clear(&iter);
    ht->iter = &iter;

    mrp_list_foreach(&ht->used, iter.bp, iter.bn) {
        bucket = mrp_list_entry(iter.bp, bucket_t, used);

        mrp_list_foreach(&bucket->entries, iter.ep, iter.en) {
            iter.entry = entry = mrp_list_entry(iter.ep, entry_t, hook);
            cb_verdict = cb(entry->key, entry->obj, user_data);
            ht_verdict = iter.verdict;

            /* delete was called from cb (unhashed entry and marked it) */
            if (ht_verdict & MRP_HTBL_ITER_DELETE) {
                free_entry(ht, entry, TRUE);
            }
            else {
                /* cb wants us to unhash (safe even if unhashed in remove) */
                if (cb_verdict & MRP_HTBL_ITER_UNHASH)
                    mrp_list_delete(iter.ep);
                /* cb want us to free entry (and remove was not called) */
                if (cb_verdict & MRP_HTBL_ITER_DELETE)
                    free_entry(ht, entry, TRUE);

                /* cb wants to stop iterating */
                if (!(cb_verdict & MRP_HTBL_ITER_MORE))
                    goto out;
            }
        }
    }

 out:
    ht->iter = NULL;

    return TRUE;
}


void *mrp_htbl_find(mrp_htbl_t *ht, mrp_htbl_find_cb_t cb, void *user_data)
{
    iter_t    iter;
    bucket_t *bucket;
    entry_t  *entry, *found;

    /*
     * Bail out if there is also an iterator active...
     */
    if (ht->iter != NULL)
        return FALSE;

    mrp_clear(&iter);
    ht->iter = &iter;
    found    = NULL;

    mrp_list_foreach(&ht->used, iter.bp, iter.bn) {
        bucket = mrp_list_entry(iter.bp, bucket_t, used);

        mrp_list_foreach(&bucket->entries, iter.ep, iter.en) {
            entry = mrp_list_entry(iter.ep, entry_t, hook);

            if (cb(entry->key, entry->obj, user_data)) {
                found = entry->obj;
                goto out;
            }
        }
    }

 out:
    ht->iter = NULL;

    return found;
}
