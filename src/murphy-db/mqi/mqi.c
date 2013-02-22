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

#include <murphy-db/assert.h>
#include <murphy-db/handle.h>
#include <murphy-db/mqi.h>
#include <murphy-db/handle.h>
#include <murphy-db/hash.h>
#include "mdb-backend.h"

#define MAX_DB 2

#define TX_DEPTH_BITS  4
#define TX_USEID_BITS  ((sizeof(mqi_handle_t) * 8) - TX_DEPTH_BITS)
#define TX_DEPTH_MAX   (((mqi_handle_t)1) << TX_DEPTH_BITS)
#define TX_USEID_MAX   (((mqi_handle_t)1) << TX_USEID_BITS)
#define TX_DEPTH_MASK  (TX_DEPTH_MAX - 1)
#define TX_USEID_MASK  (TX_USEID_MAX - 1)

#define TX_DEPTH(h)    ((h) & TX_DEPTH_MASK)
#define TX_USEID(h)    ((h) & (TX_USEID_MASK << TX_DEPTH_BITS))

#define TX_HANDLE(useid, depth)                                         \
    (((useid) & (TX_USEID_MASK << TX_DEPTH_BITS)) | ((depth) & TX_DEPTH_MASK))

#define TX_USEID_INCREMENT(u)                                   \
    (u) = ((((u) + ((mqi_handle_t)1)) << TX_DEPTH_BITS) &       \
           (TX_USEID_MASK << TX_DEPTH_BITS))


#if MQI_TXDEPTH_MAX > (1 << TX_DEPTH_BITS)
#error "Too few TX_DEPTH_BITS to represent MQI_TXDEPTH_MAX"
#endif

#define DB_TYPE(db) ((db)->flags & MQI_TABLE_TYPE_MASK)

#define GET_TABLE(tbl, ftb, h, errval)                                      \
    do {                                                                    \
        mqi_table_t *t;                                                     \
        mqi_db_t *db;                                                       \
        if (!(t = mdb_handle_get_data(table_handle, h)) || !(db = t->db)) { \
            errno = ENOENT;                                                 \
            return errval;                                                  \
        }                                                                   \
        if (!(tbl = t->handle) || !(ftb = db->functbl)) {                   \
            errno = EIO;                                                    \
            return errval;                                                  \
        }                                                                   \
    } while(0)

typedef struct {
    const char       *engine;
    uint32_t          flags;
    mqi_db_functbl_t *functbl;
} mqi_db_t;

typedef struct {
    mqi_db_t    *db;
    void        *handle;
} mqi_table_t;

typedef struct {
    uint32_t useid;
    uint32_t txid[MAX_DB];
} mqi_transaction_t;


static int db_register(const char *, uint32_t, mqi_db_functbl_t *);


static int        ndb;
static mqi_db_t   *dbs;
mdb_handle_map_t  *table_handle;
mdb_hash_t        *table_name_hash;
mdb_handle_map_t  *transact_handle;
mqi_transaction_t  txstack[MQI_TXDEPTH_MAX];
int                txdepth;


int mqi_open(void)
{
    if (!ndb && !dbs) {
        if (!(dbs = calloc(MAX_DB, sizeof(mqi_db_t)))) {
            errno = ENOMEM;
            return -1;
        }

        table_handle = MDB_HANDLE_MAP_CREATE();
        table_name_hash = MDB_HASH_TABLE_CREATE(varchar, 256);

        transact_handle = MDB_HANDLE_MAP_CREATE();

        if (db_register("MurphyDB", MQI_TEMPORARY, mdb_backend_init()) < 0) {
            errno = EIO;
            return -1;
        }
    }

    return 0;
}

int mqi_close(void)
{
    int i;

    if (ndb > 0 && dbs) {
        for (i = 0; i < ndb; i++)
            free((void *)dbs[i].engine);

        free(dbs);

        MDB_HANDLE_MAP_DESTROY(table_handle);
        MDB_HASH_TABLE_DESTROY(table_name_hash);
        MDB_HANDLE_MAP_DESTROY(transact_handle);

        table_handle = NULL;
        table_name_hash = NULL;
        transact_handle = NULL;

        dbs = NULL;
        ndb = 0;
    }

    return 0;
}


