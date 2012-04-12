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
#define mrp_calloc(n, size)    mrp_mm_alloc((n) * (size), __LOC__)
#define mrp_realloc(ptr, size) mrp_mm_realloc((ptr), (size), __LOC__)
#define mrp_free(ptr)          mrp_mm_free((ptr), __LOC__)
#define mrp_strdup(s)          mrp_mm_strdup((s), __LOC__)

#define mrp_allocz(size) ({					\
	    void *_ptr;						\
	    							\
	    if ((_ptr = mrp_mm_alloc(size, __LOC__)) != NULL)	\
		memset(_ptr, 0, size);				\
	    							\
	    _ptr;})

#define mrp_reallocz(ptr, o, n) ({					\
            typeof(ptr) __ptr;                                          \
            size_t      __size = sizeof(*__ptr) * (n);			\
	    typeof(n)   __n    = (n);					\
	    typeof(o)   __o;						\
                                                                        \
            if ((ptr) != NULL)						\
		__o = o;						\
	    else							\
		__o = 0;						\
									\
            __ptr = mrp_mm_realloc(ptr, __size, __LOC__);		\
	    if (__ptr != NULL) {					\
		if ((unsigned)(__n) > (unsigned)(__o))			\
                    memset(__ptr + (__o), 0,				\
			   ((__n)-(__o)) * sizeof(*__ptr));		\
                ptr = __ptr;                                            \
            }                                                           \
            __ptr; })


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

MRP_CDECL_END

#endif /* __MURPHY_MM_H__ */

