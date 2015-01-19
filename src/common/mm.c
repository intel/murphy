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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <execinfo.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/mm.h>
#include <murphy/common/hashtbl.h>


#define DEFAULT_DEPTH   8                     /* default backtrace depth */
#define MAX_DEPTH     128                     /* max. backtrace depth */

/*
 * memory allocator state
 */

typedef struct {
    mrp_list_hook_t blocks;                   /* list of allocated blocks */
    size_t          hdrsize;                  /* header size */
    int             depth;                    /* backtrace depth */
    uint32_t        cur_blocks;               /* currently allocated blocks */
    uint32_t        max_blocks;               /* max allocated blocks */
    uint64_t        cur_alloc;                /* currently allocated memory */
    uint64_t        max_alloc;                /* max allocated memory */
    int             poison;                   /* poisoning pattern */
    size_t          chunk_size;               /* object pool chunk size */
    mrp_mm_type_t   mode;                     /* passthru/debug mode */

    void *(*alloc)(size_t size, const char *file, int line, const char *func);
    void *(*realloc)(void *ptr, size_t size, const char *file,
                     int line, const char *func);
    int   (*memalign)(void **ptr, size_t align, size_t size,
                      const char *file, int line, const char *func);
    void  (*free)(void *ptr, const char *file, int line, const char *func);
} mm_t;


/*
 * memory block
 */

typedef struct {
    mrp_list_hook_t hook;                     /* to allocated blocks */
    mrp_list_hook_t more;                     /* with the same backtrace */
    const char     *file;                     /* file of immediate caller */
    int             line;                     /* line of immediate caller */
    const char     *func;                     /* name of immediate caller */
    size_t          size;                     /* requested size */
    void           *bt[];                     /* for accessing backtrace */
} memblk_t;



static mm_t __mm = {                          /* allocator state */
    .hdrsize = MRP_ALIGN(MRP_OFFSET(memblk_t, bt[DEFAULT_DEPTH]),
                         MRP_MM_ALIGN),
    .depth   = DEFAULT_DEPTH,
    .poison  = 0xdeadbeef,
};


static const char *get_config_key(const char *config, const char *key)
{
    const char *beg;
    int         len;

    if (config != NULL) {
        len = strlen(key);

        beg = config;
        while (beg != NULL) {
            beg = strstr(beg, key);

            if (beg != NULL) {
                if ((beg == config || beg[-1] == ':') &&
                    (beg[len] == '=' || beg[len] == ':' || beg[len] == '\0'))
                    return (beg[len] == '=' ? beg + len + 1 : "");
                else
                    beg++;
            }
        }
    }

    return NULL;
}


static int32_t get_config_int32(const char *cfg, const char *key,
                                int32_t defval)
{
    const char *v;
    char       *end;
    int         i;

    v = get_config_key(cfg, key);

    if (v != NULL) {
        if (*v) {
            i = strtol(v, &end, 10);

            if (end && (!*end || *end == ':'))
                return i;
        }
    }

    return defval;
}


static uint32_t get_config_uint32(const char *cfg, const char *key,
                                  uint32_t defval)
{
    const char *v;
    char       *end;
    int         i;

    v = get_config_key(cfg, key);

    if (v != NULL) {
        if (*v) {
            i = strtol(v, &end, 10);

            if (end && (!*end || *end == ':'))
                return i;
        }
    }

    return defval;
}


static int get_config_bool(const char *config, const char *key, int defval)
{
    const char *v;

    v = get_config_key(config, key);

    if (v != NULL) {
        if (*v) {

            if ((!strncasecmp(v, "false", 5) && (v[5] == ':' || !v[5])) ||
                (!strncasecmp(v, "true" , 4) && (v[4] == ':' || !v[4])))
                return (v[0] == 't' || v[0] == 'T');
        }
        else if (*v == '\0')
            return TRUE;
    }

    return defval;
}


static int get_config_string(const char *cfg, const char *key,
                             const char *defval, char *buf, size_t size)
{
    const char *v;
    char       *end;
    int         len;

    v = get_config_key(cfg, key);

    if (v == NULL)
        v = defval;

    end = strchr(v, ':');

    if (end != NULL)
        len = end - v;
    else
        len = strlen(v);

    len = snprintf(buf, size, "%*.*s", len, len, v);

    if (len >= (int)size - 1)
        buf[size - 1] = '\0';

    return len;
}



