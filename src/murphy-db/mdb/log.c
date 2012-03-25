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
    mdb_row_t      *before;
    mdb_row_t      *after;
} change_t;



static inline log_t *new_log(mdb_dlist_t *, mdb_dlist_t *, uint32_t, int);
static inline void delete_log(log_t *);
static inline log_t *get_last_vlog(mdb_dlist_t *);
static tx_log_t *get_tx_log(uint32_t);
static tbl_log_t *get_tbl_log(mdb_dlist_t *, mdb_dlist_t *, uint32_t,
                              mdb_table_t *);

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

    change->type   = type;
    change->before = before;
    change->after  = after;

    MDB_DLIST_PREPEND(change_t, link, change, &tblog->changes);

    return 0;
}

mdb_log_entry_t *mdb_log_transaction_iterate(uint32_t   depth,
                                             void     **cursor_ptr,
                                             int        delete)
{
    typedef struct {
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

    if ((cursor = *cursor_ptr))
        entry = &cursor->entry;
    else {
        if (!(txlog = (tx_log_t *)get_last_vlog(&tx_head)) ||
            depth > txlog->depth)
        {
            return NULL;
        }

        hhead = &txlog->hlink;

        if (MDB_DLIST_EMPTY(*hhead))
            return NULL;

        tblog = MDB_LIST_RELOCATE(tbl_log_t, hlink, hhead->next);

        if (MDB_DLIST_EMPTY(tblog->changes))
            return NULL;

        chead = &tblog->changes;

        if (!(*cursor_ptr = cursor = calloc(1, sizeof(cursor_t))))
            return NULL;
        else {
            entry = &cursor->entry;

            cursor->hhead = hhead;
            cursor->chead = chead;
            cursor->hlink = tblog->hlink.next;
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
            
            entry->change = change->type;
            entry->before = change->before;
            entry->after  = change->after;

            if (delete) {
                MDB_DLIST_UNLINK(change_t, link, change);
                free(change);
            }
            
            return entry;
        }

        if (cursor->hlink == cursor->hhead) {
            if (cursor != &empty_cursor) {
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
            cursor->clink = chead->next;
            
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
            
            entry->change = change->type;
            entry->before = change->before;
            entry->after  = change->after;

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

    if (!(log = (tbl_log_t *)get_last_vlog(vhead)) || depth > log->depth) {
        if ((log = (tbl_log_t *)new_log(vhead, hhead, depth, sizeof(*log)))) {
            log->table = tbl;
            MDB_DLIST_INIT(log->changes);
        }
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



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
