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

#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/macros.h>
#include <murphy/common/hashtbl.h>

#define MEMBER_OFFSET MRP_OFFSET
#define ALLOC_ARR(type, n) mrp_allocz(sizeof(type) * (n))
#define FREE               mrp_free
#define STRDUP             mrp_strdup

#define hash_tbl_t      mrp_htbl_t
#define hash_tbl_cfg_t  mrp_htbl_config_t
#define hash_tbl_create mrp_htbl_create
#define hash_tbl_delete mrp_htbl_destroy
#define hash_tbl_add    mrp_htbl_insert
#define hash_tbl_del    mrp_htbl_remove
#define hash_tbl_lookup mrp_htbl_lookup

#define list_hook_t     mrp_list_hook_t
#define list_init       mrp_list_init
#define list_append     mrp_list_append
#define list_delete     mrp_list_delete

#define NKEY   4
#define NPHASE 0xff

#define INFO(fmt, args...)  do {                         \
        printf("[%s] "fmt"\n" , __FUNCTION__ , ## args); \
        fflush(stdout);                                  \
    } while (0)

#define ERROR(fmt, args...) do {                                \
        printf("[%s] error: "fmt"\n" , __FUNCTION__, ## args);  \
        fflush(stdout);                                         \
    } while (0)

#define FATAL(fmt, args...) do {                                        \
        printf("[%s] fatal error: "fmt"\n" , __FUNCTION__, ## args);    \
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
    char        *str1;
    int          int1;
    char        *str2;
    list_hook_t  hook;
    char        *str3;
    int          int2;
    char        *str4;
} entry_t;


typedef struct {
    hash_tbl_t *ht;
    size_t      size;

    entry_t    *entries;
    int         nentry;

    int         keyidx;
    uint32_t    pattern;
} test_t;


test_t test;

void
populate(void)
{
    entry_t *entry;
    char    *key;
    int      i;

    INFO("populating...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        key = ENTRY_KEY(entry, test.keyidx);

        if (hash_tbl_add(test.ht, key, entry))
            INFO("hashed in entry '%s'", key);
        else
            FATAL("failed to hash in entry '%s'", key);
    }

    INFO("done.");
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
            found = hash_tbl_del(test.ht, key, FALSE);

            if (found != entry)
                FATAL("expected entry to delete '%s' not found (%p != %p)",
                      key, found, entry);

            INFO("removed entry '%s' (%p)", key, found);
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
            found = hash_tbl_lookup(test.ht, key);

            if (found != NULL)
                FATAL("unexpected entry to re-add '%s' found (%p)", key, found);

            if (!hash_tbl_add(test.ht, key, entry))
                FATAL("failed to re-add entry '%s'", key);

            INFO("re-added entry '%s'", key);
        }
    }

    INFO("done.");
}


void
check(void)
{
    entry_t *entry, *found;
    char    *key;
    int      i;

    INFO("checking...");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        key   = ENTRY_KEY(entry, test.keyidx);
        found = hash_tbl_lookup(test.ht, key);

        if (!PATTERN_BIT(test.pattern, i)) {
            if (found != entry)
                FATAL("expected entry '%s' not found (%p != %p)",
                      key, found, entry);
        }
        else {
            if (found != NULL)
                FATAL("unexpected entry '%s' found", key);
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
        found = hash_tbl_del(test.ht, key, FALSE);

        if (found != entry)
            FATAL("expected entry %s not found (%p != %p)",
                  key, found, entry);

        INFO("removed entry '%s' (%p)", key, found);
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

    INFO("setting up tests...");

    if ((test.entries = ALLOC_ARR(entry_t, test.nentry)) == NULL)
        FATAL("failed to allocate test set");

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        list_init(&entry->hook);

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
    hash_tbl_cfg_t  cfg;
    entry_t        *entry;
    int             i, j;


    /*
     * Create a hash table, run a test loop consisting of
     *
     *   1) populate table
     *   2) selectively remove entries
     *   3) check the table
     *   4) check the entries (for corruption)
     *   5) reset the table
     *
     * then delete the hash table
     */

    cfg.nbucket = test.size / 4;
    cfg.hash    = hash_func;
    cfg.comp    = cmp_func;
    cfg.free    = NULL;
    test.ht     = hash_tbl_create(&cfg);

    if (test.ht == NULL)
        FATAL("failed to create hash table (#%d, size %zd)",
              test.keyidx, test.size);

    for (i = 0, entry = test.entries; i < test.nentry; i++, entry++) {
        populate();

        test.pattern = 0;
        for (j = 0; j < NPHASE; j++) {
            INFO("Running test phase #%d...", j);

            evict();
            check();
            readd();

            test.pattern++;

            INFO("done.");
        }

        reset();
    }

    hash_tbl_delete(test.ht, FALSE);
    test.ht = NULL;
}


int
main(int argc, char *argv[])
{
    int i;

    memset(&test, 0, sizeof(test));

    if (argc < 2 || (test.nentry = (int)strtoul(argv[1], NULL, 10)) <= 16)
        test.nentry = 16;

    test_init();

    for (i = 0; i < NKEY; i++) {
        test.keyidx = i;
        test.size = test.nentry;     test_run();
        test.size = test.nentry / 2; test_run();
        test.size = test.nentry / 4; test_run();
    }

    test_exit();

    return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */

