/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#ifndef __MURPHY_HASH_TABLE_H__
#define __MURPHY_HASH_TABLE_H__

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

/**
 * \addtogroup MurphyCommonInfra
 * @{
 *
 * @file hash-table.h
 *
 * @brief Murphy hash table implementation.
 *
 * Murphy hash tables provide a simple, general hash table implementation.
 * The API provides the usual functions for creating, destroying, and
 * resetting hash tables. It provides functions for adding entries, deleting
 * entries and looking up entries to and from hash tables. Additionally,
 * macros and functions are provided for iterating through all entries in a
 * hash table.
 *
 * One potentially interesting feature of Murphy hash tables is the optional
 * cookie support. You can think of hash table cookies as hints to speed up
 * insertion, lookup, deletion and replacement operations of hash table
 * entries. In the best case, the achievable efficiency is O(1). Cookies can
 * be viewed as virtual- or pseudo-indices into the hash table. When cookies
 * are used properly, the hash table effectively becomes a table which is both
 * indexable by hash keys and by pseudo-indices, IOW cookies.
 *
 * The easiest way to use cookies is to let the table allocate cookies for
 * you. However, if you prefer to do so you can provide cookies for the table
 * yourself. If you chose to do so, some case must be taken to properly select
 * cookies such, that the table is able to utilize them to speed up access.
 *
 * Generally speaking, to make sure cookies do achieve O(1) access times, you
 * either need to let the table allocate cookies for you, or you need to
 * declare how many entries the table is going to have at max and use as
 * cookies unique integers between 1 and the maximum number of table entries.
 */

/**
 * @brief Global limits for hash table memory usage.
 */
typedef struct mrp_hashtbl_limits_s mrp_hashtbl_limits_t;

/**
 * @brief Configuration data for a hash table.
 */
typedef struct mrp_hashtbl_config_s mrp_hashtbl_config_t;

/**
 * @brief Opaque type for a hash table.
 */
typedef struct mrp_hashtbl_s mrp_hashtbl_t;

/**
 * @brief Type/prototype for a key hash function.
 */
typedef uint32_t (*mrp_hash_fn_t)(const void *key);

/**
 * @brief Type/prototype for a key comparison function.
 */
typedef int (*mrp_comp_fn_t)(const void *key1, const void *key2);

/**
 * @brief Type/prototype for a hash table entry freeing function.
 */
typedef void (*mrp_free_fn_t)(void *key, void *object);

/**
 * @brief A semi-opaque hash table iterator for keeping iteration state.
 */
typedef struct {
    void     *b;                         /* I'm not tellin ya... */
    void     *e;                         /* Not about this one, either... */
    uint32_t  g;                         /* Nope, my lips are sealed... */
    int       d;                         /* No, no, and finally no... */
} mrp_hashtbl_iter_t;

/**
 * @brief Macro to mark nonexistent cookies.
 */
#define MRP_HASH_COOKIE_NONE ((uint32_t)0)

/**
 * @brief User-specified configuration for a hash table.
 *
 * Use this structure to specify the user-configurable settings for a hash
 * table to be created. The @hash and @comp callbacks for calculating hash
 * keys and comparing keys must be set. All other fields are optional and
 * can be omitted (left 0 or NULL).
 */
struct mrp_hashtbl_config_s {
    mrp_hash_fn_t hash;                  /* key hash function */
    mrp_comp_fn_t comp;                  /* key comparison function */
    mrp_free_fn_t free;                  /* key/object freeing function */
    size_t        nalloc;                /* guaranteed/preallocated entries */
    size_t        nlimit;                /* maximum allowed entries */
    size_t        nbucket;               /* number of buckets to use */
    int           cookies : 1;           /* whether to use cookies */
};

/**
 * @brief Create a hash table with the given configuration.
 *
 * Use this function to create a new hash table with the given configuration.
 *
 * @param [in] config  configuration for the hash table to be created
 *
 * @return Returns a pointer to the newly created hash table upon success,
 *         @NULL otherwise.
 */