MRP_INIT_AT(101) static void setup(void)
{
    char *config = getenv(MRP_MM_CONFIG_ENVVAR);

    mrp_list_init(&__mm.blocks);

    __mm.depth   = get_config_int32(config, "depth", DEFAULT_DEPTH);

    if (__mm.depth > MAX_DEPTH)
        __mm.depth = MAX_DEPTH;

    __mm.hdrsize = MRP_ALIGN(MRP_OFFSET(memblk_t, bt[__mm.depth]),
                             MRP_MM_ALIGN);

    __mm.cur_blocks = 0;
    __mm.max_blocks = 0;
    __mm.cur_alloc  = 0;
    __mm.max_alloc  = 0;

    __mm.poison     = get_config_uint32(config, "poison", 0xdeadbeef);
    __mm.chunk_size = sysconf(_SC_PAGESIZE) * 2;

    if (config == NULL || !get_config_bool(config, "debug", FALSE))
        mrp_mm_config(MRP_MM_PASSTHRU);
    else
        mrp_mm_config(MRP_MM_DEBUG);
}


static void __attribute__((destructor)) cleanup(void)
{
    if (__mm.mode == MRP_MM_DEBUG) {
        mrp_mm_dump(stdout);
        /*mrp_mm_check(stdout);*/
    }
}



int32_t mrp_mm_config_int32(const char *key, int32_t defval)
{
    return get_config_int32(getenv(MRP_MM_CONFIG_ENVVAR), key, defval);
}


uint32_t mrp_mm_config_uint32(const char *key, uint32_t defval)
{
    return get_config_uint32(getenv(MRP_MM_CONFIG_ENVVAR), key, defval);
}


int mrp_mm_config_bool(const char *key, int defval)
{
    return get_config_bool(getenv(MRP_MM_CONFIG_ENVVAR), key, defval);
}


int mrp_mm_config_string(const char *key, const char *defval,
                         char *buf, size_t size)
{
    return get_config_string(getenv(MRP_MM_CONFIG_ENVVAR), key, defval,
                             buf, size);
}


/*
 * memblk handling
 */

static memblk_t *memblk_alloc(size_t size, const char *file, int line,
                              const char *func, void **bt)
{
    memblk_t *blk;

    if (MRP_UNLIKELY(size == 0))
        blk = NULL;
    else {
        if ((blk = malloc(__mm.hdrsize + size)) != NULL) {
            mrp_list_init(&blk->hook);
            mrp_list_init(&blk->more);
            mrp_list_append(&__mm.blocks, &blk->hook);

            blk->file = file;
            blk->line = line;
            blk->func = func;
            blk->size = size;

            memcpy(blk->bt, bt, __mm.depth * sizeof(*bt));

            __mm.cur_blocks++;
            __mm.cur_alloc += size;

            __mm.max_blocks = MRP_MAX(__mm.max_blocks, __mm.cur_blocks);
            __mm.max_alloc  = MRP_MAX(__mm.max_alloc , __mm.cur_alloc);
        }
    }

    return blk;
}


static void memblk_free(memblk_t *blk, const char *file, int line,
                        const char *func, void **bt)
{
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);
    MRP_UNUSED(bt);

    if (blk != NULL) {
        mrp_list_delete(&blk->hook);

        __mm.cur_blocks--;
        __mm.cur_alloc -= blk->size;

        if (__mm.poison != 0)
            memset(&blk->bt[__mm.depth], __mm.poison, blk->size);

        free(blk);
    }
}


static memblk_t *memblk_resize(memblk_t *blk, size_t size, const char *file,
                               int line, const char *func, void **bt)
{
    memblk_t *resized;

    if (blk != NULL) {
        mrp_list_delete(&blk->hook);

        if (size != 0) {
            resized = realloc(blk, __mm.hdrsize + size);

            if (resized != NULL) {
                mrp_list_init(&resized->hook);

                mrp_list_append(&__mm.blocks, &resized->hook);

                __mm.cur_alloc -= resized->size;
                __mm.cur_alloc += size;
                __mm.max_alloc  = MRP_MAX(__mm.max_alloc, __mm.cur_alloc);

                resized->file = file;
                resized->line = line;
                resized->func = func;

                memcpy(resized->bt, bt, __mm.depth * sizeof(*bt));

                resized->size = size;
            }
            else
                mrp_list_append(&__mm.blocks, &blk->hook);
        }
        else {
            resized = NULL;
            memblk_free(blk, file, line, func, bt);
        }

        return resized;
    }
    else
        return memblk_alloc(size, file, line, func, bt);
}


static inline void *memblk_to_ptr(memblk_t *blk)
{
    if (blk != NULL)
        return (void *)&blk->bt[__mm.depth];
    else
        return NULL;
}


static inline memblk_t *ptr_to_memblk(void *ptr)
{
    /*
     * XXX Hmm... maybe we should also add pre- and post-sentinels
     * and check them here to have minimal protection/detection of
     * trivial buffer overflow bugs when running in debug mode.
     */

    if (ptr != NULL)
        return ptr - MRP_OFFSET(memblk_t, bt[__mm.depth]);
    else
        return NULL;
}


