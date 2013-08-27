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

#ifndef __MURPHY_MM_H__
#define __MURPHY_MM_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

#define MRP_MM_ALIGN 8                       /* object alignment */
#define MRP_MM_CONFIG_ENVVAR "__MURPHY_MM_CONFIG"

#define mrp_alloc(size)        mrp_mm_alloc((size), __LOC__)
#define mrp_free(ptr)          mrp_mm_free((ptr), __LOC__)
#define mrp_strdup(s)          mrp_mm_strdup((s), __LOC__)
#define mrp_datadup(ptr, size) ({                                         \
            typeof(ptr) _ptr = mrp_alloc(size);                           \
                                                                          \
            if (_ptr != NULL)                                             \
                memcpy(_ptr, ptr, size);                                  \
                                                                          \
            _ptr;                                                         \
        })

#define mrp_allocz(size) ({                                               \
            void *_ptr;                                                   \
                                                                          \
            if ((_ptr = mrp_mm_alloc(size, __LOC__)) != NULL)             \
                memset(_ptr, 0, size);                                    \
                                                                          \
            _ptr;})

#define mrp_calloc(n, size) mrp_allocz((n) * (size))

#define mrp_reallocz(ptr, o, n) ({                                        \
            typeof(ptr) _ptr;                                             \
            typeof(o)   _o;                                               \
            typeof(n)   _n    = (n);                                      \
            size_t      _size = sizeof(*_ptr) * (_n);                     \
                                                                          \
            if ((ptr) != NULL)                                            \
                _o = o;                                                   \
            else                                                          \
                _o = 0;                                                   \
                                                                          \
            _ptr = (typeof(ptr))mrp_mm_realloc(ptr, _size, __LOC__);      \
            if (_ptr != NULL || _n == 0) {                                \
                if ((unsigned)(_n) > (unsigned)(_o))                      \
                    memset(_ptr + (_o), 0,                                \
                           ((_n)-(_o)) * sizeof(*_ptr));                  \
                ptr = _ptr;                                               \
            }                                                             \
            _ptr; })

#define mrp_realloc(ptr, size) ({                                         \
            typeof(ptr) _ptr;                                             \
            size_t      _size = size;                                     \
                                                                          \
            _ptr = (typeof(ptr))mrp_mm_realloc(ptr, _size, __LOC__);      \
            if (_ptr != NULL || _size == 0)                               \
                ptr = _ptr;                                               \
                                                                          \
            _ptr; })

#define mrp_clear(obj) memset((obj), 0, sizeof(*(obj)))


#define mrp_alloc_array(type, n)  ((type *)mrp_alloc(sizeof(type) * (n)))
#define mrp_allocz_array(type, n) ((type *)mrp_allocz(sizeof(type) * (n)))

typedef enum {
    MRP_MM_PASSTHRU = 0,                 /* passthru allocator */
    MRP_MM_DEFAULT  = MRP_MM_PASSTHRU,   /* default is passthru */
    MRP_MM_DEBUG                         /* debugging allocator */
} mrp_mm_type_t;


int mrp_mm_config(mrp_mm_type_t type);
void mrp_mm_check(FILE *fp);
void mrp_mm_dump(FILE *fp);

void *mrp_mm_alloc(size_t size, const char *file, int line, const char *func);
void *mrp_mm_realloc(void *ptr, size_t size, const char *file, int line,
                     const char *func);
char *mrp_mm_strdup(const char *s, const char *file, int line,
                    const char *func);
int mrp_mm_memalign(void **ptr, size_t align, size_t size, const char *file,
                    int line, const char *func);
void mrp_mm_free(void *ptr, const char *file, int line, const char *func);




#define MRP_MM_OBJSIZE_MIN 16                    /* minimum object size */

enum {
    MRP_OBJPOOL_FLAG_POISON = 0x1,               /* poison free'd objects */
};


/*
 * object pool configuration
 */

typedef struct {
    char      *name;                             /* verbose pool name */
    size_t     limit;                            /* max. number of objects */
    size_t     objsize;                          /* size of a single object */
    size_t     prealloc;                         /* preallocate this many */
    int      (*setup)(void *);                   /* object setup callback */
    void     (*cleanup)(void *);                 /* object cleanup callback */
    uint32_t   flags;                            /* MRP_OBJPOOL_FLAG_* */
    int        poison;                           /* poisoning pattern */
} mrp_objpool_config_t;


typedef struct mrp_objpool_s mrp_objpool_t;

/** Create a new object pool with the given configuration. */
mrp_objpool_t *mrp_objpool_create(mrp_objpool_config_t *cfg);

/** Destroy an object pool, freeing all associated memory. */
void mrp_objpool_destroy(mrp_objpool_t *pool);

/** Allocate a new object from the pool. */
void *mrp_objpool_alloc(mrp_objpool_t *pool);

/** Free the given object. */
void mrp_objpool_free(void *obj);

/** Grow @pool to accomodate @nobj new objects. */
int mrp_objpool_grow(mrp_objpool_t *pool, int nobj);

/** Shrink @pool by @nobj new objects, if possible. */
int mrp_objpool_shrink(mrp_objpool_t *pool, int nobj);

/** Get the value of a boolean key from the configuration. */
int mrp_mm_config_bool(const char *key, int defval);

/** Get the value of a boolean key from the configuration. */
int32_t mrp_mm_config_int32(const char *key, int32_t defval);

/** Get the value of a boolean key from the configuration. */
uint32_t mrp_mm_config_uint32(const char *key, uint32_t defval);

/** Get the value of a string key from the configuration. */
int mrp_mm_config_string(const char *key, const char *defval,
                         char *buf, size_t size);

MRP_CDECL_END

#endif /* __MURPHY_MM_H__ */