mrp_hashtbl_t *mrp_hashtbl_create(mrp_hashtbl_config_t *config);

/**
 * @brief Destroy a hash table.
 *
 * Destroy the given hash table, optionally freeing all hashed entries
 * in the table.
 *
 * @param [in] htbl     hash table to destroy
 * @param [in] release  whether to free hashed entries in the table
 */
void mrp_hashtbl_destroy(mrp_hashtbl_t *htbl, bool release);

/**
 * @brief Reset a hash table.
 *
 * Reset the given hash hash table to its initial state, optionally freeing
 * all hashed entries in the table.
 *
 * @param [in] htbl     hash table to reset
 * @param [in] release  whether to free hashed entries in the table
 */
void mrp_hashtbl_reset(mrp_hashtbl_t *htbl, bool release);

/**
 * @brief Add a new entry to a hash table.
 *
 * Adds the given object for the given key to the hash table. If @cookie is
 * given, it can later be used for fast entry retrieval and deletion later,
 * without having to re-calculate the hash function. You can think of it as
 * a generalized entry index. If set to @NULL, no cookie is used. If @cookie
 * is set to @MRP_HASH_COOKIE_NONE, the hash table will generate and set the
 * cookie for the new entry. Otherwise, the value of @cookie is used for the
 * new entry as such. In this case, if the same @cookie is already used in
 * the table, adding the new entry will fail. To significantly speed up
 * hash table lookups, @cookie should be an integer between 1 and the allowed
 * number of entries in the table.
 *
 * @param [in]     htbl    hash table to add new entry to
 * @param [in]     key     hash key to use for @obj
 * @param [in]     obj     object to add to the table
 * @param [in,out] cookie  cookie for the new entry
 *
 * @return Returns 0 for success, -1 otherwise.
 */
int mrp_hashtbl_add(mrp_hashtbl_t *htbl, const void *key, void *obj,
                    uint32_t *cookie);

/**
 * @brief Delete an entry from a hash table.
 *
 * Delete the first entry with the given key from the hash table. If @cookie is
 * specified, the entry corresponding to @cookie will be deleted.
 *
 * @param [in] htbl     hash table to delete entry from
 * @param [in] key      key for the entry to delete
 * @param [in] cookie   optional cookie for entry to be deleted
 * @param [in] release  whether to free the hashed entry
 *
 * @return Returns the deleted object from the table, which might have been
 *         already freed if @release was set to @true. Returns @NULL if no
 *         matching object was found and sets @errno to @ENOENT.
 */
void *mrp_hashtbl_del(mrp_hashtbl_t *htbl, const void *key, uint32_t cookie,
                      bool release);

/**
 * @brief Conveninece macro to remove a hash table entry without freeing it.
 *
 * @param [in] _htbl    hash table to remove entry from
 * @param [in] _key     key for the entry to remove
 * @param [in] _cookie  cookie of entry, or @MRP_HASH_COOKIE_NONE
 *
 * @return Removes and returns the first entry with the given @key, or the
 *         entry corresponding to @cookie if that is given. Returns @NULL
 *         upon failure and sets @errno to @ENOENT.
 */
#define mrp_hashtbl_remove(_htbl, _key, _cookie)        \
    mrp_hashtbl_del((_htbl), (_key), (_cookie), false)

/**
 * @brief Look up an entry from a hash table.
 *
 * Find the first entry for the given @key, or the entry for @cookie in the
 * has table.
 *
 * @param [in] htbl    hash table to find the entry in
 * @param [in] key     key for the entry
 * @param [in] cookie  cookie for the entry
 *
 * @return Returns the first found object with the given @key, or the one
 *         corresponding to @cookie if it was given. Returns @NULL if no
 *         entry was found and sets @errno to @ENOENT.
 * @htbl.
 */
void *mrp_hashtbl_lookup(mrp_hashtbl_t *htbl, const void *key, uint32_t cookie);