/*
 * debugging allocator
 */

static inline int __mm_backtrace(void **bt, size_t size)
{
    int i, n;

    n = backtrace(bt, (int)size);
    for (i = n; i < (int)size; i++)
        bt[i] = NULL;

    return n;
}


static void *__mm_alloc(size_t size, const char *file, int line,
                        const char *func)
{
    memblk_t *blk;
    void     *bt[__mm.depth + 1];

    __mm_backtrace(bt, MRP_ARRAY_SIZE(bt));
    blk = memblk_alloc(size, file, line, func, bt + 1);

    return memblk_to_ptr(blk);
}


static void *__mm_realloc(void *ptr, size_t size, const char *file,
                          int line, const char *func)
{
    memblk_t *blk;
    void     *bt[__mm.depth + 1];

    __mm_backtrace(bt, MRP_ARRAY_SIZE(bt));
    blk = ptr_to_memblk(ptr);

    if (blk != NULL)
        blk = memblk_resize(blk, size, file, line, func, bt + 1);
    else
        blk = memblk_alloc(size, file, line, func, bt + 1);

    return memblk_to_ptr(blk);
}


static int __mm_memalign(void **ptr, size_t align, size_t size,
                         const char *file, int line, const char *func)
{
    MRP_UNUSED(align);
    MRP_UNUSED(size);
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    *ptr  = NULL;
    errno = ENOSYS;

    mrp_log_error("XXX %s not implemented!!!", __FUNCTION__);
    return -1;
}


static void __mm_free(void *ptr, const char *file, int line,
                      const char *func)
{
    memblk_t *blk;
    void     *bt[__mm.depth + 1];

    if (ptr != NULL) {
        __mm_backtrace(bt, MRP_ARRAY_SIZE(bt));
        blk = ptr_to_memblk(ptr);

        if (blk != NULL)
            memblk_free(blk, file, line, func, bt + 1);
    }
}


/*
 * passthru allocator
 */

static void *__passthru_alloc(size_t size, const char *file, int line,
                              const char *func)
{
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    if (MRP_UNLIKELY(size == 0))
        return NULL;
    else
        return malloc(size);
}


static void *__passthru_realloc(void *ptr, size_t size, const char *file,
                                int line, const char *func)
{
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    return realloc(ptr, size);
}


static int __passthru_memalign(void **ptr, size_t align, size_t size,
                               const char *file, int line, const char *func)
{
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    return posix_memalign(ptr, align, size);
}


static void __passthru_free(void *ptr, const char *file, int line,
                            const char *func)
{
    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    free(ptr);
}


/*
 * common public interface - uses either passthru or debugging
 */

void *mrp_mm_alloc(size_t size, const char *file, int line, const char *func)
{
    return __mm.alloc(size, file, line, func);
}


void *mrp_mm_realloc(void *ptr, size_t size, const char *file, int line,
                     const char *func)
{
    return __mm.realloc(ptr, size, file, line, func);
}


char *mrp_mm_strdup(const char *s, const char *file, int line, const char *func)
{
    char   *p;
    size_t  size;

    if (s != NULL) {
        size = strlen(s) + 1;
        p    = mrp_mm_alloc(size, file, line, func);

        if (p != NULL)
            strcpy(p, s);
    }
    else
        p = NULL;

    return p;
}


int mrp_mm_memalign(void **ptr, size_t align, size_t size, const char *file,
                    int line, const char *func)
{
    return __mm.memalign(ptr, align, size, file, line, func);
}


void mrp_mm_free(void *ptr, const char *file, int line, const char *func)
{
    return __mm.free(ptr, file, line, func);
}


int mrp_mm_config(mrp_mm_type_t type)
{
    if (__mm.cur_blocks != 0)
        return FALSE;

    switch (type) {
    case MRP_MM_PASSTHRU:
        __mm.alloc    = __passthru_alloc;
        __mm.realloc  = __passthru_realloc;
        __mm.memalign = __passthru_memalign;
        __mm.free     = __passthru_free;
        __mm.mode     = MRP_MM_PASSTHRU;
        return TRUE;

    case MRP_MM_DEBUG:
        __mm.alloc    = __mm_alloc;
        __mm.realloc  = __mm_realloc;
        __mm.memalign = __mm_memalign;
        __mm.free     = __mm_free;
        __mm.mode     = MRP_MM_DEBUG;
        return TRUE;

    default:
        mrp_log_error("Invalid memory allocator type 0x%x requested.", type);
        return FALSE;
    }
}


#define NBUCKET 1024

