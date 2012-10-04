#include "message.h"

static int append_one_row(mrp_msg_t *msg, uint16_t tag, mqi_column_def_t *col,
                          int ncolumn, mrp_pep_value_t *data);

mrp_msg_t *create_register_message(mrp_pep_t *pep)
{
    mrp_msg_t        *msg;
    mrp_pep_table_t  *t;
    mqi_column_def_t *c;
    uint16_t          ncolumn, type;
    int               i, j;

    ncolumn = 0;
    for (i = 0; i < pep->nowned; i++)
        ncolumn += pep->owned[i].ncolumn;
    for (i = 0; i < pep->nwatched; i++)
        ncolumn += pep->watched[i].ncolumn;

    msg = mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_REGISTER),
                         MRP_PEPMSG_UINT32(MSGSEQ , 0),
                         MRP_PEPMSG_STRING(NAME   , pep->name),
                         MRP_PEPMSG_UINT16(NTABLE , pep->nowned),
                         MRP_PEPMSG_UINT16(NWATCH , pep->nwatched),
                         MRP_PEPMSG_UINT16(NCOLDEF, ncolumn),
                         MRP_MSG_END);

    for (i = 0, t = pep->owned; i < pep->nowned; i++, t++) {
        mrp_msg_append(msg, MRP_PEPMSG_STRING(TBLNAME, t->name));
        mrp_msg_append(msg, MRP_PEPMSG_UINT16(NCOLUMN, t->ncolumn));
        mrp_msg_append(msg, MRP_PEPMSG_SINT16(TBLIDX , t->idx_col));
        for (j = 0, c = t->columns; j < t->ncolumn; j++, c++) {
            if (c->type == mqi_varchar)
                type = mqi_blob + c->length;
            else
                type = c->type;

            mrp_msg_append(msg, MRP_PEPMSG_STRING(COLNAME, c->name));
            mrp_msg_append(msg, MRP_PEPMSG_UINT16(COLTYPE, type));
        }
    }

    for (i = 0, t = pep->watched; i < pep->nwatched; i++, t++) {
        mrp_msg_append(msg, MRP_PEPMSG_STRING(TBLNAME, t->name));
        mrp_msg_append(msg, MRP_PEPMSG_UINT16(NCOLUMN, t->ncolumn));
        for (j = 0, c = t->columns; j < t->ncolumn; j++, c++) {
            if (c->type == mqi_varchar)
                type = mqi_blob + c->length;
            else
                type = c->type;

            mrp_msg_append(msg, MRP_PEPMSG_STRING(COLNAME, c->name));
            mrp_msg_append(msg, MRP_PEPMSG_UINT16(COLTYPE, type));
        }
    }


    return msg;
}