/**
 * @brief Replace a hash table entry with a new one.
 *
 * Replace the hash table entry for the given @key with the given @object.
 *
 * @param [in] htbl     hash table to replace entry in
 * @param [in] key      key for the entry to replce
 * @param [in] cookie   cookie for the entry to replace or @MRP_HASH_COOKIE_NONE
 * @param [in] object   new object to replace the old one
 * @param [in] release  whether to free the old entry
 *
 * @return Returns the old entry found in the table, or @NULL if no old entry
 *         was found in which case @errno is set to @ENOENT and the operation
 *         is equivalent to @mrp_hashtbl_add. If replacing or inserting fails,
 *         @errno will be set to an error code other than @ENOENT.
 *
 *         Please note, that if @key is a member of the objects hashed in the
 *         table and the provided free callback frees the full object, if you
 *         invoke @mrp_hashtbl_lookup with @release set to @true, the old key
 *         will be invalidated. Be sure to pass in the key within the new
 *         object so the hash table will correctly update the corresponding
 *         entry accordingly.
 */
void *mrp_hashtbl_replace(mrp_hashtbl_t *htbl, void *key, uint32_t cookie,
                          void *object, bool release);

/**
 * @brief Helper function to initialize a hash-table iterator.
 */
void _mrp_hashtbl_begin(mrp_hashtbl_t *t, mrp_hashtbl_iter_t *it, int dir);

/**
 * @brief Helper function to iterate through all hash table entries.
 *
 * This is the helper function that takes care of the iterating logic
 * and iterator state administration for @MRP_HASHTBL_FOREACH and
 * @MRP_HASHTBL_FOREACH_BACK. You're better off using either on of
 * those macros directly.
 *
 * @param [in] htbl     hash table to iterate through
 * @param [in,out] it   iterator state variable
 * @param [in] dir      direction, >= 0 for forward, < 0 for backward
 * @param [out] key     pointer to variable to put entry key into, or @NULL
 * @param [out] cookie  pointer to variable to put entry cookie into, or @NULL
 * @param [out] object  pointer to variable to put entry object into, or @NULL
 *
 * @return Returns @NULL once all entries have been iterated through.
 */
void *_mrp_hashtbl_iter(mrp_hashtbl_t *t, mrp_hashtbl_iter_t *it, int dir,
                        const void **key, uint32_t *cookie, const void **obj);

/**
 * @brief Convenience helper macro for iterating in forward direction.
 */
#define _mrp_hashtbl_next(_t, _it, _key, _cookie, _obj) \
    _mrp_hashtbl_iter(_t, _it, +1, _key, _cookie, _obj)

/**
 * @brief Convenience helper macro for iterating in reverse direction.
 */
#define _mrp_hashtbl_prev(_t, _it, _key, _cookie, _obj) \
    _mrp_hashtbl_iter(_t, _it, -1, _key, _cookie, _obj)

/**
 * @brief Loop through all entries in a hash table.
 *
 * Macro to loop through all key/cookie/value tuples in the given hash table.
 * The variables @_key, @_cookie, and @_obj will be successively set to the
 * key, cookie, and object from the entries found in the hash table. If you
 * are not interested in the key, cookie, or the object itself, you can use
 * a @NULL for the correspoding pointer.
 *
 * @param [in] _htbl     hash table to iterate through
 * @param [in,out] _it   a hash table iterator
 * @param [out] _key     key of the current entry
 * @param [out] _cookie  cookie of the current entry
 * @param [out] _obj     object of the curent entry
 */
#define MRP_HASHTBL_FOREACH(_htbl, _it, _key, _cookie, _obj)            \
    for (_mrp_hashtbl_begin(_htbl, (_it), +1),                          \
             _mrp_hashtbl_next(_htbl, (_it),                            \
                               (const void **)(_key),                   \
                               (_cookie),                               \
                               (const void **)(_obj));                  \
         (_it)->b != NULL;                                              \
         _mrp_hashtbl_next(_htbl, (_it),                                \
                           (const  void **)(_key),                      \
                           (_cookie),                                   \
                           (const void **)(_obj)))