static int btcmp(void **bt1, void **bt2)
{
    ptrdiff_t diff;
    int       i;

    for (i = 0; i < __mm.depth; i++) {
        diff = bt1[i] - bt2[i];

        if (diff < 0)
            return -1;
        else if (diff > 0)
            return +1;
    }

    return 0;
}


static uint32_t blkhash(memblk_t *blk)
{
    uint32_t h;
    int      i;

    h = 0;
    for (i = 0; i < __mm.depth; i++)
        h ^= (blk->bt[i] - NULL) & 0xffffffffUL;

    return h % NBUCKET;
}


static memblk_t *blkfind(mrp_list_hook_t *buckets, memblk_t *blk)
{
    uint32_t         h    = blkhash(blk);
    mrp_list_hook_t *head = buckets + h;
    mrp_list_hook_t *p, *n;
    memblk_t        *b;

    mrp_list_foreach(head, p, n) {
        b = mrp_list_entry(p, typeof(*b), hook);
        if (!btcmp(&b->bt[0], &blk->bt[0]))
            return b;
    }

    return NULL;
}


static void collect_blocks(mrp_list_hook_t *buckets)
{
    mrp_list_hook_t *p, *n;
    memblk_t        *head, *blk;
    uint32_t         h;
    int              i;

    for (i = 0; i < NBUCKET; i++)
        mrp_list_init(buckets + i);

    mrp_list_foreach(&__mm.blocks, p, n) {
        blk = mrp_list_entry(p, typeof(*blk), hook);

        mrp_list_init(&blk->more);
        head = blkfind(buckets, blk);

        if (head != NULL) {
            mrp_list_append(&head->more, &blk->more);
            head->size += blk->size;
        }
        else {
            h = blkhash(blk);
            mrp_list_delete(&blk->hook);
            mrp_list_append(buckets + h, &blk->hook);
        }
    }
}


static uint32_t group_usage(memblk_t *head, int exclude_head)
{
    mrp_list_hook_t *p, *n;
    memblk_t        *blk;
    uint32_t         total;

    total = exclude_head ? 0 : head->size;
    mrp_list_foreach(&head->more, p, n) {
        blk = mrp_list_entry(p, typeof(*blk), more);
        total += blk->size;
    }

    return total;
}


static void dump_group(FILE *fp, memblk_t *head)
{
    mrp_list_hook_t  *p, *n;
    memblk_t         *blk;
    char            **syms, *sym;
    uint32_t          total;
    int               nblk, i;

    fprintf(fp, "Allocations with call stack fingerprint:\n");
    syms = backtrace_symbols(head->bt, __mm.depth);
    for (i = 0; i < __mm.depth && head->bt[i]; i++) {
        sym = syms && syms[i] ? strrchr(syms[i], '/') : NULL;
        fprintf(fp, "    %p (%s)\n", head->bt[i], sym ? sym + 1 : "<unknown>");
    }
    free(syms);

    total = head->size - group_usage(head, TRUE);
    nblk  = 1;

    fprintf(fp, "        %lu bytes at %p\n", (unsigned long)total,
            memblk_to_ptr(head));

    mrp_list_foreach(&head->more, p, n) {
        blk = mrp_list_entry(p, typeof(*blk), more);

        total += blk->size;
        nblk++;

        fprintf(fp, "        %zd bytes at %p\n", blk->size, memblk_to_ptr(blk));
    }

    if (nblk > 1)
        fprintf(fp, "    total %lu bytes in %d blocks\n",
                (unsigned long)total, nblk);
}


static void sort_blocks(mrp_list_hook_t *buckets, mrp_list_hook_t *sorted)
{
    mrp_list_hook_t *bp, *bn, *sp, *sn;
    memblk_t        *head, *entry, *next;
    int              i;

    mrp_list_init(sorted);

    for (i = 0; i < NBUCKET; i++) {
        mrp_list_foreach(buckets + i, bp, bn) {
            head = mrp_list_entry(bp, typeof(*head), hook);

            next = NULL;
            mrp_list_foreach(sorted, sp, sn) {
                entry = mrp_list_entry(sp, typeof(*entry), hook);

                if (head->size <= entry->size) {
                    next = entry;
                    break;
                }
            }

            mrp_list_delete(&head->hook);

            if (next != NULL)
                mrp_list_insert_before(&next->hook, &head->hook);
            else
                mrp_list_append(sorted, &head->hook);
        }
    }
}


static void dump_blocks(FILE *fp, mrp_list_hook_t *sorted)
{
    mrp_list_hook_t *p, *n;
    memblk_t        *head;

    mrp_list_foreach(sorted, p, n) {
        head = mrp_list_entry(p, typeof(*head), hook);
        dump_group(fp, head);
    }
}