int mqi_show_tables(uint32_t flags, char **buf, int len)
{
    mqi_handle_t h;
    mqi_table_t *tbl;
    mqi_db_t *db;
    void *data;
    char *name;
    void *cursor;
    int   i = 0;
    int   j;

    MDB_CHECKARG(buf && len > 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    MDB_HASH_TABLE_FOR_EACH_WITH_KEY(table_name_hash, data, name, cursor) {
        if (i >= len) {
            errno = EOVERFLOW;
            return -1;
        }

        if ((h = data - NULL) == MQI_HANDLE_INVALID)
            continue;

        if (!(tbl = mdb_handle_get_data(table_handle, h)) || !(db = tbl->db))
            continue;

        if (!(DB_TYPE(db) & flags))
            continue;

        for (j = 0; j < i;  j++) {
            if (strcasecmp(name, buf[j]) < 0) {
                memmove(buf + (j+1), buf + j, sizeof(char *) * (i-j));
                break;
            }
        }
        buf[j] = name;

        i++;
    }

    return i;
}


int mqi_create_transaction_trigger(mqi_trigger_cb_t callback, void *user_data)
{
    mqi_db_t         *db;
    mqi_db_functbl_t *ftb;
    int               i;

    MDB_CHECKARG(callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    for (i = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->create_transaction_trigger(callback, user_data) < 0) {

            for (i--;  i >= 0;  i--) {
                db  = dbs + i;
                ftb = db->functbl;

                ftb->drop_transaction_trigger(callback, user_data);
            }

            return -1;
        }
    }

    return 0;
}

int mqi_create_table_trigger(mqi_trigger_cb_t callback, void *user_data)
{
    mqi_db_t         *db;
    mqi_db_functbl_t *ftb;
    int               i;

    MDB_CHECKARG(callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    for (i = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->create_table_trigger(callback, user_data) < 0) {

            for (i--;  i >= 0;  i--) {
                db  = dbs + i;
                ftb = db->functbl;

                ftb->drop_table_trigger(callback, user_data);
            }

            return -1;
        }
    }

    return 0;
}


int mqi_create_row_trigger(mqi_handle_t h,
                           mqi_trigger_cb_t callback,
                           void *user_data,
                           mqi_column_desc_t *cds)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->create_row_trigger(tbl, callback, user_data, cds);
}


int mqi_create_column_trigger(mqi_handle_t h,
                              int colidx,
                              mqi_trigger_cb_t callback,
                              void *user_data,
                              mqi_column_desc_t *cds)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->create_column_trigger(tbl, colidx, callback, user_data, cds);
}


int mqi_drop_transaction_trigger(mqi_trigger_cb_t callback, void *user_data)
{
    mqi_db_t         *db;
    mqi_db_functbl_t *ftb;
    int               sts;
    int               i;

    MDB_CHECKARG(callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    for (sts = 0, i = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->drop_transaction_trigger(callback, user_data) < 0)
            sts = -1;
    }

    return sts;
}


int mqi_drop_table_trigger(mqi_trigger_cb_t callback, void *user_data)
{
    mqi_db_t         *db;
    mqi_db_functbl_t *ftb;
    int               sts;
    int               i;

    MDB_CHECKARG(callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    for (sts = 0, i = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->drop_table_trigger(callback, user_data) < 0)
            sts = -1;
    }

    return sts;
}


int mqi_drop_row_trigger(mqi_handle_t h,
                         mqi_trigger_cb_t callback,
                         void *user_data)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->drop_row_trigger(tbl, callback, user_data);
}


int mqi_drop_column_trigger(mqi_handle_t h,
                            int colidx,
                            mqi_trigger_cb_t callback,
                            void *user_data)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && callback, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->drop_column_trigger(tbl, colidx, callback, user_data);
}


mqi_handle_t mqi_begin_transaction(void)
{
    mqi_transaction_t *tx;
    mqi_db_t          *db;
    mqi_db_functbl_t  *ftb;
    uint32_t           depth;
    int                i;

    MDB_PREREQUISITE(dbs && ndb > 0 && transact_handle, MQI_HANDLE_INVALID);
    MDB_ASSERT(txdepth < MQI_TXDEPTH_MAX - 1, EOVERFLOW, MQI_HANDLE_INVALID);

    depth = txdepth++;
    tx = txstack + depth;

    TX_USEID_INCREMENT(tx->useid);

    for (i = 0; i < ndb; i++) {
        db  = dbs + i;
        ftb = db->functbl;
        tx->txid[i] = ftb->begin_transaction();
    }

    return TX_HANDLE(tx->useid, depth);
}


