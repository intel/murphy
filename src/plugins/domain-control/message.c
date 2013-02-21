#include <murphy/common/mm.h>
#include <murphy/common/msg.h>
#include <murphy/common/json.h>

#include <murphy-db/mql.h>

#include "message.h"


static void unref_wire(msg_t *msg)
{
    if (msg->any.wire && msg->any.unref_wire) {
        msg->any.unref_wire(msg->any.wire);
        msg->any.wire = NULL;
    }
}


static void msg_unref_wire(void *wire)
{
    mrp_msg_unref((mrp_msg_t *)wire);
}


static void msg_free_register(msg_t *msg)
{
    register_msg_t *reg = (register_msg_t *)msg;

    if (reg != NULL) {
        mrp_free(reg->tables);
        mrp_free(reg->watches);
        unref_wire(msg);

        mrp_free(reg);
    }
}


mrp_msg_t *msg_encode_register(register_msg_t *reg)
{
    mrp_msg_t          *msg;
    mrp_domctl_table_t *t;
    mrp_domctl_watch_t *w;
    int                 i;

    msg = mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_REGISTER),
                         MSG_UINT32(MSGSEQ , 0),
                         MSG_STRING(NAME   , reg->name),
                         MSG_UINT16(NTABLE , reg->ntable),
                         MSG_UINT16(NWATCH , reg->nwatch),
                         MSG_END);

    for (i = 0, t = reg->tables; i < reg->ntable; i++, t++) {
        mrp_msg_append(msg, MSG_STRING(TBLNAME, t->table));
        mrp_msg_append(msg, MSG_STRING(COLUMNS, t->mql_columns));
        mrp_msg_append(msg, MSG_STRING(INDEX  , t->mql_index));
    }

    for (i = 0, w = reg->watches; i < reg->nwatch; i++, w++) {
        mrp_msg_append(msg, MSG_STRING(TBLNAME, w->table));
        mrp_msg_append(msg, MSG_STRING(COLUMNS, w->mql_columns));
        mrp_msg_append(msg, MSG_STRING(WHERE  , w->mql_where));
        mrp_msg_append(msg, MSG_UINT16(MAXROWS, w->max_rows));
    }

    return msg;
}


msg_t *msg_decode_register(mrp_msg_t *msg)
{
    register_msg_t     *reg;
    void               *it;
    mrp_domctl_table_t *t;
    mrp_domctl_watch_t *w;
    char               *name, *table, *columns, *index, *where;
    uint16_t            ntable, nwatch, max_rows;
    uint32_t            seqno;
    int                 i;

    it = NULL;

    if (!mrp_msg_iterate_get(msg, &it,
                             MSG_UINT32(MSGSEQ, &seqno),
                             MSG_STRING(NAME  , &name),
                             MSG_UINT16(NTABLE, &ntable),
                             MSG_UINT16(NWATCH, &nwatch),
                             MSG_END))
        return NULL;

    reg = mrp_allocz(sizeof(*reg));

    if (reg == NULL)
        return NULL;

    reg->type    = MSG_TYPE_REGISTER;
    reg->seq     = seqno;
    reg->name    = name;
    reg->tables  = mrp_allocz(sizeof(*reg->tables)  * ntable);
    reg->watches = mrp_allocz(sizeof(*reg->watches) * nwatch);

    if ((reg->tables == NULL && ntable) || (reg->watches == NULL && nwatch))
        goto fail;

    for (i = 0, t = reg->tables; i < ntable; i++, t++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MSG_STRING(TBLNAME, &table),
                                MSG_STRING(COLUMNS, &columns),
                                MSG_STRING(INDEX  , &index),
                                MSG_END)) {
            t->table       = table;
            t->mql_columns = columns;
            t->mql_index   = index;
        }
        else
            goto fail;
    }

    reg->ntable = ntable;

    for (i = 0, w = reg->watches; i < nwatch; i++, w++) {
        if (mrp_msg_iterate_get(msg, &it,
                                MSG_STRING(TBLNAME, &table),
                                MSG_STRING(COLUMNS, &columns),
                                MSG_STRING(WHERE  , &where),
                                MSG_UINT16(MAXROWS, &max_rows),
                                MSG_END)) {
            w->table       = table;
            w->mql_columns = columns;
            w->mql_where   = where;
            w->max_rows    = max_rows;
        }
        else
            goto fail;
    }

    reg->nwatch = nwatch;

    reg->wire       = mrp_msg_ref(msg);
    reg->unref_wire = msg_unref_wire;

    return (msg_t *)reg;

 fail:
    msg_free_register((msg_t *)reg);

    return NULL;
}