static void relink_blocks(mrp_list_hook_t *sorted)
{
    mrp_list_hook_t *p, *n;
    memblk_t        *head;
    uint32_t         rest;

    mrp_list_foreach(sorted, p, n) {
        head = mrp_list_entry(p, typeof(*head), hook);
        mrp_list_delete(&head->hook);
        mrp_list_append(&__mm.blocks, &head->hook);

        rest = group_usage(head, TRUE);
        head->size -= rest;
    }
}


void mrp_mm_dump(FILE *fp)
{
    mrp_list_hook_t buckets[NBUCKET];
    mrp_list_hook_t sorted;

    mrp_list_init(&sorted);

    collect_blocks(buckets);
    sort_blocks(buckets, &sorted);
    dump_blocks(fp, &sorted);
    relink_blocks(&sorted);

    fprintf(fp, "Max: %llu bytes (%.2f M, %.2f G), %ld blocks\n",
            (unsigned long long)__mm.max_alloc,
            1.0 * __mm.max_alloc / (1024 * 1024),
            1.0 * __mm.max_alloc / (1024 * 1024 * 1024),
            (unsigned long)__mm.max_blocks);
    fprintf(fp, "Current: %llu bytes (%.2f M, %.2f G) in %ld blocks.\n",
            (unsigned long long)__mm.cur_alloc,
            1.0 * __mm.cur_alloc / (1024 * 1024),
            1.0 * __mm.cur_alloc / (1024 * 1024 * 1024),
            (unsigned long)__mm.cur_blocks);
}


void mrp_mm_check(FILE *fp)
{
    mrp_mm_dump(fp);
}





/*
 * object pool interface
 */

typedef unsigned int mask_t;

#define W sizeof(mask_t)
#define B (W * 8)
#define MASK_BYTES (sizeof(mask_t))
#define MASK_BITS  (MASK_BYTES * 8)
#define MASK_EMPTY ((mask_t)-1)
#define MASK_FULL  ((mask_t) 0)

typedef struct pool_chunk_s pool_chunk_t;

static int pool_calc_sizes(mrp_objpool_t *pool);
static int pool_grow(mrp_objpool_t *pool, int nobj);
static int pool_shrink(mrp_objpool_t *pool, int nobj);
static pool_chunk_t *chunk_alloc(int nperchunk);
static void chunk_free(pool_chunk_t *chunk);
static inline int chunk_empty(pool_chunk_t *chunk);
static void pool_foreach_object(mrp_objpool_t *pool,
                                void (*cb)(void *obj, void *user_data),
                                void *user_data);
static void chunk_foreach_object(pool_chunk_t *chunk,
                                 void (*cb)(void *obj, void *user_data),
                                 void *user_data);


/*
 * an object pool
 */

struct mrp_objpool_s {
    char             *name;                      /* verbose pool name */
    size_t            limit;                     /* max. number of objects */
    size_t            objsize;                   /* size of a single object */
    size_t            prealloc;                  /* preallocate this many */
    size_t            nobj;                      /* currently allocated */
    int             (*setup)(void *);            /* object setup callback */
    void            (*cleanup)(void *);          /* object cleanup callback */
    uint32_t          flags;                     /* pool flags */
    int               poison;                    /* poisoning pattern */

    size_t            nperchunk;                 /* objects per chunk */
    size_t            dataidx;                   /* data  */
    mrp_list_hook_t   space;                     /* chunk with frees slots */
    size_t            nspace;                    /* number of such chunks */
    mrp_list_hook_t   full;                      /* fully allocated chunks */
    size_t            nfull;                     /* number of such chunks */
};


/*
 * a chunk of memory allocated to an object pool
 */

struct pool_chunk_s {
    mrp_objpool_t   *pool;                       /* pool we're alloced to */
    mrp_list_hook_t  hook;                       /* hook to chunk list */
    mask_t           cache;                      /* cache bits */
    mask_t           used[];                     /* allocation mask */
};



mrp_objpool_t *mrp_objpool_create(mrp_objpool_config_t *cfg)
{
    mrp_objpool_t *pool;

    if ((pool = mrp_allocz(sizeof(*pool))) != NULL) {
        if ((pool->name = mrp_strdup(cfg->name)) == NULL)
            goto fail;

        pool->limit    = cfg->limit;
        pool->objsize  = MRP_MAX(cfg->objsize, (size_t)MRP_MM_OBJSIZE_MIN);
        pool->prealloc = cfg->prealloc;
        pool->setup    = cfg->setup;
        pool->cleanup  = cfg->cleanup;
        pool->flags    = cfg->flags;
        pool->poison   = cfg->poison;

        mrp_list_init(&pool->space);
        mrp_list_init(&pool->full);
        pool->nspace = 0;
        pool->nfull  = 0;

        if (!pool_calc_sizes(pool))
            goto fail;

        if (!mrp_objpool_grow(pool, pool->prealloc))
            goto fail;

        mrp_debug("pool <%s> created, with %zd/%zd objects.", pool->name,
                  pool->prealloc, pool->limit);

        return pool;
    }


 fail:
    mrp_objpool_destroy(pool);
    return NULL;
}