int mqi_commit_transaction(mqi_handle_t h)
{
    uint32_t           depth = TX_DEPTH(h);
    uint32_t           useid = TX_USEID(h);
    mqi_transaction_t *tx;
    mqi_db_t          *db;
    mqi_db_functbl_t  *ftb;
    int                err;
    int                i;

    MDB_CHECKARG(h != MQI_HANDLE_INVALID && depth < MQI_TXDEPTH_MAX, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);
    MDB_ASSERT(txdepth > 0 && depth == (uint32_t)txdepth - 1, EBADSLT, -1);

    tx = txstack + depth;

    MDB_ASSERT(tx->useid == useid, EBADSLT, -1);

    for (i = 0, err = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->commit_transaction(tx->txid[i]) < 0)
            err = -1;
    }

    txdepth--;

    return err;
}

int mqi_rollback_transaction(mqi_handle_t h)
{
    uint32_t           depth = TX_DEPTH(h);
    uint32_t           useid = TX_USEID(h);
    mqi_transaction_t *tx;
    mqi_db_t          *db;
    mqi_db_functbl_t  *ftb;
    int                err;
    int                i;

    MDB_CHECKARG(h != MQI_HANDLE_INVALID && depth < MQI_TXDEPTH_MAX, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);
    MDB_ASSERT(txdepth > 0 && depth == (uint32_t)txdepth - 1, EBADSLT, -1);

    tx = txstack + depth;

    MDB_ASSERT(tx->useid == useid, EBADSLT, -1);

    for (i = 0, err = 0;  i < ndb;  i++) {
        db  = dbs + i;
        ftb = db->functbl;

        if (ftb->rollback_transaction(tx->txid[i]) < 0)
            err = -1;
    }

    txdepth--;

    return err;
}

mqi_handle_t mqi_get_transaction_handle(void)
{
    uint32_t           depth;
    mqi_transaction_t *tx;

    MDB_CHECKARG(txdepth > 0, MQI_HANDLE_INVALID);
    MDB_PREREQUISITE(dbs && ndb > 0, MQI_HANDLE_INVALID);

    depth = txdepth - 1;
    tx = txstack + depth;

    return TX_HANDLE(tx->useid, depth);
}


uint32_t mqi_get_transaction_depth(void)
{
    return txdepth;
}


mqi_handle_t mqi_create_table(char *name,
                              uint32_t flags,
                              char **index_columns,
                              mqi_column_def_t *cdefs)
{
    mqi_db_t         *db;
    mqi_db_functbl_t *ftb;
    mqi_table_t      *tbl = NULL;
    mqi_handle_t      h = MQI_HANDLE_INVALID;
    char             *namedup = NULL;
    int               i;

    MDB_CHECKARG(name && cdefs, MQI_HANDLE_INVALID);
    MDB_PREREQUISITE(dbs && ndb > 0, MQI_HANDLE_INVALID);

    for (i = 0, ftb = NULL;  i < ndb;  i++) {
        db = dbs + i;

        if ((DB_TYPE(db) & flags) != 0) {
            ftb = db->functbl;
            break;
        }
    }

    MDB_ASSERT(ftb, ENOENT, MQI_HANDLE_INVALID);

    if(!(tbl = calloc(1, sizeof(mqi_table_t))))
        return MQI_HANDLE_INVALID;

    tbl->db = db;
    tbl->handle = NULL;

    if (!(namedup = strdup(name)))
        goto cleanup;

    if (!(tbl->handle = ftb->create_table(name, index_columns, cdefs)))
        goto cleanup;

    if ((h = mdb_handle_add(table_handle, tbl)) == MQI_HANDLE_INVALID)
        goto cleanup;

    if (mdb_hash_add(table_name_hash, 0,namedup, NULL + h) < 0) {
        mdb_handle_delete(table_handle, h);
        h = MQI_HANDLE_INVALID;
    }

    ftb->register_table_handle(tbl->handle, h);

    return h;

 cleanup:
    if (tbl) {
        if (tbl->handle) {
            mdb_handle_delete(table_handle, h);
            ftb->drop_table(tbl->handle);
        }
        mdb_hash_delete(table_name_hash, 0,name);
        free(namedup);
        free(tbl);
    }

    return MDB_HANDLE_INVALID;
}


int mqi_create_index(mqi_handle_t h, char **index_columns)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && index_columns, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->create_index(tbl, index_columns);
}