void msg_free_unregister(msg_t *msg)
{
    unregister_msg_t *ureg = (unregister_msg_t *)msg;

    if (ureg != NULL) {
        unref_wire(msg);
        mrp_free(ureg);
    }
}


mrp_msg_t *msg_encode_unregister(unregister_msg_t *ureg)
{
    return mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_UNREGISTER),
                          MSG_UINT32(MSGSEQ , ureg->seq),
                          MSG_END);
}


msg_t *msg_decode_unregister(mrp_msg_t *msg)
{
    unregister_msg_t *ureg;
    void             *it;
    uint32_t          seqno;

    ureg = mrp_allocz(sizeof(*ureg));

    if (ureg != NULL) {
        it = NULL;

        if (mrp_msg_iterate_get(msg, &it,
                                MSG_UINT32(MSGSEQ, &seqno),
                                MSG_END)) {
            ureg->type = MSG_TYPE_UNREGISTER;
            ureg->seq  = seqno;

            return (msg_t *)ureg;
        }

        msg_free_unregister((msg_t *)ureg);
    }

    return NULL;
}


void msg_free_ack(msg_t *msg)
{
    ack_msg_t *ack = (ack_msg_t *)msg;

    if (ack != NULL) {
        unref_wire(msg);
        mrp_free(ack);
    }
}


mrp_msg_t *msg_encode_ack(ack_msg_t *ack)
{
    return mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_ACK),
                          MSG_UINT32(MSGSEQ , ack->seq),
                          MSG_END);
}


msg_t *msg_decode_ack(mrp_msg_t *msg)
{
    ack_msg_t *ack;
    void      *it;
    uint32_t   seqno;

    ack = mrp_allocz(sizeof(*ack));

    if (ack != NULL) {
        it = NULL;

        if (mrp_msg_iterate_get(msg, &it,
                                MSG_UINT32(MSGSEQ, &seqno),
                                MSG_END)) {
            ack->type = MSG_TYPE_ACK;
            ack->seq  = seqno;

            return (msg_t *)ack;
        }

        msg_free_ack((msg_t *)ack);
    }

    return NULL;
}


void msg_free_nak(msg_t *msg)
{
    nak_msg_t *nak = (nak_msg_t *)msg;

    if (nak != NULL) {
        unref_wire(msg);
        mrp_free(nak);
    }
}


mrp_msg_t *msg_encode_nak(nak_msg_t *nak)
{
    return mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_NAK),
                          MSG_UINT32(MSGSEQ , nak->seq),
                          MSG_SINT32(ERRCODE, nak->error),
                          MSG_STRING(ERRMSG, nak->msg),
                          MSG_END);
}


msg_t *msg_decode_nak(mrp_msg_t *msg)
{
    nak_msg_t  *nak;
    void       *it;
    uint32_t    seqno;
    int32_t     error;
    const char *errmsg;

    nak = mrp_allocz(sizeof(*nak));

    if (nak != NULL) {
        it = NULL;

        if (mrp_msg_iterate_get(msg, &it,
                                MSG_UINT32(MSGSEQ , &seqno),
                                MSG_SINT32(ERRCODE, &error),
                                MSG_STRING(ERRMSG , &errmsg),
                                MSG_END)) {
            nak->type  = MSG_TYPE_NAK;
            nak->seq   = seqno;
            nak->error = error;
            nak->msg   = errmsg;

            nak->wire       = mrp_msg_ref(msg);
            nak->unref_wire = msg_unref_wire;

            return (msg_t *)nak;
        }

        msg_free_nak((msg_t *)nak);
    }

    return NULL;
}


void msg_free_set(msg_t *msg)
{
    set_msg_t *set = (set_msg_t *)msg;
    int        values_freed, i;

    if (set != NULL) {
        values_freed = FALSE;
        for (i = 0; i < set->ntable; i++) {
            if (set->tables[i].rows != NULL) {
                if (!values_freed && set->tables[i].rows[0] != NULL) {
                    mrp_free(set->tables[i].rows[0]);
                    values_freed = TRUE;
                }
                mrp_free(set->tables[i].rows);
            }
        }

        mrp_free(set->tables);
        mrp_free(set);
    }
}