int decode_register_message(mrp_msg_t *msg, mrp_pep_table_t *owned, int nowned,
                            mrp_pep_table_t *watched, int nwatched,
                            mqi_column_def_t *columns, int ncolumn)
{
    mrp_pep_table_t  *t;
    mqi_column_def_t *c;
    void             *it;
    char             *name;
    uint16_t          ntbl, nwch, ncol, type, idx_col;
    int               i, j, n;

    it = NULL;

    if (!mrp_msg_iterate_get(msg, &it,
                             MRP_PEPMSG_UINT16(NTABLE , &ntbl),
                             MRP_PEPMSG_UINT16(NWATCH , &nwch),
                             MRP_PEPMSG_UINT16(NCOLDEF, &ncol),
                             MRP_MSG_END))
        return FALSE;

    if (ntbl > nowned || nwch > nwatched || ncol > ncolumn)
        return FALSE;

    n = 0;
    c = columns;
    for (i = 0, t = owned; i < nowned; i++, t++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MRP_PEPMSG_STRING(TBLNAME, &name),
                                MRP_PEPMSG_UINT16(NCOLUMN, &ncol),
                                MRP_PEPMSG_SINT16(TBLIDX , &idx_col),
                                MRP_MSG_END)) {
            t->name    = name;
            t->columns = c;
            t->ncolumn = ncol;
            t->idx_col = idx_col;
        }
        else
            return FALSE;

        for (j = 0; j < t->ncolumn; j++, c++, n++) {
            if (n >= ncolumn)
                return FALSE;

            if (mrp_msg_iterate_get(msg, &it,
                                    MRP_PEPMSG_STRING(COLNAME, &name),
                                    MRP_PEPMSG_UINT16(COLTYPE, &type),
                                    MRP_MSG_END)) {
                c->name = name;

                if (type > mqi_blob) {
                    c->type   = mqi_varchar;
                    c->length = type - mqi_blob;
                }
                else
                    c->type = type;
            }
        }
    }

    for (i = 0, t = watched; i < nwatched; i++, t++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MRP_PEPMSG_STRING(TBLNAME, &name),
                                MRP_PEPMSG_UINT16(NCOLUMN, &ncol),
                                MRP_MSG_END)) {
            t->name    = name;
            t->columns = c;
            t->ncolumn = ncol;
            t->idx_col = -1;
        }
        else
            return FALSE;

        for (j = 0; j < t->ncolumn; j++, c++, n++) {
            if (n >= ncolumn)
                return FALSE;
            if (mrp_msg_iterate_get(msg, &it,
                                    MRP_PEPMSG_STRING(COLNAME, &name),
                                    MRP_PEPMSG_UINT16(COLTYPE, &type),
                                    MRP_MSG_END)) {
                c->name = name;

                if (type > mqi_blob) {
                    c->type   = mqi_varchar;
                    c->length = type - mqi_blob;
                }
                else {
                    c->type   = type;
                    c->length = 0;
                }

                c->flags = 0;
            }
        }
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
                          int ncolumn, mrp_pep_value_t *data, int nrow)
{
    mrp_pep_value_t *v;
    uint16_t         tid, nr;
    int              i;

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


int decode_notify_message(mrp_msg_t *msg, void **it, mrp_pep_data_t *data)
{
    int               i, j;
    mrp_pep_value_t  *v;
    mqi_column_def_t *d;

    v = data->columns;
    d = data->coldefs;

    for (i = 0; i < data->nrow; i++) {
        for (j = 0; j < data->ncolumn; j++) {
            switch (d[j].type) {
            case mqi_varchar:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_STRING(DATA, &v->str),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_integer:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_SINT32(DATA, &v->s32),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_unsignd:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_UINT32(DATA, &v->u32),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_floating:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_DOUBLE(DATA, &v->dbl),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            default:
                return FALSE;
            }

            v++;
        }
    }

    return TRUE;
}


mrp_msg_t *create_set_message(uint32_t seq, mrp_pep_data_t *data, int ndata)
{
    mrp_msg_t        *msg;
    mrp_pep_value_t  *vals;
    mqi_column_def_t *defs;
    uint16_t          utable, utotal, tid, nval, nrow;
    int               i, j;

    utable = ndata;
    utotal = 0;

    msg = mrp_msg_create(MRP_PEPMSG_UINT16(MSGTYPE, MRP_PEPMSG_SET),
                         MRP_PEPMSG_UINT32(MSGSEQ , seq),
                         MRP_PEPMSG_UINT16(NCHANGE, utable),
                         MRP_PEPMSG_UINT16(NTOTAL , 0),
                         MRP_MSG_END);

    if (msg != NULL) {
        for (i = 0; i < ndata; i++) {
            tid  = data[i].id;
            vals = data[i].columns;
            defs = data[i].coldefs;
            nval = data[i].ncolumn;
            nrow = data[i].nrow;

            if (!mrp_msg_append(msg, MRP_PEPMSG_UINT16(TBLID, tid)) ||
                !mrp_msg_append(msg, MRP_PEPMSG_UINT16(NROW , nrow)))
                goto fail;

            for (j = 0; j < nrow; j++) {
                if (!append_one_row(msg, MRP_PEPTAG_DATA, defs, nval, vals))
                    goto fail;
                vals   += nval;
                utotal += nval;
            }
        }

        mrp_msg_set(msg, MRP_PEPMSG_UINT16(NTOTAL, utotal));

        return msg;
    }

 fail:
    mrp_msg_unref(msg);
    return NULL;
}


int decode_set_message(mrp_msg_t *msg, void **it, mrp_pep_data_t *data)
{
    int               i, j;
    mrp_pep_value_t  *v;
    mqi_column_def_t *d;

    v = data->columns;
    d = data->coldefs;

    for (i = 0; i < data->nrow; i++) {
        for (j = 0; j < data->ncolumn; j++) {
            switch (d[j].type) {
            case mqi_varchar:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_STRING(DATA, &v->str),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_integer:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_SINT32(DATA, &v->s32),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_unsignd:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_UINT32(DATA, &v->u32),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            case mqi_floating:
                if (!mrp_msg_iterate_get(msg, it,
                                         MRP_PEPMSG_DOUBLE(DATA, &v->dbl),
                                         MRP_MSG_END))
                    return FALSE;
                break;

            default:
                return FALSE;
            }

            v++;
        }
    }

    return TRUE;
}


static int append_one_row(mrp_msg_t *msg, uint16_t tag, mqi_column_def_t *col,
                          int ncolumn, mrp_pep_value_t *data)
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
