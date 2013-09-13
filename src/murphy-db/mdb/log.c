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
#include <limits.h>
#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <string.h>

#include <murphy-db/assert.h>
#include <murphy-db/hash.h>
#include <murphy-db/sequence.h>
#include "log.h"
#include "row.h"
#include "table.h"

#ifndef LOG_STATISTICS
#define LOG_STATISTICS
#endif

#define LOG_COMMON_FIELDS   \
    mdb_dlist_t     vlink;  \
    mdb_dlist_t     hlink;  \
    uint32_t        depth

typedef struct {
    LOG_COMMON_FIELDS;
} log_t;

typedef struct {
    LOG_COMMON_FIELDS;
} tx_log_t;

typedef struct {
    LOG_COMMON_FIELDS;
    mdb_table_t *table;
    mdb_dlist_t  changes;
} tbl_log_t;

typedef struct {
    mdb_dlist_t     link;
    mdb_log_type_t  type;
    mqi_bitfld_t    colmask;
    union {
        mdb_row_t   *before;
        mdb_opcnt_t *cnt;
    };
    mdb_row_t      *after;
} change_t;



static inline log_t *new_log(mdb_dlist_t *, mdb_dlist_t *, uint32_t, int);
static inline void delete_log(log_t *);
static inline log_t *get_last_vlog(mdb_dlist_t *);
static tx_log_t *get_tx_log(uint32_t);
static tbl_log_t *get_tbl_log(mdb_dlist_t *, mdb_dlist_t *, uint32_t,
                              mdb_table_t *);
static void delete_tx_log(uint32_t);

static MDB_DLIST_HEAD(tx_head);

int mdb_log_create(mdb_table_t *tbl)
{
    MDB_CHECKARG(tbl, -1);

    MDB_DLIST_INIT(tbl->logs);

    return 0;
}


int mdb_log_change(mdb_table_t    *tbl,
                   uint32_t        depth,
                   mdb_log_type_t  type,
                   mqi_bitfld_t    colmask,
                   mdb_row_t      *before,
                   mdb_row_t      *after)
{
    tx_log_t  *txlog;
    tbl_log_t *tblog;
    change_t  *change;

    MDB_CHECKARG(tbl, -1);

    if (!depth)
        return 0;

    if (!(txlog = get_tx_log(depth)) ||
        !(tblog = get_tbl_log(&tbl->logs, &txlog->hlink, depth, tbl)))
    {
        return -1;
    }

    if (!(change = calloc(1, sizeof(change_t)))) {
        errno = ENOMEM;
        return -1;
    }

    change->type    = type;
    change->colmask = colmask;
    change->before  = before;
    change->after   = after;

    switch (type) {
    case mdb_log_insert: tbl->cnt.inserts++; break;
    case mdb_log_delete: tbl->cnt.deletes++; break;
    case mdb_log_update: tbl->cnt.updates++; break;
    default:                                 break;
    }

    MDB_DLIST_PREPEND(change_t, link, change, &tblog->changes);

    return 0;
}

