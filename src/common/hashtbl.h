#ifndef __MURPHY_HASHTBL_H__
#define __MURPHY_HASHTBL_H__


MRP_CDECL_BEGIN

typedef struct mrp_htbl_s mrp_htbl_t;

/** Prototype for key comparison functions. */
typedef int (*mrp_htbl_comp_fn_t)(const void *key1, const void *key2);

/** Prototoype for key hash functions. */
typedef uint32_t (*mrp_htbl_hash_fn_t)(const void *key);

/** Prototype for functions used to free entries. */
typedef void (*mrp_htbl_free_fn_t)(void *key, void *object);


/*
 * hash table configuration
 */
typedef struct {
    size_t             nentry;                   /* estimated entries */
    mrp_htbl_comp_fn_t comp;                     /* comparison function */
    mrp_htbl_hash_fn_t hash;                     /* hash function */
    mrp_htbl_free_fn_t free;                     /* freeing function */
    size_t             nbucket;                  /* number of buckets, or 0 */
} mrp_htbl_config_t;


/** Create a new hash table with the given configuration. */
mrp_htbl_t *mrp_htbl_create(mrp_htbl_config_t *cfg);

/** Destroy a hash table, free all entries unless @free is FALSE. */
void mrp_htbl_destroy(mrp_htbl_t *ht, int free);

/** Reset a hash table, also free all entries unless @free is FALSE. */
void mrp_htbl_reset(mrp_htbl_t *ht, int free);

/** Insert the given @key/@object pair to the hash table. */
int mrp_htbl_insert(mrp_htbl_t *ht, void *key, void *object);

/** Remove and return the object for @key, also free unless @free is FALSE. */
void *mrp_htbl_remove(mrp_htbl_t *ht, void *key, int free);

/** Look up the object corresponding to @key. */
void *mrp_htbl_lookup(mrp_htbl_t *ht, void *key);

/** Find the first matching entry in a hash table. */
typedef int (*mrp_htbl_find_cb_t)(void *key, void *object, void *user_data);
void *mrp_htbl_find(mrp_htbl_t *ht, mrp_htbl_find_cb_t cb, void *user_data);


/*
 * hash table iterators
 */

enum {
    MRP_HTBL_ITER_STOP   = 0x0,                  /* stop iterating */
    MRP_HTBL_ITER_MORE   = 0x1,                  /* keep iterating */
    MRP_HTBL_ITER_UNHASH = 0x2,                  /* unhash without freeing */
    MRP_HTBL_ITER_DELETE = 0x6,                  /* unhash and free */
};

typedef int (*mrp_htbl_iter_cb_t)(void *key, void *object, void *user_data);
int mrp_htbl_foreach(mrp_htbl_t *ht, mrp_htbl_iter_cb_t cb, void *user_data);

MRP_CDECL_END

#endif /* __MURPHY_HASHTBL_H__ */
