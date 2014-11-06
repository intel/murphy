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
#include <murphy/common/mm.h>

#define fatal(fmt, args...) do {                                          \
        fprintf(stderr, "fatal error: "fmt"\n" , ## args);                \
        exit(1);                                                          \
    } while (0)

#define error(fmt, args...) do {                                          \
        fprintf(stdout, "error: "fmt"\n" , ## args);                      \
    } while (0)

#define info(fmt, args...) do {                                           \
        fprintf(stdout, fmt"\n" , ## args);                               \
    } while (0)



static int basic_tests(int n)
{
    void **ptrs;
    char   buf[1024], *p;
    int    i;

    mrp_mm_config(MRP_MM_DEBUG);

    ptrs = mrp_allocz(n * sizeof(*ptrs));

    if (ptrs == NULL)
        fatal("Failed to allocate pointer table.");

    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "#%d: message number %d (0x%x)", i, i, i);

        p = ptrs[i] = mrp_strdup(buf);

        if (p != NULL) {
            if (!strcmp(buf, p)) {
                printf("'%s' was duplicated as '%s'\n", buf, p);
            }
            else {
                printf("'%s' was incorrectly duplicated as '%s'\n", buf, p);
                return FALSE;
            }
        }
        else {
            printf("failed to duplicate '%s'\n", buf);
            return FALSE;
        }
    }

    mrp_mm_check(stdout);

    for (i = 0; i < n; i += 2) {
        mrp_free(ptrs[i]);
        ptrs[i] = NULL;
    }

    mrp_mm_check(stdout);

    for (i = 0; i < n; i++) {
        mrp_free(ptrs[i]);
        ptrs[i] = NULL;
    }

    mrp_mm_check(stdout);

    mrp_free(ptrs);

    mrp_mm_check(stdout);

    return TRUE;
}


typedef struct {
    char    name[32];
    int     i;
    double  d;
    char   *s;
    void   *p;
} obj_t;


#define NAME_FORMAT "#%d test object"
#define POISON      0xf3

static int obj_setup(void *ptr)
{
    static int  idx = 0;
    obj_t      *obj = ptr;

    snprintf(obj->name, sizeof(obj->name), NAME_FORMAT, idx);
    obj->i = idx;
    obj->d = 2.0 * idx;
    obj->s = mrp_strdup(obj->name);
    obj->p = ptr;

    return TRUE;
}


static void obj_cleanup(void *ptr)
{
    obj_t *obj = ptr;

    mrp_free(obj->s);
}


static int obj_check(obj_t *obj, int alloced)
{
    char name[32];

    if (alloced) {
        snprintf(name, sizeof(name), NAME_FORMAT, obj->i);

        return (!strcmp(name, obj->name) && !strcmp(name, obj->s) &&
                obj->d == 2 * obj->i && obj->p == obj);
    }
    else {
        char check[sizeof(obj_t)];

        memset(check, POISON, sizeof(check));
        if (memcmp(check, obj, sizeof(*obj)))
            error("Object %p not properly poisoned.", obj);
    }

        return TRUE;
}


static int pool_tests(void)
{
    mrp_objpool_config_t  cfg;
    mrp_objpool_t        *pool;
    obj_t               **ptrs;
    int                   limit, prealloc, i, max;
    int                   success;

    limit    = 0;
    prealloc = 512;
    max      = 8382;
    ptrs     = mrp_allocz(max * sizeof(obj_t));

    if (ptrs == NULL) {
        error("Failed to allocate check pointer table.");
        return FALSE;
    }

    cfg.name     = "test pool";
    cfg.limit    = limit;
    cfg.objsize  = sizeof(obj_t);
    cfg.prealloc = prealloc;
    cfg.setup    = obj_setup;
    cfg.cleanup  = obj_cleanup;
    cfg.poison   = POISON;
    cfg.flags    = MRP_OBJPOOL_FLAG_POISON;

    info("Creating object pool...");
    pool = mrp_objpool_create(&cfg);

    if (pool == NULL) {
        error("Failed to create test object pool.");
        return FALSE;
    }

    info("Allocating objects...");
    for (i = 0; i < max; i++) {
        ptrs[i] = mrp_objpool_alloc(pool);

        if (ptrs[i] == NULL) {
            error("Failed to allocate test object #%d.", i);
            success = FALSE;
            goto out;
        }

        if (!obj_check(ptrs[i], TRUE)) {
            error("Object check failed for %p.", ptrs[i]);
            success = FALSE;
        }
    }

    info("Freeing objects...");
    for (i = 0; i < max; i += 2) {
        mrp_objpool_free(ptrs[i]);
        obj_check(ptrs[i], FALSE);
        ptrs[i] = NULL;
    }

    info("Reallocating objects...");
    for (i = 0; i < max; i += 2) {
        ptrs[i] = mrp_objpool_alloc(pool);

        if (ptrs[i] == NULL) {
            error("Failed to re-allocate test object #%d.", i);
            success = FALSE;
            goto out;
        }

        if (!obj_check(ptrs[i], TRUE)) {
            error("Object check failed for %p.", ptrs[i]);
            success = FALSE;
        }

    }

    info("Freeing objects...");
    for (i = 0; i < max; i++) {
        mrp_objpool_free(ptrs[i]);
        ptrs[i] = NULL;
    }

    info("Reallocating again objects...");
    for (i = 0; i < max; i++) {
        ptrs[i] = mrp_objpool_alloc(pool);

        if (ptrs[i] == NULL) {
            error("Failed to re-allocate test object #%d.", i);
            success = FALSE;
            goto out;
        }

        if (!obj_check(ptrs[i], TRUE)) {
            error("Object check failed for %p.", ptrs[i]);
            success = FALSE;
        }
    }

 out:
    mrp_free(ptrs);
    info("Destroying object pool...");
    mrp_objpool_destroy(pool);

    return success;
}


int main(int argc, char *argv[])
{
    int max;

    if (argc > 1)
        max = (int)strtol(argv[1], NULL, 10);
    else
        max = 256;

    info("Running basic tests...");
    basic_tests(max);

    info("Running object pool tests...");
    pool_tests();

    return 0;
}
