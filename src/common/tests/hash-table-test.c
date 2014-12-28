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
#include <string.h>
#include <unistd.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/hash-table.h>

#define MEMBER_OFFSET MRP_OFFSET
#define ALLOC_ARR(type, n) mrp_allocz(sizeof(type) * (n))
#define FREE               mrp_free
#define STRDUP             mrp_strdup

#define NKEY   4
#define NPHASE 0xff

enum {
    V_FATAL = 0,
    V_ERROR,
    V_PROGRESS,
    V_INFO
};

#define INFO(fmt, args...)  do {                                \
        if (test.verbosity >= V_INFO) {                         \
            printf("[%s] "fmt"\n" , __FUNCTION__ , ## args);    \
            fflush(stdout);                                     \
        }                                                       \
    } while (0)

#define PROGRESS(fmt, args...) do {                             \
        if (test.verbosity >= V_PROGRESS) {                     \
            printf("[%s] "fmt"\n" , __FUNCTION__ , ## args);    \
            fflush(stdout);                                     \
        }                                                       \
    } while (0)

#define ERROR(fmt, args...) do {                                        \
        if (test.verbosity >= V_ERROR) {                                \
            printf("[%s] [phase #%d.%d] error: "fmt"\n",                \
                   __FUNCTION__, test.phi, test.phj, ## args);          \
            fflush(stdout);                                             \
        }                                                               \
    } while (0)

#define FATAL(fmt, args...) do {                                        \
        printf("[%s] [phase #%d.%d (%d)] fatal error: "fmt"\n",         \
               __FUNCTION__, test.phi, test.phj, test.size, ## args);   \
        fflush(stdout);                                                 \
        exit(1);                                                        \
    } while (0)

#define MKSTR(fmt, args...) ({                                  \
            char *_ptr, _buf[64] = "";                          \
            snprintf(_buf, sizeof(_buf), fmt , ## args);        \
            _ptr = STRDUP(_buf);                                \
            _ptr; })

#define ENTRY_KEY(entry, idx) ({                        \
        char *_key;                                     \
        switch ((idx)) {                                \
        case 0:  _key = (entry)->str1; break;           \
        case 1:  _key = (entry)->str2; break;           \
        case 2:  _key = (entry)->str3; break;           \
        case 3:  _key = (entry)->str4; break;           \
        default: FATAL("invalid key idx %d", (idx));    \
        }                                               \
        _key; })

#define PATTERN_BIT(pattern, idx)                       \
    (pattern & (1 << ((idx) & ((sizeof(pattern) * 8) - 1))))

typedef struct {
    char            *str1;
    int              int1;
    char            *str2;
    char            *str3;
    int              int2;
    char            *str4;
    uint32_t         cookie;
} entry_t;


typedef struct {
    mrp_hashtbl_t *ht;
    int            size;

    entry_t       *entries;
    int            nentry;

    int            keyidx;
    uint64_t       pattern;
    int            cookies;
    int            iter;

    int            verbosity;
    int            run, phi, phj;
    uint32_t       ncycle, cycles;
} test_t;


test_t test;

void
populate(void)
{
    entry_t  *entry;
    char     *key;
    int       i;

    INFO("populating with %s-generated cookies...",
         test.cookies ? (test.run & 0x1 ? "user" : "reversed user") : "table");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        key = ENTRY_KEY(entry, test.keyidx);

        if (test.cookies)
            entry->cookie = (test.run & 0x1) ? 1 + i : test.nentry - i;
        else
            entry->cookie = 0;

        if (mrp_hashtbl_add(test.ht, key, entry, &entry->cookie) < 0)
            FATAL("failed to hash in entry '%s'", key);
        else
            INFO("hashed in entry '%s'", key);
    }

    INFO("done.");
}


void
iterate(void)
{
    entry_t  *obj, *del;
    uint32_t  cookie;
    char     *key;
    int       n;
    mrp_hashtbl_iter_t it;

    INFO("iterating forward...");

    n = 0;
    MRP_HASHTBL_FOREACH(test.ht, &it, &key, &cookie, &obj) {
        INFO("forward: %s (0x%x): %p (%s)", key, cookie, obj,
             ENTRY_KEY(obj, test.keyidx));
        n++;
    }

    if (n != test.nentry)
        FATAL("failed to iterate through all %d entries (got %d)\n",
              test.nentry, n);

    INFO("iterating backward...");

    n = 0;
    MRP_HASHTBL_FOREACH_BACK(test.ht, &it, &key, &cookie, &obj) {
        INFO("backward: %s (0x%x): %p (%s)", key, cookie, obj,
             ENTRY_KEY(obj, test.keyidx));
        n++;
    }

    if (n != test.nentry)
        FATAL("failed to iterate backwards through all %d entries (got %d)\n",
              test.nentry, n);

    INFO("iterating forward and deleting...");

    n = 0;
    MRP_HASHTBL_FOREACH(test.ht, &it, &key, &cookie, &obj) {
        INFO("forward/del: %s (0x%x): %p (%s)", key, cookie, obj,
             ENTRY_KEY(obj, test.keyidx));
        if (n & 0x1)
            del = mrp_hashtbl_del(test.ht, key, cookie, false);
        else
            del = mrp_hashtbl_del(test.ht, key, 0, false);

        if (del != obj)
            FATAL("expected entry %s%s not found (%p != %p)",
                  n & 0x1 ? "by cookie " : "", key, del, obj);

        n++;
    }

    if (n != test.nentry)
        FATAL("failed to iterate/del through all %d entries (got %d)\n",
              test.nentry, n);

    populate();

    INFO("iterating backward and deleting...");

    n = 0;
    MRP_HASHTBL_FOREACH_BACK(test.ht, &it, &key, &cookie, &obj) {
        INFO("backward/del: %s (0x%x): %p (%s)", key, cookie, obj,
             ENTRY_KEY(obj, test.keyidx));
        if (n & 0x1)
            del = mrp_hashtbl_del(test.ht, key, cookie, false);
        else
            del = mrp_hashtbl_del(test.ht, key, 0, false);

        if (del != obj)
            FATAL("expected entry %s%s not found (%p != %p)",
                  n & 0x1 ? "by cookie " : "", key, del, obj);

        n++;
    }

    if (n != test.nentry)
        FATAL("failed to iterate back/del through all %d entries (got %d)\n",
              test.nentry, n);

    INFO("done.");

    populate();
}


void
evict(void)
{
    entry_t *entry, *found;
    char    *key;
    int      i;

    INFO("evicting...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        if (PATTERN_BIT(test.pattern, i)) {
            key   = ENTRY_KEY(entry, test.keyidx);
            if (i & 0x1)
                found = mrp_hashtbl_del(test.ht, key, 0, FALSE);
            else
                found = mrp_hashtbl_del(test.ht, key, entry->cookie, FALSE);

            if (found != entry)
                FATAL("expected entry to delete%s '%s' not found (%p != %p)",
                      i & 0x1 ? " by cookie" : "", key, found, entry);

            INFO("removed entry%s '%s' (%p)", i & 0x1 ? " by cookie" : "",
                 key, found);
        }
    }

    INFO("done.");
}


void
readd(void)
{
    entry_t *entry, *found;
    char    *key;
    int      i;

    INFO("re-adding...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        if (PATTERN_BIT(test.pattern, i)) {
            key   = ENTRY_KEY(entry, test.keyidx);
            found = mrp_hashtbl_lookup(test.ht, key, 0);

            if (found != NULL)
                FATAL("unexpected entry to re-add '%s' found (%p)", key, found);

            if (mrp_hashtbl_add(test.ht, key, entry, &entry->cookie) < 0)
                FATAL("failed to re-add entry '%s'", key);

            INFO("re-added entry '%s'", key);
        }
    }

    INFO("done.");
}


void
check(void)
{
    entry_t *entry, *found, *byc, *wrc;
    char    *key;
    int      i;

    INFO("checking...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        key   = ENTRY_KEY(entry, test.keyidx);
        found = mrp_hashtbl_lookup(test.ht, key, 0);
        byc   = mrp_hashtbl_lookup(test.ht, key, entry->cookie);
        wrc   = mrp_hashtbl_lookup(test.ht, key, entry->cookie + 5);
        if (!PATTERN_BIT(test.pattern, i)) {
            if (found != entry)
                FATAL("expected entry '%s' not found (%p != %p)",
                      key, found, entry);
            if (byc != entry)
                FATAL("entry by coookie '%s' not found (%p != %p)",
                      key, byc, entry);
            if (wrc != NULL)
                FATAL("unexpected entry by wrong cookie '%s' (%p) found",
                      key, entry);
        }
        else {
            if (found != NULL)
                FATAL("unexpected entry '%s' found", key);
            if (byc != NULL)
                FATAL("unexpected entry by cookie '%s' found", key);
            if (wrc != NULL)
                FATAL("unexpected entry by wrong coookie '%s' found", key);
        }
    }

    INFO("done.");
}


void
empty_cb(char *key, entry_t *entry, void *data)
{
    (void)data;

    FATAL("unexpected entry %p (%s) in hash table", entry, key);
}