mdb_log_entry_t *mdb_log_transaction_iterate(uint32_t   depth,
                                             void     **cursor_ptr,
                                             bool       forward,
                                             int        delete)
{
    typedef struct {
        uint32_t         depth;
        mdb_dlist_t     *hhead;
        mdb_dlist_t     *chead;
        mdb_dlist_t     *hlink;
        mdb_dlist_t     *clink;
        mdb_log_entry_t  entry;
    } cursor_t;

    static cursor_t  empty_cursor;

    cursor_t        *cursor;
    tx_log_t        *txlog;
    tbl_log_t       *tblog;
    mdb_dlist_t     *hhead;
    mdb_dlist_t     *chead;
    change_t        *change;
    mdb_log_entry_t *entry;

    MDB_CHECKARG(cursor_ptr, NULL);

    if (!depth)
        return NULL;

    if ((cursor = *cursor_ptr)) {
        if (cursor == &empty_cursor)
            return NULL;

        entry = &cursor->entry;
    }
    else {
        if (!(txlog = (tx_log_t *)get_last_vlog(&tx_head)))
            return NULL;

        if (depth > txlog->depth)
            return NULL;

        hhead = &txlog->hlink;

        if (MDB_DLIST_EMPTY(*hhead)) {
            if (delete)
                delete_log((log_t *)txlog);
            return NULL;
        }

        tblog = MDB_LIST_RELOCATE(tbl_log_t, hlink, hhead->next);

        if (MDB_DLIST_EMPTY(tblog->changes))
            return NULL;

        chead = &tblog->changes;

        if (!(*cursor_ptr = cursor = calloc(1, sizeof(cursor_t))))
            return NULL;
        else {
            entry = &cursor->entry;

            cursor->depth = txlog->depth;
            cursor->hhead = hhead;
            cursor->chead = chead;
            cursor->hlink = tblog->hlink.next;
            cursor->clink = forward ? chead->next : chead->prev;

            entry->table = tblog->table;
        }
    }

    for (;;) {
        if (cursor->clink == cursor->chead) {
            if (delete) {
                tblog = MDB_LIST_RELOCATE(tbl_log_t, changes, cursor->chead);
                delete_log((log_t *)tblog);
            }
        }
        else {
            change = MDB_LIST_RELOCATE(change_t, link, cursor->clink);

            cursor->clink = forward ? change->link.next : change->link.prev;

            entry->change  = change->type;
            entry->colmask = change->colmask;
            entry->before  = change->before;
            entry->after   = change->after;

            if (delete) {
                MDB_DLIST_UNLINK(change_t, link, change);
                free(change);
            }

            return entry;
        }

        if (cursor->hlink == cursor->hhead) {
            if (cursor != &empty_cursor) {
                if (delete)
                    delete_tx_log(cursor->depth);
                *cursor_ptr = &empty_cursor;
                free(cursor);
            }
            return NULL;
        }
        else {
            tblog = MDB_LIST_RELOCATE(tbl_log_t, hlink, cursor->hlink);
            chead = &tblog->changes;

            cursor->hlink = tblog->hlink.next;
            cursor->chead = chead;
            cursor->clink = forward ? chead->next : chead->prev;

            entry->table  = tblog->table;
        }
    }
}



mdb_log_entry_t *mdb_log_table_iterate(mdb_table_t  *tbl,
                                       void        **cursor_ptr,
                                       int           delete)
{
    typedef struct {
        mdb_dlist_t     *vhead;
        mdb_dlist_t     *chead;
        mdb_dlist_t     *vlink;
        mdb_dlist_t     *clink;
        mdb_log_entry_t  entry;
    } cursor_t;

    static cursor_t  empty_cursor;

    cursor_t        *cursor;
    tbl_log_t       *tblog;
    mdb_dlist_t     *vhead;
    mdb_dlist_t     *chead;
    change_t        *change;
    mdb_log_entry_t *entry;

    MDB_CHECKARG(tbl && cursor_ptr, NULL);

    if ((cursor = *cursor_ptr))
        entry = &cursor->entry;
    else {
        vhead = &tbl->logs;

        if (MDB_DLIST_EMPTY(*vhead))
            return NULL;

        if (!(tblog = (tbl_log_t *)get_last_vlog(vhead)))
            return NULL;

        if (tblog->table != tbl || MDB_DLIST_EMPTY(tblog->changes))
            return NULL;

        chead = &tblog->changes;

        if (!(*cursor_ptr = cursor = calloc(1, sizeof(cursor_t))))
            return NULL;
        else {
            entry = &cursor->entry;

            cursor->vhead = vhead;
            cursor->chead = chead;
            cursor->vlink = tblog->vlink.prev;
            cursor->clink = chead->next;

            entry->table = tblog->table;
        }
    }

    for (;;) {
        if (cursor->clink == cursor->chead) {
            if (delete) {
                tblog = MDB_LIST_RELOCATE(tbl_log_t, changes, cursor->chead);
                delete_log((log_t *)tblog);
            }
        }
        else {
            change = MDB_LIST_RELOCATE(change_t, link, cursor->clink);

            cursor->clink = change->link.next;

            entry->change  = change->type;
            entry->colmask = change->colmask;
            entry->before  = change->before;
            entry->after   = change->after;

            if (delete) {
                MDB_DLIST_UNLINK(change_t, link, change);
                free(change);
            }

            return entry;
        }

        if (cursor->vlink == cursor->vhead) {
            if (cursor != &empty_cursor) {
                *cursor_ptr = &empty_cursor;
                free(cursor);
            }
            return NULL;
        }
        else {
            tblog = MDB_LIST_RELOCATE(tbl_log_t, vlink, cursor->vlink);
            chead = &tblog->changes;

            cursor->vlink = tblog->vlink.prev;
            cursor->chead = chead;
            cursor->clink = chead->next;

            if (tbl != tblog->table)
                return NULL;
        }
    }
}



