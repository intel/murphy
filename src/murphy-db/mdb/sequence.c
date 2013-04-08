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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>


#ifndef SEQUENCE_STATISTICS
#define SEQUENCE_STATISTICS
#endif

#include <murphy-db/assert.h>
#include <murphy-db/sequence.h>

typedef struct mdb_sequence_entry_s {
    void         *key;
    void         *data;
} sequence_entry_t;


struct mdb_sequence_s {
    int                     alloc;
    mdb_sequence_compare_t  scomp;
    mdb_sequence_print_t    sprint;
#ifdef SEQUENCE_STATISTICS
    int                     max_entry;
#endif
    int                     size;
    int                     nentry;
    sequence_entry_t       *entries;
};



mdb_sequence_t *mdb_sequence_table_create(int                    alloc,
                                          mdb_sequence_compare_t scomp,
                                          mdb_sequence_print_t   sprint)
{
    mdb_sequence_t *seq;

    MDB_CHECKARG(scomp && sprint && alloc > 0 && alloc < 65536, NULL);

    if (!(seq = calloc(1, sizeof(mdb_sequence_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    seq->alloc  = alloc;
    seq->scomp  = scomp;
    seq->sprint = sprint;

    return seq;
}

int mdb_sequence_table_destroy(mdb_sequence_t *seq)
{
    MDB_CHECKARG(seq, -1);

    free(seq->entries);
    free(seq);

    return 0;
}

int mdb_sequence_table_get_size(mdb_sequence_t *seq)
{
    MDB_CHECKARG(seq, -1);

    return seq->nentry;
}

int mdb_sequence_table_reset(mdb_sequence_t *seq)
{
    MDB_CHECKARG(seq, -1);

    free(seq->entries);

    seq->size    = 0;
    seq->nentry  = 0;
    seq->entries = NULL;

    return 0;
}

int mdb_sequence_table_print(mdb_sequence_t *seq, char *buf, int len)
{
    sequence_entry_t *entry;
    char             *p, *e;
    int               i;
    char              key[256];

    MDB_CHECKARG(seq && buf && len > 0, 0);

    e = (p = buf) + len;
    *buf = '\0';

    for (i = 0;  i < seq->nentry && p < e;  i++) {
        entry = seq->entries + i;

        seq->sprint(entry->key, key, sizeof(key));

        p += snprintf(p, e-p, "   %05d: '%s' / %p\n", i, key, entry->data);
    }

    return p - buf;
}

int mdb_sequence_add(mdb_sequence_t *seq, int klen, void *key, void *data)
{
    sequence_entry_t *entry;
    int               nentry;
    size_t            old_size;
    size_t            length;
    int               cmp;
    int               min, max, i;

    MDB_CHECKARG(seq && key && data, -1);

    nentry = seq->nentry++;

    if ((nentry + 1) > seq->size) {
        old_size = seq->size;
        seq->size += seq->alloc;
        length = sizeof(sequence_entry_t) * seq->size;

        if (!(seq->entries = realloc(seq->entries, length))) {
            seq->nentry = 0;
            errno = ENOMEM;
            return 0;
        }


        memset(seq->entries + old_size, 0, 
               length - (old_size * sizeof(sequence_entry_t)));
    }

    for (min = 0,  i = (max = nentry)/2;   ;    i = (min+max)/2) {
        if (!(cmp = seq->scomp(klen, key, seq->entries[i].key)))
            break;

        if (i == min) {
            if (cmp > 0)
                i = max;
            break;
        }

        if (cmp < 0)
            max = i;
        else
            min = i;
    }

    entry = seq->entries + i;

    if (i < nentry) {
        memmove(entry+1, entry, sizeof(sequence_entry_t) * (nentry - i));
    }

    entry->key  = key;
    entry->data = data;

#ifdef SEQUENCE_STATISTICS
    if (seq->nentry > seq->max_entry)
        seq->max_entry = seq->nentry;
#endif

    return 0;
}

void *mdb_sequence_delete(mdb_sequence_t *seq, int klen, void *key)
{
    sequence_entry_t *entry;
    int               i;
    int               min, max;
    int               cmp;
    int               found;
    void             *data;
    size_t            length;

    MDB_CHECKARG(seq && key, NULL);

    for (found = 0, min = 0, i = (max = seq->nentry)/2;  ;  i = (min+max)/2) {
        entry = seq->entries + i;

        if (!(cmp = seq->scomp(klen, key, entry->key))) {
            found = 1;
            break;
        }

        if (i == min) {
            if (i != max) {
                entry = seq->entries + max;
                if (!seq->scomp(klen, key, entry->key))
                    found = 1;
            }
            break;
        }

        if (cmp < 0)
            max = i;
        else
            min = i;
    }

    if (!found) {
        errno = ENOENT;
        return NULL;
    }

    data = entry->data;

    if (--seq->nentry <= 0) {
        free(seq->entries);

        seq->size    = 0;
        seq->nentry  = 0;
        seq->entries = NULL;
    }
    else {
        if (i < seq->nentry) {
            length = sizeof(sequence_entry_t) * (seq->nentry - i);
            memmove(entry, entry+1, length);
        }

        if (seq->nentry <= (seq->size - seq->alloc)) {
            length = sizeof(sequence_entry_t) * (seq->size -= seq->alloc);

            if (!(seq->entries = realloc(seq->entries, length))) {
                seq->nentry = 0;
                errno = ENOMEM;
            }
        }
    }

    return data;
}



void *mdb_sequence_iterate(mdb_sequence_t *seq, void **cursor_ptr)
{
    typedef struct {
        int   index;
        int   nentry;
        void *entries[];
    } cursor_t;

    static cursor_t   empty_cursor;

    size_t            length;
    cursor_t         *cursor;
    int               i;

    MDB_CHECKARG(seq && cursor_ptr, NULL);

    if (!(cursor = *cursor_ptr)) {
        length = sizeof(cursor_t) + sizeof(void *) * seq->nentry;

        if (!(cursor = malloc(length)))
            return NULL;

        cursor->index = 0;
        cursor->nentry = seq->nentry;

        for (i = 0;  i < seq->nentry;  i++)
            cursor->entries[i] = seq->entries[i].data;

        *cursor_ptr = cursor;
    }

    if (cursor->index >= cursor->nentry) {
        if (*cursor_ptr != &empty_cursor) {
            *cursor_ptr = &empty_cursor;
            free(cursor);
        }
        return NULL;
    }

    return (void *)cursor->entries[cursor->index++];
}


void mdb_sequence_cursor_destroy(mdb_sequence_t *seq, void **cursor)
{
    (void)seq;

    if (cursor)
        free(*cursor);
}




/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
