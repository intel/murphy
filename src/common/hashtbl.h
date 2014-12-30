/*
 * Copyright (c) 2012 - 2014, Intel Corporation
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

#ifndef __MURPHY_HASHTBL_H__
#define __MURPHY_HASHTBL_H__

#include <stdint.h>

#include <murphy/common/macros.h>
#include <murphy/common/hash-table.h>


MRP_CDECL_BEGIN

/**
 * \addtogroup MurphyCommonInfra
 * @{
 *
 * @file hash-table.h
 *
 * @brief Backward-ccompatibilty layer to provide the original Murphy hash table
 *        implementation API. Please consider directly using directly the new
 *        hash table implementatio @mrp_hashtbl_t. For more details, and the new
 *        API, please refer to hash-table.h.
 * @}
 */

typedef mrp_hashtbl_t mrp_htbl_t;
typedef mrp_comp_fn_t mrp_htbl_comp_fn_t;
typedef mrp_hash_fn_t mrp_htbl_hash_fn_t;
typedef mrp_free_fn_t mrp_htbl_free_fn_t;

typedef struct {
    size_t             nentry;
    mrp_htbl_comp_fn_t comp;
    mrp_htbl_hash_fn_t hash;
    mrp_htbl_free_fn_t free;
    size_t             nbucket;
} mrp_htbl_config_t;

enum {
    MRP_HTBL_ITER_STOP   = 0x0,
    MRP_HTBL_ITER_MORE   = 0x1,
    MRP_HTBL_ITER_UNHASH = 0x2,
    MRP_HTBL_ITER_DELETE = 0x6,
};

typedef int (*mrp_htbl_find_cb_t)(void *key, void *object, void *user_data);
typedef int (*mrp_htbl_iter_cb_t)(void *key, void *object, void *user_data);


static inline mrp_htbl_t *mrp_htbl_create(mrp_htbl_config_t *cfg)
{
    mrp_hashtbl_config_t c;

    if (cfg->nentry > 16384)
        cfg->nentry = 16384;

    mrp_clear(&c);
    c.hash    = cfg->hash;
    c.comp    = cfg->comp;
    c.free    = cfg->free;
    c.nalloc  = cfg->nentry;
    c.nbucket = cfg->nbucket;

    return mrp_hashtbl_create(&c);
}


static inline void mrp_htbl_destroy(mrp_htbl_t *t, int free)
{
    mrp_hashtbl_destroy(t, free ? true : false);
}


static inline void mrp_htbl_reset(mrp_htbl_t *t, int free)
{
    mrp_hashtbl_reset(t, free ? true : false);
}


static inline int mrp_htbl_insert(mrp_htbl_t *t, void *key, void *object)
{
    return mrp_hashtbl_add(t, key, object, NULL) < 0 ? false : true;
}


static inline void *mrp_htbl_remove(mrp_htbl_t *t, void *key, int free)
{
    return mrp_hashtbl_del(t, key, MRP_HASH_COOKIE_NONE, free ? true : false);
}


static inline void *mrp_htbl_lookup(mrp_htbl_t *t, void *key)
{
    return mrp_hashtbl_lookup(t, key, MRP_HASH_COOKIE_NONE);
}


static inline void *mrp_htbl_find(mrp_htbl_t *t, mrp_htbl_find_cb_t cb, void *user_data)
{
    mrp_hashtbl_iter_t  it;
    const void         *key, *obj;

    MRP_HASHTBL_FOREACH(t, &it, &key, NULL, &obj) {
        if (cb((void *)key, (void *)obj, user_data))
            return (void *)obj;
    }

    return NULL;
}


static inline int mrp_htbl_foreach(mrp_htbl_t *t, mrp_htbl_iter_cb_t cb, void *user_data)
{
    mrp_hashtbl_iter_t  it;
    const void         *key, *obj;
    int                 verdict, saved_errno, status;
    bool                release;

    saved_errno = errno;
    errno = 0;
    MRP_HASHTBL_FOREACH(t, &it, &key, NULL, &obj) {
        verdict = cb((void *)key, (void *)obj, user_data);

        if (verdict & MRP_HTBL_ITER_UNHASH) {
            release = !!((verdict & ~MRP_HTBL_ITER_UNHASH) & MRP_HTBL_ITER_DELETE);
            mrp_hashtbl_del(t, key, MRP_HASH_COOKIE_NONE, release);
        }

        if (!(verdict & MRP_HTBL_ITER_MORE))
            break;
    }

    status = (errno == EBUSY ? FALSE : TRUE);
    errno = saved_errno;

    return status;
}


MRP_CDECL_END


#endif /* __MURPHY_HASHTBL_H__ */