static void free_object(void *obj, void *user_data)
{
    mrp_objpool_t *pool = (mrp_objpool_t *)user_data;

    printf("Releasing unfreed object %p from pool <%s>.\n", obj, pool->name);
    mrp_objpool_free(obj);
}


void mrp_objpool_destroy(mrp_objpool_t *pool)
{
    if (pool != NULL) {
        if (pool->cleanup != NULL)
            pool_foreach_object(pool, free_object, pool);

        mrp_free(pool->name);
        mrp_free(pool);
    }
}


void *mrp_objpool_alloc(mrp_objpool_t *pool)
{
    pool_chunk_t *chunk;
    void         *obj;
    unsigned int  cidx, uidx, sidx;

    if (pool->limit && pool->nobj >= pool->limit)
        return NULL;

    if (mrp_list_empty(&pool->space)) {
        if (!pool_grow(pool, 1))
            return NULL;
    }

    chunk = mrp_list_entry(pool->space.next, pool_chunk_t, hook);
    cidx  = ffs(chunk->cache);

    if (!cidx) {
        mrp_log_error("object pool bug: no free slots in cache mask.");
        return NULL;
    }
    else
        cidx--;

    uidx = ffs(chunk->used[cidx]);

    if (!uidx) {
        mrp_log_error("object pool bug: no free slots in used mask.");
        return NULL;
    }
    else
        uidx--;

    sidx = cidx * MASK_BITS + uidx;
    obj  = ((void *)&chunk->used[pool->dataidx]) + (sidx * pool->objsize);

    mrp_debug("%p: %u/%u: %u, offs %zd\n", obj, cidx, uidx, sidx,
              sidx * pool->objsize);

    chunk->used[cidx] &= ~(1 << uidx);

    if (chunk->used[cidx] == MASK_FULL) {
        chunk->cache &= ~(1 << cidx);

        if (chunk->cache == MASK_FULL) {          /* chunk exhausted */
            mrp_list_delete(&chunk->hook);
            pool->nspace--;
            mrp_list_append(&pool->full, &chunk->hook);
            pool->nfull++;
        }
    }

    if (pool->setup == NULL || pool->setup(obj)) {
        pool->nobj++;
        return obj;
    }
    else {
        mrp_objpool_free(obj);
        return NULL;
    }
}


void mrp_objpool_free(void *obj)
{
    pool_chunk_t  *chunk;
    mrp_objpool_t *pool;
    unsigned int   cidx, uidx, sidx;
    mask_t         cache, used;
    void          *base;

    if (obj == NULL)
        return;

    chunk = (pool_chunk_t *)(((ptrdiff_t)obj) & ~(__mm.chunk_size - 1));
    pool  = chunk->pool;

    base = (void *)&chunk->used[pool->dataidx];
    sidx = (obj - base) / pool->objsize;
    cidx = sidx / MASK_BITS;
    uidx = sidx & (MASK_BITS - 1);

    mrp_debug("%p: %u/%u: %u, offs %zd\n", obj, cidx, uidx, sidx,
              sidx * pool->objsize);

    cache = chunk->cache;
    used  = chunk->used[cidx];

    if (used & (1 << uidx)) {
        mrp_log_error("Trying to free unallocated object %p of pool <%s>.",
                      obj, pool->name);
        return;
    }

    if (pool->cleanup != NULL)
        pool->cleanup(obj);

    if (pool->flags & MRP_OBJPOOL_FLAG_POISON)
        memset(obj, pool->poison, pool->objsize);

    chunk->used[cidx] |= (1 << uidx);
    chunk->cache      |= (1 << cidx);

    if (cache == MASK_FULL) {                    /* chunk was full */
        mrp_list_delete(&chunk->hook);
        pool->nfull--;
        mrp_list_append(&pool->space, &chunk->hook);
        pool->nspace++;
    }

    pool->nobj--;
}


int mrp_objpool_grow(mrp_objpool_t *pool, int nobj)
{
    int nchunk = (nobj + pool->nperchunk - 1) / pool->nperchunk;

    return pool_grow(pool, nchunk) == nchunk;
}