void
reset(void)
{
    entry_t *entry, *found;
    char    *key;
    int      i;

    INFO("resetting...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        key   = ENTRY_KEY(entry, test.keyidx);
        if (i & 0x1)
            found = mrp_hashtbl_del(test.ht, key, entry->cookie, FALSE);
        else
            found = mrp_hashtbl_del(test.ht, key, 0, FALSE);

        if (found != entry)
            FATAL("expected entry %s%s not found (%p != %p)",
                  i & 0x1 ? "by cookie " : "", key, found, entry);

        INFO("removed entry%s '%s' (%p)", i & 0x1 ? " by cookie" : "",
             key, found);
    }

    INFO("done.");
}


unsigned int hash_func(const void *key)
{
    unsigned int  h;
    const char   *p;

    for (h = 0, p = key; *p; p++) {
        h <<= 1;
        h  ^= *p;
    }

    return h;
}


int cmp_func(const void *key1, const void *key2)
{
    return strcmp(key1, key2);
}


void
test_init(void)
{
    int      i;
    entry_t *entry;

    INFO("setting up tests for %d entries...", test.nentry);

    if ((test.entries = ALLOC_ARR(entry_t, test.nentry)) == NULL)
        FATAL("failed to allocate test set");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        entry->str1 = MKSTR("entry-string-%d:1", i);
        entry->int1 = i;
        entry->str2 = MKSTR("entry-string-%d:2", i);
        entry->str3 = MKSTR("entry-string-%d:3", i);
        entry->int2 = i * 2;
        entry->str4 = MKSTR("entry-string-%d:4", i);

        if (!entry->str1 || !entry->str2 || !entry->str3 || !entry->str4)
            FATAL("failed to initialize test set");
    }

    INFO("test setup done.");
}


void
test_exit(void)
{
    entry_t *entry;
    int      i;

    INFO("cleaning up tests...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        FREE(entry->str1);
        FREE(entry->str2);
        FREE(entry->str3);
        FREE(entry->str4);
    }

    FREE(test.entries);

    test.entries = NULL;
    test.nentry  = 0;

    INFO("test cleanup done.");
}


void
test_run(void)
{
    mrp_hashtbl_config_t cfg;
    entry_t             *entry;
    int                  i, j;


    /*
     * Create a hash table, run a test loop consisting of
     *
     *   1) populate table
     *   2) iterate through entries
     *   3) selectively remove entries
     *   4) check the table
     *   5) check the entries (for corruption)
     *   6) reset the table
     *
     * then delete the hash table
     */

    mrp_clear(&cfg);
    cfg.nbucket = test.size / 4;
    cfg.nlimit  = test.size < test.nentry ? test.nentry : test.size;
    cfg.hash    = hash_func;
    cfg.comp    = cmp_func;
    cfg.free    = NULL;
    test.ht     = mrp_hashtbl_create(&cfg);

    if (test.ht == NULL)
        FATAL("failed to create hash table (#%d, size %d)",
              test.keyidx, test.size);

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        test.phi = i;
        test.phj = 0;

        PROGRESS("Running test cycle #%d %.2f %% "
                 "(size %d, %s-generated cookies)...", i,
                 (test.ncycle * 100.0) / test.cycles,
                 test.size,
                 test.cookies ? (test.run & 0x1 ? "user" : "reversed user") :
                 "table");
        populate();

        test.pattern = 0;
        for (j = 0; j < NPHASE; j++) {
            test.phj = j;

            INFO("Running test phase #%d.%d...", i, j);

            if (test.iter)
                iterate();
            evict();
            check();
            readd();

            test.pattern++;

            INFO("done.");
        }

        reset();
        test.ncycle++;
    }

    mrp_hashtbl_destroy(test.ht, FALSE);
    test.ht = NULL;
    test.run++;
}


int
main(int argc, char *argv[])
{
    int i, n, size;

    memset(&test, 0, sizeof(test));

    test.nentry    = 16;
    test.verbosity = V_ERROR;
    test.iter      = TRUE;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            mrp_debug_enable(true);
            mrp_debug_set("@hash-table.c");
        }
        else if (!strcmp(argv[i], "-v"))
            test.verbosity++;
        else {
            if ((test.nentry = (int)strtoul(argv[i], NULL, 10)) <= 16)
                test.nentry = 16;
        }
    }

    test_init();

    size = test.nentry;
    n    = 0;
    while (size >= 8) {
        n++;
        size /= 2;
    }

    test.cycles = NKEY * test.nentry * n * 3;

    for (i = 0; i < NKEY; i++) {
        test.size = test.nentry;
        test.keyidx = i;
        test.size = test.nentry;
        while (test.size >= 8) {
            test.cookies = FALSE;
            test_run();
            test.cookies = TRUE;
            test_run();
            test_run();
            test.size /= 2;
        }
    }

    test_exit();

    return 0;
}
