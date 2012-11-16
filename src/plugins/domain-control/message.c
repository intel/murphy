/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include "message.h"

static int append_one_row(mrp_msg_t *msg, uint16_t tag, mqi_column_def_t *col,
                          int ncolumn, mrp_domctl_value_t *data);

mrp_msg_t *create_register_message(mrp_domctl_t *dc)
{
    mrp_msg_t          *msg;
    mrp_domctl_table_t *t;
    mrp_domctl_watch_t *w;
    int                 i;

    msg = mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_REGISTER),
                         MRP_PEPMSG_UINT32(MSGSEQ , 0),
                         MRP_PEPMSG_STRING(NAME   , dc->name),
                         MRP_PEPMSG_UINT16(NTABLE , dc->ntable),
                         MRP_PEPMSG_UINT16(NWATCH , dc->nwatch),
                         MRP_MSG_END);

    for (i = 0, t = dc->tables; i < dc->ntable; i++, t++) {
        mrp_msg_append(msg, MRP_PEPMSG_STRING(TBLNAME, t->table));
        mrp_msg_append(msg, MRP_PEPMSG_STRING(COLUMNS, t->mql_columns));
        mrp_msg_append(msg, MRP_PEPMSG_STRING(INDEX  , t->mql_index));
    }

    for (i = 0, w = dc->watches; i < dc->nwatch; i++, w++) {
        mrp_msg_append(msg, MRP_PEPMSG_STRING(TBLNAME, w->table));
        mrp_msg_append(msg, MRP_PEPMSG_STRING(COLUMNS, w->mql_columns));
        mrp_msg_append(msg, MRP_PEPMSG_STRING(WHERE  , w->mql_where));
        mrp_msg_append(msg, MRP_PEPMSG_UINT16(MAXROWS, w->max_rows));
    }


    return msg;
}


int decode_register_message(mrp_msg_t *msg,
                            mrp_domctl_table_t *tables, int ntable,
                            mrp_domctl_watch_t *watches, int nwatch)
{
    mrp_domctl_table_t *t;
    mrp_domctl_watch_t *w;
    void               *it;
    char               *table, *columns, *index, *where;
    uint16_t            ntbl, nwch, max_rows;
    int                 i;

    it = NULL;

    if (!mrp_msg_iterate_get(msg, &it,
                             MRP_PEPMSG_UINT16(NTABLE , &ntbl),
                             MRP_PEPMSG_UINT16(NWATCH , &nwch),
                             MRP_MSG_END))
        return FALSE;

    if (ntbl > ntable || nwch > nwatch)
        return FALSE;

    for (i = 0, t = tables; i < ntable; i++, t++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MRP_PEPMSG_STRING(TBLNAME, &table),
                                MRP_PEPMSG_STRING(COLUMNS, &columns),
                                MRP_PEPMSG_STRING(INDEX  , &index),
                                MRP_MSG_END)) {
            t->table       = table;
            t->mql_columns = columns;
            t->mql_index   = index;
        }
        else
            return FALSE;
    }

    for (i = 0, w = watches; i < nwatch; i++, w++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MRP_PEPMSG_STRING(TBLNAME, &table),
                                MRP_PEPMSG_STRING(COLUMNS, &columns),
                                MRP_PEPMSG_STRING(WHERE  , &where),
                                MRP_PEPMSG_UINT16(MAXROWS, &max_rows),
                                MRP_MSG_END)) {
            w->table       = table;
            w->mql_columns = columns;
            w->mql_where   = where;
            w->max_rows    = max_rows;
        }
        else
            return FALSE;
    }

    return TRUE;
}


mrp_msg_t *create_ack_message(uint32_t seq)
{
    return mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_ACK),
                          MRP_PEPMSG_UINT32(MSGSEQ , seq),
                          MRP_MSG_END);
}


mrp_msg_t *create_nak_message(uint32_t seq, int error, const char *errmsg)
{
    return mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_NAK),
                          MRP_PEPMSG_UINT32(MSGSEQ , seq),
                          MRP_PEPMSG_SINT32(ERRCODE, error),
                          MRP_PEPMSG_STRING(ERRMSG , errmsg),
                          MRP_MSG_END);
}