mrp_msg_t *msg_encode_set(set_msg_t *set)
{
    mrp_msg_t          *msg;
    mrp_domctl_value_t *rows, *col;
    uint16_t            utable, utotal, tid, ncol, nrow;
    int                 i, r, c;

    utable = set->ntable;
    utotal = 0;

    msg = mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_SET),
                         MSG_UINT32(MSGSEQ , set->seq),
                         MSG_UINT16(NCHANGE, utable),
                         MSG_UINT16(NTOTAL , 0),
                         MSG_END);

    if (msg == NULL)
        return NULL;

    for (i = 0; i < set->ntable; i++) {
        tid  = set->tables[i].id;
        ncol = set->tables[i].ncolumn;
        nrow = set->tables[i].nrow;

        if (!mrp_msg_append(msg, MSG_UINT16(TBLID, tid))  ||
            !mrp_msg_append(msg, MSG_UINT16(NROW , nrow)) ||
            !mrp_msg_append(msg, MSG_UINT16(NCOL , ncol)))
            goto fail;

        for (r = 0; r < nrow; r++) {
            rows = set->tables[i].rows[r];

            for (c = 0; c < ncol; c++) {
                col = rows + c;

#define HANDLE_TYPE(pt, t, m)                                           \
                case MRP_DOMCTL_##pt:                                   \
                    if (!mrp_msg_append(msg, MSG_##t(DATA,col->m)))     \
                        goto fail;                                      \
                    break

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

    mrp_msg_set(msg, MSG_UINT16(NTOTAL, utotal));

    return msg;

 fail:
    mrp_msg_unref(msg);
    return NULL;
}


msg_t *msg_decode_set(mrp_msg_t *msg)
{
    set_msg_t          *set;
    void               *it;
    mrp_domctl_data_t  *d;
    mrp_domctl_value_t *values, *v;
    uint32_t            seqno;
    uint16_t            ntable, ntotal, nrow, ncol, tblid, type;
    int                 t, r, c;
    mrp_msg_value_t     value;

    it = NULL;

    if (!mrp_msg_iterate_get(msg, &it,
                             MSG_UINT32(MSGSEQ , &seqno),
                             MSG_UINT16(NCHANGE, &ntable),
                             MSG_UINT16(NTOTAL , &ntotal),
                             MSG_END))
        return NULL;

    set = mrp_allocz(sizeof(*set));

    if (set == NULL)
        return NULL;

    set->type   = MSG_TYPE_SET;
    set->seq    = seqno;
    set->tables = mrp_allocz(sizeof(*set->tables) * ntable);

    if (set->tables == NULL)
        goto fail;

    values = mrp_allocz(sizeof(*values) * ntotal);

    if (values == NULL)
        goto fail;

    d = set->tables;
    v = values;

    for (t = 0; t < ntable; t++) {
        if (!mrp_msg_iterate_get(msg, &it,
                                 MSG_UINT16(TBLID, &tblid),
                                 MSG_UINT16(NROW , &nrow ),
                                 MSG_UINT16(NCOL , &ncol ),
                                 MSG_END))
            goto fail;

        d->id      = tblid;
        d->ncolumn = ncol;
        d->nrow    = nrow;
        d->rows    = mrp_allocz(sizeof(*d->rows) * nrow);

        if (d->rows == NULL && nrow)
            goto fail;

        for (r = 0; r < nrow; r++) {
            d->rows[r] = v;
            values     = NULL;

            for (c = 0; c < ncol; c++) {
                if (!mrp_msg_iterate_get(msg, &it,
                                         MSG_ANY(DATA, &type, &value),
                                         MSG_END))
                    goto fail;

                switch (type) {
                case MRP_MSG_FIELD_STRING:
                    v->type = MRP_DOMCTL_STRING;
                    v->str  = value.str;
                    break;
                case MRP_MSG_FIELD_SINT32:
                    v->type = MRP_DOMCTL_INTEGER;
                    v->s32  = value.s32;
                    break;
                case MRP_MSG_FIELD_UINT32:
                    v->type = MRP_DOMCTL_UNSIGNED;
                    v->u32  = value.u32;
                    break;
                case MRP_MSG_FIELD_DOUBLE:
                    v->type = MRP_DOMCTL_DOUBLE;
                    v->dbl  = value.dbl;
                    break;
                default:
                    goto fail;
                }

                v++;
            }
        }

        d++;
    }

    set->ntable = ntable;

    set->wire       = mrp_msg_ref(msg);
    set->unref_wire = msg_unref_wire;

    return (msg_t *)set;

 fail:
    msg_free_set((msg_t *)set);
    mrp_free(values);

    return NULL;
}


void msg_free_notify(msg_t *msg)
{
    notify_msg_t *notify = (notify_msg_t *)msg;
    int           values_freed, i;

    if (notify != NULL) {
        values_freed = FALSE;
        for (i = 0; i < notify->ntable; i++) {
            if (notify->tables[i].rows != NULL) {
                if (!values_freed && notify->tables[i].rows[0] != NULL) {
                    mrp_free(notify->tables[i].rows[0]);
                    values_freed = TRUE;
                }
                mrp_free(notify->tables[i].rows);
            }
        }

        mrp_free(notify->tables);
        unref_wire((msg_t *)notify);
        mrp_free(notify);
    }
}


mrp_msg_t *msg_encode_notify(notify_msg_t *msg)
{
    MRP_UNUSED(msg);

    return NULL;
}


mrp_msg_t *msg_create_notify(void)
{
    return mrp_msg_create(MSG_UINT16(MSGTYPE, MSG_TYPE_NOTIFY),
                          MSG_UINT32(MSGSEQ , 0),
                          MSG_UINT16(NCHANGE, 0),
                          MSG_UINT16(NTOTAL , 0),
                          MSG_END);
}


int msg_update_notify(mrp_msg_t *msg, int tblid, mql_result_t *r)
{
    uint16_t    tid, nrow, ncol;
    int         types[MQI_COLUMN_MAX];
    const char *str;
    uint32_t    u32;
    int32_t     s32;
    double      dbl;
    int         i, j;

    if (r != NULL) {
        nrow = mql_result_rows_get_row_count(r);
        ncol = mql_result_rows_get_row_column_count(r);
    }
    else
        nrow = ncol = 0;

    tid = tblid;
    if (!mrp_msg_append(msg, MSG_UINT16(TBLID, tid))  ||
        !mrp_msg_append(msg, MSG_UINT16(NROW , nrow)) ||
        !mrp_msg_append(msg, MSG_UINT16(NCOL , ncol)))
        goto fail;

    for (i = 0; i < ncol; i++)
        types[i] = mql_result_rows_get_row_column_type(r, i);

    for (i = 0; i < nrow; i++) {
        for (j = 0; j < ncol; j++) {
            switch (types[j]) {
            case mqi_string:
                str = mql_result_rows_get_string(r, j, i, NULL, 0);
                if (!mrp_msg_append(msg, MSG_STRING(DATA, str)))
                    goto fail;
                break;
            case mqi_integer:
                s32 = mql_result_rows_get_integer(r, j, i);
                if (!mrp_msg_append(msg, MSG_SINT32(DATA, s32)))
                    goto fail;
                break;
            case mqi_unsignd:
                u32 = mql_result_rows_get_unsigned(r, j, i);
                if (!mrp_msg_append(msg, MSG_UINT32(DATA, u32)))
                    goto fail;
                break;

            case mqi_floating:
                dbl = mql_result_rows_get_floating(r, j, i);
                if (!mrp_msg_append(msg, MSG_DOUBLE(DATA, dbl)))
                    goto fail;
                break;

            default:
                goto fail;
            }
        }
    }

    return nrow * ncol;

 fail:
    return -1;
}


msg_t *msg_decode_notify(mrp_msg_t *msg)
{
    notify_msg_t       *notify;
    mrp_domctl_data_t  *d;
    mrp_domctl_value_t *values, *v;
    void               *it;
    uint32_t            seqno;
    uint16_t            ntable, ntotal, nrow, ncol;
    uint16_t            tblid;
    int                 t, r, c;
    uint16_t            type;
    mrp_msg_value_t     value;

    it = NULL;

    if (!mrp_msg_iterate_get(msg, &it,
                             MSG_UINT32(MSGSEQ, &seqno),
                             MSG_UINT16(NCHANGE, &ntable),
                             MSG_UINT16(NTOTAL , &ntotal),
                             MSG_END))
        return NULL;

    notify = mrp_allocz(sizeof(*notify));

    if (notify == NULL)
        return NULL;

    notify->type   = MSG_TYPE_NOTIFY;
    notify->seq    = seqno;
    notify->tables = mrp_allocz(sizeof(*notify->tables) * ntable);

    if (notify->tables == NULL && ntable != 0)
        goto fail;

    values = ntotal ? mrp_allocz(sizeof(*values) * ntotal) : NULL;

    if (values == NULL && ntotal != 0)
        goto fail;

    d = notify->tables;
    v = values;

    for (t = 0; t < ntable; t++) {
        if (!mrp_msg_iterate_get(msg, &it,
                                 MSG_UINT16(TBLID, &tblid),
                                 MSG_UINT16(NROW , &nrow ),
                                 MSG_UINT16(NCOL , &ncol ),
                                 MSG_END))
            goto fail;

        d->id      = tblid;
        d->ncolumn = ncol;
        d->nrow    = nrow;
        d->rows    = nrow ? mrp_allocz(sizeof(*d->rows) * nrow) : NULL;

        if (d->rows == NULL && nrow != 0)
            goto fail;

        for (r = 0; r < nrow; r++) {
            d->rows[r] = v;

            for (c = 0; c < ncol; c++) {
                if (!mrp_msg_iterate_get(msg, &it,
                                         MSG_ANY(DATA, &type, &value),
                                         MSG_END))
                    goto fail;

                switch (type) {
                case MRP_MSG_FIELD_STRING:
                    v->type = MRP_DOMCTL_STRING;
                    v->str  = value.str;
                    break;
                case MRP_MSG_FIELD_SINT32:
                    v->type = MRP_DOMCTL_INTEGER;
                    v->s32  = value.s32;
                    break;
                case MRP_MSG_FIELD_UINT32:
                    v->type = MRP_DOMCTL_UNSIGNED;
                    v->u32  = value.u32;
                    break;
                case MRP_MSG_FIELD_DOUBLE:
                    v->type = MRP_DOMCTL_DOUBLE;
                    v->dbl  = value.dbl;
                    break;
                default:
                    goto fail;
                }

                v++;
            }
        }

        d++;
    }

    notify->ntable = ntable;

    notify->wire       = mrp_msg_ref(msg);
    notify->unref_wire = msg_unref_wire;

    return (msg_t *)notify;

 fail:
    msg_free_notify((msg_t *)notify);
    mrp_free(values);

    return NULL;
}


msg_t *msg_decode_message(mrp_msg_t *msg)
{
    uint16_t type;

    if (mrp_msg_get(msg, MSG_UINT16(MSGTYPE, &type), MSG_END)) {
        switch (type) {
        case MSG_TYPE_REGISTER:   return msg_decode_register(msg);
        case MSG_TYPE_UNREGISTER: return msg_decode_unregister(msg);
        case MSG_TYPE_SET:        return msg_decode_set(msg);
        case MSG_TYPE_NOTIFY:     return msg_decode_notify(msg);
        case MSG_TYPE_ACK:        return msg_decode_ack(msg);
        case MSG_TYPE_NAK:        return msg_decode_nak(msg);
        default:                  break;
        }
    }

    return NULL;
}


mrp_msg_t *msg_encode_message(msg_t *msg)
{
    switch (msg->any.type) {
    case MSG_TYPE_REGISTER:   return msg_encode_register(&msg->reg);
    case MSG_TYPE_UNREGISTER: return msg_encode_unregister(&msg->unreg);
    case MSG_TYPE_SET:        return msg_encode_set(&msg->set);
    case MSG_TYPE_NOTIFY:     return msg_encode_notify(&msg->notify);
    case MSG_TYPE_ACK:        return msg_encode_ack(&msg->ack);
    case MSG_TYPE_NAK:        return msg_encode_nak(&msg->nak);
    default:                  return NULL;
    }
}


void msg_free_message(msg_t *msg)
{
    if (msg != NULL) {
        switch (msg->any.type) {
        case MSG_TYPE_REGISTER:   msg_free_register(msg);   break;
        case MSG_TYPE_UNREGISTER: msg_free_unregister(msg); break;
        case MSG_TYPE_SET:        msg_free_set(msg);        break;
        case MSG_TYPE_NOTIFY:     msg_free_notify(msg);     break;
        case MSG_TYPE_ACK:        msg_free_ack(msg);        break;
        case MSG_TYPE_NAK:        msg_free_nak(msg);        break;
        default:                                            break;
        }
    }
}


static void json_unref_wire(void *wire)
{
    mrp_json_unref((mrp_json_t *)wire);
}


mrp_json_t *json_encode_register(register_msg_t *reg)
{
    MRP_UNUSED(reg);

    return NULL;
}


msg_t *json_decode_register(mrp_json_t *msg)
{

    register_msg_t     *reg;
    mrp_domctl_table_t *t;
    mrp_domctl_watch_t *w;
    int                 seqno;
    char               *name, *table, *columns, *index, *where;
    int                 ntable, nwatch, max_rows;
    mrp_json_t         *arr, *tbl, *wch;
    int                 i;

    if (!mrp_json_get_integer(msg, "seq"   , &seqno)  ||
        !mrp_json_get_string (msg, "name"  , &name)   ||
        !mrp_json_get_integer(msg, "ntable", &ntable) ||
        !mrp_json_get_integer(msg, "nwatch", &nwatch))
        return NULL;

    reg = mrp_allocz(sizeof(*reg));

    if (reg == NULL)
        return NULL;

    reg->type    = MSG_TYPE_REGISTER;
    reg->seq     = seqno;
    reg->name    = name;
    reg->tables  = mrp_allocz(sizeof(*reg->tables)  * ntable);
    reg->watches = mrp_allocz(sizeof(*reg->watches) * nwatch);

    if ((reg->tables == NULL && ntable) || (reg->watches == NULL && nwatch))
        goto fail;

    if (!mrp_json_get_array(msg, "tables", &arr))
        goto fail;

    if (mrp_json_array_length(arr) != ntable)
        goto fail;

    for (i = 0, t = reg->tables; i < ntable; i++, t++) {
        if (!mrp_json_array_get_object(arr, i, &tbl))
            goto fail;

        if (mrp_json_get_string(tbl, "table"  , &table)   &&
            mrp_json_get_string(tbl, "columns", &columns) &&
            mrp_json_get_string(tbl, "index"  , &index)) {
            t->table       = table;
            t->mql_columns = columns;
            t->mql_index   = index;
        }
        else
            goto fail;
    }

    reg->ntable = ntable;

    if (!mrp_json_get_array(msg, "watches", &arr))
        goto fail;

    if (mrp_json_array_length(arr) != nwatch)
        goto fail;

    for (i = 0, w = reg->watches; i < nwatch; i++, w++) {
        if (!mrp_json_array_get_object(arr, i, &wch))
            goto fail;

        if (mrp_json_get_string (wch, "table"  , &table)   &&
            mrp_json_get_string (wch, "columns", &columns) &&
            mrp_json_get_string (wch, "where"  , &where)   &&
            mrp_json_get_integer(wch, "maxrows", &max_rows)) {
            w->table       = table;
            w->mql_columns = columns;
            w->mql_where   = where;
            w->max_rows    = max_rows;
        }
        else
            goto fail;
    }

    reg->nwatch = nwatch;

    reg->wire       = mrp_json_ref(msg);
    reg->unref_wire = json_unref_wire;

    return (msg_t *)reg;

 fail:
    msg_free_register((msg_t *)reg);

    return NULL;
}


msg_t *json_decode_unregister(mrp_json_t *msg)
{
    unregister_msg_t *ureg;
    int               seqno;

    ureg = mrp_allocz(sizeof(*ureg));

    if (ureg != NULL) {
        if (mrp_json_get_integer(msg, "seq", &seqno)) {
            ureg->type = MSG_TYPE_UNREGISTER;
            ureg->seq  = seqno;

            return (msg_t *)ureg;
        }

        msg_free_unregister((msg_t *)ureg);
    }

    return NULL;
}


mrp_json_t *json_encode_ack(ack_msg_t *ack)
{
    mrp_json_t *msg;
    int         seqno;

    msg = mrp_json_create(MRP_JSON_OBJECT);

    if (msg != NULL) {
        seqno = ack->seq;

        if (mrp_json_add_string (msg, "type", "ack") &&
            mrp_json_add_integer(msg, "seq" , seqno))
            return msg;
        else
            mrp_json_unref(msg);
    }

    return NULL;
}


msg_t *json_decode_ack(mrp_json_t *msg)
{
    ack_msg_t *ack;
    int        seqno;

    ack = mrp_allocz(sizeof(*ack));

    if (ack != NULL) {
        if (mrp_json_get_integer(msg, "seq", &seqno)) {
            ack->type = MSG_TYPE_ACK;
            ack->seq  = seqno;

            return (msg_t *)ack;
        }

        msg_free_ack((msg_t *)ack);
    }

    return NULL;
}


mrp_json_t *json_encode_nak(nak_msg_t *nak)
{
    mrp_json_t *msg;
    int         seqno, error;
    const char *errmsg;

    msg = mrp_json_create(MRP_JSON_OBJECT);

    if (msg != NULL) {
        seqno  = nak->seq;
        error  = nak->error;
        errmsg = nak->msg;

        if (mrp_json_add_string (msg, "type"  , "nak") &&
            mrp_json_add_integer(msg, "seq"   , seqno) &&
            mrp_json_add_integer(msg, "error" , error) &&
            mrp_json_add_string (msg, "errmsg", errmsg))
            return msg;
        else
            mrp_json_unref(msg);
    }

    return NULL;
}


msg_t *json_decode_nak(mrp_json_t *msg)
{
    nak_msg_t  *nak;
    int         seqno, error;
    const char *errmsg;

    nak = mrp_allocz(sizeof(*nak));

    if (nak != NULL) {
        if (mrp_json_get_integer(msg, "seqno" , &seqno) &&
            mrp_json_get_integer(msg, "error" , &error) &&
            mrp_json_get_string (msg, "errmsg", &errmsg)) {
            nak->type  = MSG_TYPE_NAK;
            nak->seq   = seqno;
            nak->error = error;
            nak->msg   = errmsg;

            nak->wire       = mrp_json_ref(msg);
            nak->unref_wire = json_unref_wire;

            return (msg_t *)nak;
        }

        msg_free_nak((msg_t *)nak);
    }

    return NULL;
}


msg_t *json_decode_set(mrp_json_t *msg)
{
    set_msg_t          *set;
    mrp_domctl_data_t  *d;
    mrp_domctl_value_t *values, *v;
    mrp_json_t         *tables, *tbl, *rows, *row, *col;
    int                 seqno, ntable, ntotal, nrow, ncol, tblid;
    int                 t, r, c;

    if (!mrp_json_get_integer(msg, "seq"    , &seqno)  ||
        !mrp_json_get_integer(msg, "nchange", &ntable) ||
        !mrp_json_get_integer(msg, "ntotal" , &ntotal))
        return NULL;

    set = mrp_allocz(sizeof(*set));

    if (set == NULL)
        return NULL;

    set->type   = MSG_TYPE_SET;
    set->seq    = seqno;
    set->tables = mrp_allocz(sizeof(*set->tables) * ntable);

    if (set->tables == NULL)
        goto fail;

    values = mrp_allocz(sizeof(*values) * ntotal);

    if (values == NULL)
        goto fail;

    d = set->tables;
    v = values;

    if (!mrp_json_get_array(msg, "tables", &tables))
        goto fail;

    for (t = 0; t < ntable; t++) {
        if (!mrp_json_array_get_object(tables, t, &tbl))
            goto fail;

        if (!mrp_json_get_integer(tbl, "id"  , &tblid) ||
            !mrp_json_get_integer(tbl, "nrow", &nrow)  ||
            !mrp_json_get_integer(tbl, "ncol", &ncol))
            goto fail;

        d->id      = tblid;
        d->ncolumn = ncol;
        d->nrow    = nrow;
        d->rows    = mrp_allocz(sizeof(*d->rows) * nrow);

        if (d->rows == NULL && nrow)
            goto fail;

        if (!mrp_json_get_array(tbl, "rows", &rows))
            goto fail;

        for (r = 0; r < nrow; r++) {
            if (!mrp_json_array_get_array(rows, t, &row))
                goto fail;

            d->rows[r] = v;
            values     = NULL;

            for (c = 0; c < ncol; c++) {
                col = mrp_json_array_get(row, c);

                if (col == NULL)
                    goto fail;

                switch (mrp_json_get_type(col)) {
                case MRP_JSON_STRING:
                    v->type = MRP_DOMCTL_STRING;
                    v->str  = mrp_json_string_value(col);
                    break;

                case MRP_JSON_INTEGER:
                    v->type = MRP_DOMCTL_INTEGER;
                    v->s32  = mrp_json_integer_value(col);
                    break;

                case MRP_JSON_BOOLEAN:
                    v->type = MRP_DOMCTL_INTEGER;
                    v->s32  = !!mrp_json_boolean_value(col);
                    break;

                case MRP_JSON_DOUBLE:
                    v->type = MRP_DOMCTL_DOUBLE;
                    v->dbl  = mrp_json_double_value(col);
                    break;

                default:
                    goto fail;
                }

                v++;
            }
        }

        d++;
    }

    set->ntable = ntable;

    set->wire       = mrp_json_ref(msg);
    set->unref_wire = json_unref_wire;

    return (msg_t *)set;

 fail:
    msg_free_set((msg_t *)set);
    mrp_free(values);

    return NULL;
}


mrp_json_t *json_create_notify(void)
{
    mrp_json_t *msg;

    msg = mrp_json_create(MRP_JSON_OBJECT);

    if (msg != NULL) {
        if (mrp_json_add_string (msg, "type"   , "notify") &&
            mrp_json_add_integer(msg, "seq"    , 0))
            return msg;
        else
            mrp_json_unref(msg);
    }

    return NULL;
}


int json_update_notify(mrp_json_t *msg, int tblid, mql_result_t *r)
{
    int         nrow, ncol;
    int         types[MQI_COLUMN_MAX];
    const char *str;
    uint32_t    u32;
    int32_t     s32;
    double      dbl;
    mrp_json_t *tables, *tbl, *rows, *row;
    int         i, j;

    if (r != NULL) {
        nrow = mql_result_rows_get_row_count(r);
        ncol = mql_result_rows_get_row_column_count(r);
    }
    else
        nrow = ncol = 0;

    if (!mrp_json_get_array(msg, "tables", &tables)) {
        tables = mrp_json_create(MRP_JSON_ARRAY);

        if (tables == NULL)
            goto fail;

        mrp_json_add(msg, "tables", tables);
    }

    tbl = mrp_json_create(MRP_JSON_OBJECT);

    if (tbl == NULL || !mrp_json_array_append(tables, tbl)) {
        mrp_json_unref(tbl);
        goto fail;
    }

    if (!mrp_json_add_integer(tbl, "id"  , tblid) ||
        !mrp_json_add_integer(tbl, "nrow", nrow)  ||
        !mrp_json_add_integer(tbl, "ncol", ncol))
        goto fail;

    rows = mrp_json_create(MRP_JSON_ARRAY);

    if (rows == NULL)
        goto fail;

    mrp_json_add(tbl, "rows", rows);

    for (i = 0; i < ncol; i++)
        types[i] = mql_result_rows_get_row_column_type(r, i);

    for (i = 0; i < nrow; i++) {
        row = mrp_json_create(MRP_JSON_ARRAY);

        if (row == NULL || !mrp_json_array_append(rows, row)) {
            mrp_json_unref(row);
            goto fail;
        }

        for (j = 0; j < ncol; j++) {
            switch (types[j]) {
            case mqi_string:
                str = mql_result_rows_get_string(r, j, i, NULL, 0);
                if (!mrp_json_array_append_string(row, str))
                    goto fail;
                break;
            case mqi_integer:
                s32 = mql_result_rows_get_integer(r, j, i);
                if (!mrp_json_array_append_integer(row, s32))
                    goto fail;
                break;
            case mqi_unsignd:
                u32 = mql_result_rows_get_unsigned(r, j, i);
                /* XXX TODO: check for overflow */
                if (!mrp_json_array_append_integer(row, u32))
                    goto fail;
                break;
            case mqi_floating:
                dbl = mql_result_rows_get_floating(r, j, i);
                if (!mrp_json_array_append_double(row, dbl))
                    goto fail;
                break;
            default:
                goto fail;
            }
        }
    }

    return nrow * ncol;

 fail:
    return -1;
}


msg_t *json_decode_message(mrp_json_t *msg)
{
    const char *type;

    if (mrp_json_get_string(msg, "type", &type)) {
        if (!strcmp(type, "register"  )) return json_decode_register(msg);
        if (!strcmp(type, "unregister")) return json_decode_unregister(msg);
        if (!strcmp(type, "set"       )) return json_decode_set(msg);
    }

    return NULL;
}


mrp_json_t *json_encode_message(msg_t *msg)
{
    switch (msg->any.type) {
    case MSG_TYPE_ACK: return json_encode_ack(&msg->ack);
    case MSG_TYPE_NAK: return json_encode_nak(&msg->nak);
    default:           return NULL;
    }
}