int mqi_drop_table(mqi_handle_t h)
{
    mqi_table_t      *tbl;
    mqi_db_functbl_t *ftb;
    char             *name;
    void             *data;
    void             *cursor;
    int               sts;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    if (!(tbl = mdb_handle_delete(table_handle, h)))
        return -1;

    ftb = tbl->db->functbl;

    MDB_HASH_TABLE_FOR_EACH_WITH_KEY_SAFE(table_name_hash, data,name, cursor) {
        if ((mqi_handle_t)(data - NULL) == h) {
            mdb_hash_delete(table_name_hash, 0,name);
            sts = ftb->drop_table(tbl->handle);
            free(name);
            free(tbl);
            return sts;
        }
    }

    return -1;
}

int mqi_describe(mqi_handle_t h, mqi_column_def_t *defs, int len)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && defs && len > 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->describe(tbl, defs, len);
}

int mqi_insert_into(mqi_handle_t         h,
                    int                 ignore,
                    mqi_column_desc_t  *cds,
                    void              **data)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && cds && data && data[0], -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->insert_into(tbl, ignore, cds, data);
}

int mqi_select(mqi_handle_t       h,
               mqi_cond_entry_t  *cond,
               mqi_column_desc_t *cds,
               void              *rows,
               int                rowsize,
               int                dim)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && cds &&
                 rows && rowsize > 0 && dim > 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->select(tbl, cond, cds, rows, rowsize, dim);
}

int mqi_select_by_index(mqi_handle_t       h,
                        mqi_variable_t    *idxvars,
                        mqi_column_desc_t *cds,
                        void              *result)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && idxvars && cds && result, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->select_by_index(tbl, idxvars, cds, result);
}

int mqi_update(mqi_handle_t       h,
               mqi_cond_entry_t  *cond,
               mqi_column_desc_t *cds,
               void              *data)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && cds && data, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->update(tbl, cond, cds, data);
}

int mqi_delete_from(mqi_handle_t h, mqi_cond_entry_t *cond)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->delete_from(tbl, cond);
}

mqi_handle_t mqi_get_table_handle(char *table_name)
{
    void *data;

    MDB_CHECKARG(table_name, MQI_HANDLE_INVALID);
    MDB_PREREQUISITE(dbs && ndb > 0, MQI_HANDLE_INVALID);

    data = mdb_hash_get_data(table_name_hash, 0,table_name);

    if (data != NULL)
        return data - NULL;
    else
        return MQI_HANDLE_INVALID;
}


int mqi_get_column_index(mqi_handle_t h, char *column_name)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && column_name, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->get_column_index(tbl, column_name);
}

int mqi_get_table_size(mqi_handle_t h)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return  ftb->get_table_size(tbl);
}

uint32_t mqi_get_table_stamp(mqi_handle_t h)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID, MQI_STAMP_NONE);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return  ftb->get_table_stamp(tbl);
}

char *mqi_get_column_name(mqi_handle_t h, int colidx)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && colidx >= 0, NULL);
    MDB_PREREQUISITE(dbs && ndb > 0, NULL);

    GET_TABLE(tbl, ftb, h, NULL);

    return  ftb->get_column_name(tbl, colidx);
}

mqi_data_type_t mqi_get_column_type(mqi_handle_t h, int colidx)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && colidx >= 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return  ftb->get_column_type(tbl, colidx);
}

int mqi_get_column_size(mqi_handle_t h, int colidx)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && colidx >= 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return  ftb->get_column_size(tbl, colidx);
}

int mqi_print_rows(mqi_handle_t h, char *buf, int len)
{
    mqi_db_functbl_t *ftb;
    void             *tbl;

    MDB_CHECKARG(h != MDB_HANDLE_INVALID && buf && len > 0, -1);
    MDB_PREREQUISITE(dbs && ndb > 0, -1);

    GET_TABLE(tbl, ftb, h, -1);

    return ftb->print_rows(tbl, buf, len);
}



static int db_register(const char       *engine,
                       uint32_t          flags,
                       mqi_db_functbl_t *functbl)
{
    mqi_db_t *db;
    int i;

    MDB_CHECKARG(engine && engine[0] && functbl, -1);
    MDB_PREREQUISITE(dbs, -1);

    if (ndb + 1 >= MAX_DB) {
        errno = EOVERFLOW;
        return -1;
    }

    for (i = 0; i < ndb;  i++) {
        if (!strcmp(engine, dbs[i].engine)) {
            errno = EEXIST;
            return -1;
        }
    }

    db = dbs + ndb++;

    db->engine  = strdup(engine);
    db->flags   = flags;
    db->functbl = functbl;

    return 0;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