int mrp_objpool_shrink(mrp_objpool_t *pool, int nobj)
{
    int nchunk = (nobj + pool->nperchunk - 1) / pool->nperchunk;

    return pool_shrink(pool, nchunk) == nchunk;
}


static int pool_calc_sizes(mrp_objpool_t *pool)
{
    size_t S, C, Hf, Hv, P;
    size_t n, T;

    if (!pool->objsize)
        return FALSE;

    pool->objsize = MRP_ALIGN(pool->objsize, MRP_MM_ALIGN);

    /*
     * Pool chunks consist of an administrative header followed by object
     * slots each of which can be either claimed/allocated or free. The
     * header contains a back pointer to the pool, a hook to one of the
     * chunk lists and a two-level bit-mask for slot allocation status.
     * The two-level mask consists of a 32-bit cache word and actual slot
     * status words. The nth bit of the cache word caches whether there are
     * any free among the nth - (n + 31)th slots. The slot status words keep
     * the status of the actual slots. To find a free slot we find the idx
     * of the 1st word with a free slot from the cache and then the free
     * slot index in that word. To be able to use FFS we use inverted bit
     * semantics (0=allocated, 1=free) and we populate the words starting
     * at the LSB.
     *
     * Here we calculate how many objects we'll be able to squeeze into a
     * single pool chunk and how many mask bits we'll need to administer
     * the status of these. To do this we use the following equations:
     *
     *     1) Hf + Hv + n * S = C
     *     2) Hv = W + (n + B - 1) / B * W
     * where
     *     C: chunk size
     *     S: object size (aligned to our minimum alignment)
     *    Hf: header size, fixed part
     *    Hv: Header size, variable part (allocation mask)
     *     n: number of objects / chunk
     *     W: bitmask word size in bytes
     *     B: bitmask word size in bits
     *
     * Solving the equations for n gives us
     *     n = (B*C - B*Hf - W*(2*B - 1)) / (B*S + W)
     *
     * If any, the only non-obvious thing below is that instead of trying
     * to express padding as part of the equation system (which seems to be
     * way beyond my abilities in math nowadays), we initally assume no
     * padding then check and compensate for it in the end if necessary.
     */

    Hf = sizeof(pool_chunk_t);
    C  = __mm.chunk_size;
    P  = 0;

    S  = MRP_ALIGN(pool->objsize, MRP_MM_ALIGN);
    n  = (B * C - B * Hf - W * (2*B - 1)) / (B * S + W);
    Hv = W + W * (n + B - 1) / B;

    P = (Hf + Hv) % sizeof(void *);
    if (P != 0) {
        P = sizeof(void *) - P;

        if (Hv + Hf + P + n * S > C) {
            n--;
            Hv = W + W * (n + B - 1) / B;
        }
    }

    T  = Hf + Hv + P + n * S;

    if (T > C) {
        mrp_log_error("Could not size pool '%s' properly.", pool->name);
        return FALSE;
    }

    pool->nperchunk = n;
    pool->dataidx   = (n + B - 1) / B;

    if (pool->limit && (pool->limit % pool->nperchunk) != 0)
        pool->limit += (pool->nperchunk - (pool->limit % pool->nperchunk));

    return TRUE;
}


static int pool_grow(mrp_objpool_t *pool, int nchunk)
{
    pool_chunk_t *chunk;
    int           cnt;

    for (cnt = 0; cnt < nchunk; cnt++) {
        chunk = chunk_alloc(pool->nperchunk);

        if (chunk != NULL) {
            chunk->pool = pool;
            mrp_list_append(&pool->space, &chunk->hook);
            pool->nspace++;
        }
        else
            break;
    }

    return cnt;
}


static int pool_shrink(mrp_objpool_t *pool, int nchunk)
{
    mrp_list_hook_t *p, *n;
    pool_chunk_t    *chunk;
    int              cnt;

    cnt = 0;
    mrp_list_foreach(&pool->space, p, n) {
        chunk = mrp_list_entry(p, pool_chunk_t, hook);

        if (chunk_empty(chunk)) {
            mrp_list_delete(&chunk->hook);
            chunk_free(chunk);
            pool->nspace--;
            cnt++;
        }

        if (cnt >= nchunk)
            break;
    }

    return cnt;
}


static void pool_foreach_object(mrp_objpool_t *pool,
                                void (*cb)(void *obj, void *user_data),
                                void *user_data)
{
    mrp_list_hook_t *p, *n;
    pool_chunk_t    *chunk;

    mrp_list_foreach(&pool->full, p, n) {
        chunk = mrp_list_entry(p, pool_chunk_t, hook);
        chunk_foreach_object(chunk, cb, user_data);
    }

    mrp_list_foreach(&pool->space, p, n) {
        chunk = mrp_list_entry(p, pool_chunk_t, hook);
        chunk_foreach_object(chunk, cb, user_data);
    }
}


