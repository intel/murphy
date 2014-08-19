/*
 * Copyright (c) 2014, Intel Corporation
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

#include <stdbool.h>
#include <murphy/common.h>

#define LE_STRING "/org/murphy/resource/0/%d"

// Placeholder structure
typedef struct {
    void *pointer;
} test_object;

static void htbl_free_test_object(void *key, void *object) {
    test_object *obj = object;

    if (key)
        mrp_free(key);

    if (obj)
        mrp_free(obj);
}

int main(int argc, char *argv[]) {
    mrp_htbl_config_t cfg;

    mrp_htbl_t  *table  = NULL;
    test_object *object = NULL;

    char  *string      = NULL;
    size_t string_size = 0;
    int written_count  = 0;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    cfg.comp    = mrp_string_comp;
    cfg.hash    = mrp_string_hash;
    cfg.free    = htbl_free_test_object;
    cfg.nbucket = 0; // nentry/4 -> smaller than min -> 8 by def
    cfg.nentry  = 10;

    table = mrp_htbl_create(&cfg);
    if (!table) {
        printf("blergh @ creating initial hash table\n");
        return 1;
    }

    // broken range: 12 - 66
    for (int i = 0; i < 12; i++) {
        object = mrp_allocz(sizeof(test_object));
        if (!object) {
            printf("blergh @ allocating object %d\n", i);
            return 1;
        }
        // allocz should handle this, but let's just have a test value written there
        object->pointer = NULL;

        string_size = snprintf(NULL, 0, LE_STRING, i);
        if (!string_size) {
            printf("blergh @ calculating string %d size\n", i);
            return 1;
        }
        // we need the null character as well
        string_size++;

        string = mrp_allocz(string_size);
        if (!string) {
            printf("blergh @ allocating string %d\n", i);
            return 1;
        }

        written_count = snprintf(string, string_size, LE_STRING, i);
        if (written_count <= 0 || written_count + 1 < (int)string_size) {
            printf("blergh @ writing string %d\n", i);
            return 1;
        }

        mrp_htbl_insert(table, string, object);
        mrp_htbl_remove(table, string, TRUE);
    }

    mrp_htbl_destroy(table, TRUE);
    printf("Successfully finished the test\n");
    return 0;
}