/**
 * @brief Loop through all entries in a hash table in reverse.
 *
 * Macro to loop through all key/cookie/value tuples in the given hash table.
 * The variables @_key, @_cookie, and @_obj will be successively set to the
 * key, cookie, and object from the entries found in the hash table. If you
 * are not interested in the key, cookie, or the object itself, you can use
 * a @NULL for the correspoding pointer.
 *
 * @param [in] _htbl     hash table to iterate through
 * @param [in,out] _it   a hash table iterator
 * @param [out] _key     key of the current entry
 * @param [out] _cookie  cookie of the current entry
 * @param [out] _obj     object of the curent entry
 */
#define MRP_HASHTBL_FOREACH_BACK(_htbl, _it, _key, _cookie, _obj)       \
    for (_mrp_hashtbl_begin(_htbl, (_it), -1),                          \
             _mrp_hashtbl_prev(_htbl, (_it),                            \
                               (const void **)(_key),                   \
                               (_cookie),                               \
                               (const void **)(_obj));                  \
         (_it)->b != NULL;                                              \
         _mrp_hashtbl_prev(_htbl, (_it),                                \
                           (const  void **)(_key),                      \
                           (_cookie),                                   \
                           (const void **)(_obj)))

/**
 * @brief A simple string hash function.
 *
 * A simple generic string hash function.
 *
 * @param [in] key  The string to calculate the hash value for.
 *
 * @return Returns the hash value for the given string.
 */
uint32_t mrp_hash_string(const void *key);

/**
 * @brief A simple strcmp-based string comparison function.
 *
 * A simple function to compare two strings for lexical sort order. This
 * function is a simple wrapper around strcmp.
 *
 * @param [in] key1  string to compare
 * @param [in] key2  string to compare
 *
 * @return Returns 0, < 0, or > 0 depending on whether key1 is equal, smaller,
 *         or greater than key2 in the lexical sorting sense.
 */
int mrp_comp_string(const void *key1, const void *key2);

/**
 * @brief A straight-forward hash function that hashes the key to itself.
 *
 * A simple hash function which treats its argument as a numeric value and uses
 * that as the hash value.
 *
 * @param [in] key  key to return hash value for
 *
 * @return Returns the lowest 32-bits of key as the hash value.
 */
uint32_t mrp_hash_direct(const void *key);

/**
 * @brief A straight-forward numerical key comparison function.
 *
 * A simple comparison function that treats its keys as numeric values.
 *
 * @param [in] key1  key to compare
 * @param [in] key2  key to compare
 *
 * @return Returns 0, < 0, or > 0 depending on whether key1 is equal, smaller,
 *         or greater than key2.

 */
int mrp_comp_direct(const void *key1, const void *key2);

/**
 * @brief User-specified global limits concerning all hash-tables.
 */
struct mrp_hashtbl_limits_s {
    uint32_t table_maxmem;               /**< max memory for a table */
    uint32_t total_maxmem;               /**< max memory for all tables */
};

/**
 * @brief Macro to denote an unbounded limit.
 */
#define MRP_HASHLIMIT_UNLIMITED ((uint32_t)-1)

/**
 * @brief Macro to denote an unspecified limit.
 */
#define MRP_HASHLIMIT_DONTCARE ((uint32_t)0)

/**
 * @brief Replace the currently set limits with the given ones.
 */
int mrp_hashtbl_set_limits(mrp_hashtbl_limits_t *limits);

/**
 * @brief Combine the given limits with the currently set ones.
 */

int mrp_hashtbl_add_limits(mrp_hashtbl_limits_t *limits);

/**
 * @}
 */

MRP_CDECL_END

#endif /* __MURPHY_HASH_TABLE_H__ */