static void chunk_foreach_object(pool_chunk_t *chunk,
                                 void (*cb)(void *obj, void *user_data),
                                 void *user_data)
{
    mrp_objpool_t *pool = chunk->pool;
    void          *obj;
    int            sidx, cidx, uidx;
    mask_t         used;

    sidx = 0;
    while (sidx < (int)pool->nperchunk) {
        cidx = sidx / MASK_BITS;
        uidx = sidx & (MASK_BITS - 1);
        used = chunk->used[cidx];

        if (!(used & (1 << uidx))) {
            obj = ((void *)&chunk->used[pool->dataidx]) + (sidx*pool->objsize);
            cb(obj, user_data);
            sidx++;
        }
        else {
            if (used == MASK_EMPTY)
                sidx = (sidx + MASK_BITS) & ~(MASK_BITS - 1);
            else
                sidx++;
        }
    }
}


static inline int chunk_empty(pool_chunk_t *chunk)
{
    mask_t mask;
    int    i, n;

    if (chunk->cache != (MASK_EMPTY & 0xffff))
        return FALSE;
    else {
        for (n = chunk->pool->nperchunk, i = 0; n > 0; n -= MASK_BITS, i++) {
            if (n >= (int)MASK_BITS)
                mask = MASK_EMPTY;
            else
                mask = (1 << n) - 1;

            if ((chunk->used[i] & mask) != mask)
                return FALSE;
        }

        return TRUE;
    }
}


static void chunk_init(pool_chunk_t *chunk, int nperchunk)
{
    int nword, left, i;

    mrp_list_init(&chunk->hook);

    left  = nperchunk;
    nword = (nperchunk + MASK_BITS - 1) / MASK_BITS;

    /*
     * initialize allocation bitmask
     *
     * Note that every bit that corresponds to a non-allocatable slots
     * we mark as reserved. This keeps both the allocation and freeing
     * code paths simpler.
     */

    chunk->cache = (1 << nword) - 1;

    for (i = 0; left > 0; i++) {
        if (left >= (int)MASK_BITS)
            chunk->used[i] = MASK_EMPTY;
        else
            chunk->used[i] = (((mask_t)1) << left) - 1;

        left -= B;
    }
}


static pool_chunk_t *chunk_alloc(int nperchunk)
{
    void *chunk;
    int   err;

    err = posix_memalign(&chunk, __mm.chunk_size, __mm.chunk_size);

    if (err == 0) {
        memset(chunk, 0, __mm.chunk_size);
        chunk_init((pool_chunk_t *)chunk, nperchunk);
    }
    else
        chunk = NULL;

    return chunk;
}


static void chunk_free(pool_chunk_t *chunk)
{
    free(chunk);
}


#if 0
static void test_sizes(void)
{
    size_t S, C, Hf, Hv, P;
    size_t i, n, T, Hv1, n1, T1;
    int    ok, ok1;

    Hf = sizeof(pool_chunk_t);
    C  = __mm.chunk_size;
    P  = 0;

    printf(" C: %zd\n", C);
    printf("Hf: %zd\n", Hf);

    for (i = 1; i < __mm.chunk_size / 8; i++) {
        S  = MRP_ALIGN(i, MRP_MM_ALIGN);
        n  = (B * C - B * Hf - W * (2*B - 1)) / (B * S + W);
        Hv = W + W * (n + B - 1) / B;

        P = (Hf + Hv) % sizeof(void *);
        if (P != 0) {
            P = sizeof(void *) - P;

            if (Hv + Hf + P + n * S > C) {
                n--;
                Hv = W + W * (n + B - 1) / B;
            }
        }

        T  = Hf + Hv + P + n * S;
        ok = T <= C;

        n1  = n + 1;
        Hv1 = W + W * (n1 + B - 1) / B;
        T1  = Hf + Hv1 + P + n1 * S;
        ok1 = T1 > C;

        printf("  i = %zd: %zd * %zd + %zd (%zd: %s, %zd: %s)\n", i, n, S, P,
               T , ok  ? "OK" : "FAIL",
               T1, ok1 ? "OK" : "FAIL");
        {
            size_t hs, us;

            us = sizeof(uint32_t);
            hs = (Hf + Hv + P) / us;

            printf("  H+P: %zd (%zd * %zd = %zd)\n", Hf + Hv + P,
                   hs, us, hs * us);

            if (((Hf + Hv + P) % sizeof(void *)) != 0) {
                printf("Padding error!\n");
                exit(1);
            }
        }

        if (!ok || !ok1)
            exit(1);
    }
}
#endif
