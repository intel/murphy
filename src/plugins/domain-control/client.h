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

#ifndef __MURPHY_DOMAIN_CONTROL_CLIENT_H__
#define __MURPHY_DOMAIN_CONTROL_CLIENT_H__

#include <murphy-db/mqi.h>
#include <murphy/common/mainloop.h>

#define MRP_DEFAULT_DOMCTL_ADDRESS "unxs:@murphy-domctrl"


/*
 * a table owned by a domain controller
 */

typedef struct {
    const char *table;                   /* table name */
    const char *mql_columns;             /* column definition scriptlet */
    const char *mql_index;               /* index column list */
} mrp_domctl_table_t;

#define MRP_DOMCTL_TABLE(_table, _columns, _index) \
    { .table = _table, .mql_columns = _columns, .mql_index = _index }


/*
 * a table tracked by a domain controller
 */

typedef struct {
    const char *table;                   /* table name */
    const char *mql_columns;             /* column list for select */
    const char *mql_where;               /* where clause for select */
    int         max_rows;                /* max number of rows to select */
} mrp_domctl_watch_t;

#define MRP_DOMCTL_WATCH(_table, _columns, _where, _max_rows) {       \
        .table       = _table                  ,                      \
        .mql_columns = _columns ? _columns : "",                      \
        .mql_where   = _where   ? _where   : "",                      \
        .max_rows    = _max_rows               ,                      \
    }


/*
 * table column types and values
 */

typedef enum {
    MRP_DOMCTL_STRING   = mqi_varchar,
    MRP_DOMCTL_INTEGER  = mqi_integer,
    MRP_DOMCTL_UNSIGNED = mqi_unsignd,
    MRP_DOMCTL_DOUBLE   = mqi_floating
} mrp_domctl_type_t;

typedef struct {
    mrp_domctl_type_t  type;             /* data type */
    union {
        const char *str;                 /* MRP_DOMCTL_STRING */
        uint32_t    u32;                 /* MRP_DOMCTL_UNSIGNED */
        int32_t     s32;                 /* MRP_DOMCTL_INTEGER */
        double      dbl;                 /* MRP_DOMCTL_DOUBLE */
    };
} mrp_domctl_value_t;


/*
 * table data
 */

typedef struct {
    int                  id;             /* table id */
    mqi_column_def_t    *coldefs;        /* column definitions */
    int                  ncolumn;        /* columns per row */
    mrp_domctl_value_t **rows;           /* row data */
    int                  nrow;           /* number of rows */
} mrp_domctl_data_t;



/** Opaque policy domain controller type. */
typedef struct mrp_domctl_s mrp_domctl_t;

/** Callback type for connection state notifications. */
typedef void (*mrp_domctl_connect_cb_t)(mrp_domctl_t *dc, int connection,
                                        int errcode, const char *errmsg,
                                        void *user_data);

/** Callback type for request status notifications. */
typedef void (*mrp_domctl_status_cb_t)(mrp_domctl_t *dc, int errcode,
                                       const char *errmsg, void *user_data);

/** Callback type for data change notifications. */
typedef void (*mrp_domctl_watch_cb_t)(mrp_domctl_t *dc,
                                      mrp_domctl_data_t *tables, int ntable,
                                      void *user_data);

/** Create a new policy domain controller. */
mrp_domctl_t *mrp_domctl_create(const char *name, mrp_mainloop_t *ml,
                                mrp_domctl_table_t *tables, int ntable,
                                mrp_domctl_watch_t *watches, int nwatch,
                                mrp_domctl_connect_cb_t connect_cb,
                                mrp_domctl_watch_cb_t watch_cb,
                                void *user_data);

/** Destroy the given policy domain controller. */
void mrp_domctl_destroy(mrp_domctl_t *dc);

/**
 * Connect and register the given controller to the server. If timeout
 * is non-negative, it will be used to automatically attempt re-connecting
 * to the server this often (in seconds) whenever the connection goes down.
 */
int mrp_domctl_connect(mrp_domctl_t *dc, const char *address, int timeout);

/** Close the connection to the server. */
void mrp_domctl_disconnect(mrp_domctl_t *dc);

/** Set the content of the given tables to the provided data. */
int mrp_domctl_set_data(mrp_domctl_t *dc, mrp_domctl_data_t *tables, int ntable,
                        mrp_domctl_status_cb_t status_cb, void *user_data);

#endif /* __MURPHY_DOMAIN_CONTROL_CLIENT_H__ */