static inline log_t *new_log(mdb_dlist_t *vhead,
                             mdb_dlist_t *hhead,
                             uint32_t     depth,
                             int          size)
{
    log_t *log;

    if ((log = calloc(1, size))) {
        MDB_DLIST_APPEND(mdb_log_t, vlink, log, vhead);

        if (hhead)
            MDB_DLIST_APPEND(mdb_log_t, hlink, log, hhead);
        else
            MDB_DLIST_INIT(log->hlink);

        log->depth = depth;
    }

    return log;
}

static inline void delete_log(log_t *log)
{
    MDB_DLIST_UNLINK(log_t, vlink, log);
    MDB_DLIST_UNLINK(log_t, hlink, log);

    free(log);
}


static inline log_t *get_last_vlog(mdb_dlist_t *vhead)
{
    if (MDB_DLIST_EMPTY(*vhead))
        return NULL;

    return MDB_LIST_RELOCATE(log_t, vlink, vhead->prev);
}


static tx_log_t *get_tx_log(uint32_t depth)
{
    tx_log_t *log;

    if (!(log = (tx_log_t *)get_last_vlog(&tx_head)) || depth > log->depth) {
        return (tx_log_t *)new_log(&tx_head, NULL, depth, sizeof(*log));
    }

    if (depth < log->depth) {
        errno = ENOKEY;
        return NULL;
    }

    return log;
}

static tbl_log_t *get_tbl_log(mdb_dlist_t *vhead,
                              mdb_dlist_t *hhead,
                              uint32_t     depth,
                              mdb_table_t *tbl)
{
    tbl_log_t *log;
    change_t  *change;

    if (!(log = (tbl_log_t *)get_last_vlog(vhead)) || depth > log->depth) {
        if ((log = (tbl_log_t *)new_log(vhead, hhead, depth, sizeof(*log)))) {
            log->table = tbl;
            MDB_DLIST_INIT(log->changes);

            if (!(change = calloc(1, sizeof(change_t)))) {
                errno = ENOMEM;
                return NULL;
            }

            if (!(change->cnt = calloc(1, sizeof(*change->cnt)))) {
                free(change);
                errno = ENOMEM;
                return NULL;
            }

            change->type = mdb_log_start;
            *change->cnt = tbl->cnt;
            tbl->cnt.stamp++;

            MDB_DLIST_PREPEND(change_t, link, change, &log->changes);
        }
    }

    if (!log) {
        errno = ENOMEM;
        return NULL;
    }

    if (tbl != log->table) {
        errno = EINVAL;
        return NULL;
    }

    if (depth < log->depth) {
        errno = ENOKEY;
        return NULL;
    }

    return log;
}

static void delete_tx_log(uint32_t depth)
{
    log_t *log;

    if ((log = get_last_vlog(&tx_head)) && depth == log->depth)
        delete_log(log);
}



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