mrp_msg_t *create_notify_message(void)
{
    return mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_NOTIFY),
                          MRP_PEPMSG_UINT32(MSGSEQ , 0),
                          MRP_PEPMSG_UINT16(NCHANGE, 0),
                          MRP_PEPMSG_UINT16(NTOTAL , 0),
                          MRP_MSG_END);
}


int update_notify_message(mrp_msg_t *msg, int id, mqi_column_def_t *columns,
                          int ncolumn, mrp_domctl_value_t *data, int nrow)
{
    mrp_domctl_value_t *v;
    uint16_t           tid, nr;
    int                i;

    nr  = nrow;
    tid = id;

    if (!mrp_msg_append(msg, MRP_PEPMSG_UINT16(TBLID, tid)) ||
        !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NROW , nr )))
        return FALSE;

    for (i = 0, v = data; i < nrow; i++, v += ncolumn) {
        if (!append_one_row(msg, MRP_PEPTAG_DATA, columns, ncolumn, v))
            return FALSE;
    }

    return TRUE;
}


mrp_msg_t *create_set_message(uint32_t seq, mrp_domctl_data_t *tables,
                              int ntable)
{
    mrp_msg_t          *msg;
    mrp_domctl_value_t *rows, *col;
    uint16_t            utable, utotal, tid, ncol, nrow;
    int                 i, r, c;

    utable = ntable;
    utotal = 0;

    msg = mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_SET),
                         MRP_PEPMSG_UINT32(MSGSEQ , seq),
                         MRP_PEPMSG_UINT16(NCHANGE, utable),
                         MRP_PEPMSG_UINT16(NTOTAL , 0),
                         MRP_MSG_END);

    if (msg != NULL) {
        for (i = 0; i < ntable; i++) {
            tid  = tables[i].id;
            ncol = tables[i].ncolumn;
            nrow = tables[i].nrow;

            if (!mrp_msg_append(msg, MRP_PEPMSG_UINT16(TBLID, tid))  ||
                !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NROW , nrow)) ||
                !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NCOL , ncol)))
                goto fail;

            for (r = 0; r < nrow; r++) {
                rows = tables[i].rows[r];

                for (c = 0; c < ncol; c++) {
                    col = rows + c;
#define HANDLE_TYPE(pt, t, m)                                                  \
                    case MRP_DOMCTL_##pt:                                      \
                        if (!mrp_msg_append(msg, MRP_PEPMSG_##t(DATA,col->m))) \
                            goto fail;                                         \
                        break;

                    switch (col->type) {
                        HANDLE_TYPE(STRING  , STRING, str);
                        HANDLE_TYPE(INTEGER , SINT32, s32);
                        HANDLE_TYPE(UNSIGNED, UINT32, u32);
                        HANDLE_TYPE(DOUBLE  , DOUBLE, dbl);
                    default:
                        goto fail;
                    }
#undef HANDLE_TYPE
                }
            }
            utotal += nrow * ncol;
        }

        mrp_msg_set(msg, MRP_PEPMSG_UINT16(NTOTAL, utotal));

        return msg;
    }

 fail:
    mrp_msg_unref(msg);
    return NULL;
}


static int append_one_row(mrp_msg_t *msg, uint16_t tag, mqi_column_def_t *col,
                          int ncolumn, mrp_domctl_value_t *data)
{
#define HANDLE_TYPE(dbtype, type, member)                                 \
    case mqi_##dbtype:                                                    \
        if (!mrp_msg_append(msg, MRP_MSG_TAG_##type(tag, data->member)))  \
            return FALSE;                                                 \
        break

    int i;

    for (i = 0; i < ncolumn; i++, data++, col++) {
        switch (col->type) {
            HANDLE_TYPE(integer , SINT32, s32);
            HANDLE_TYPE(unsignd , UINT32, u32);
            HANDLE_TYPE(floating, DOUBLE, dbl);
            HANDLE_TYPE(string  , STRING, str);
        case mqi_blob:
        default:
            return FALSE;
        }
    }

    return TRUE;

#undef HANDLE_TYPE
}
