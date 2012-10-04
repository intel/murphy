#ifndef __MURPHY_DECISION_CLIENT_H__
#define __MURPHY_DECISION_CLIENT_H__

#include <murphy-db/mqi.h>
#include <murphy/common/mainloop.h>

#define MRP_DEFAULT_PEP_ADDRESS "unxs:@murphy-decision"


/*
 * helper macros for defining tables
 */

#define MRP_PEP_TABLE_COLUMNS(_var, _columns...)  \
    static mqi_column_def_t _var[] = {            \
        _columns,                                 \
        { NULL, mqi_unknown, 0, 0 },              \
    }

#define MRP_PEP_STRING(_name, _len, _is_idx)                            \
    { .name = _name, .type = mqi_varchar, .length = _len, .flags = _is_idx }

#define MRP_PEP_INTEGER(_name, _is_idx)                                 \
    { .name = _name, .type = mqi_integer, .length = 0, .flags = _is_idx }

#define MRP_PEP_UNSIGNED(_name, _is_idx)                                \
    { .name = _name, .type = mqi_unsignd, .length = 0, .flags = _is_idx }

#define MRP_PEP_FLOATING(_name, _is_idx)                                \
    { .name = _name, .type = mqi_floating, .length = 0, .flags = _is_idx }

#define MRP_PEP_TABLE(_name, _columns...) {      \
        .name    = _name,                        \
        .columns = _columns,                     \
        .ncolumn = MRP_ARRAY_SIZE(_columns),     \
        .id      = 0,                            \
    }

#define MRP_PEP_TABLES(var, tables...) \
    static mrp_pep_table_t var[] = {   \
        tables                         \
    }

/*
 * a table definition
 */

typedef struct {
    const char       *name;              /* table name */
    mqi_column_def_t *columns;           /* column definitions */
    int               ncolumn;           /* number of columns */
    int               idx_col;           /* column to use as index */
    int               id;                /* id used to reference this table */
} mrp_pep_table_t;


/*
 * table column values
 */

typedef union {
    const char *str;                     /* mqi_varchar */
    uint32_t    u32;                     /* mqi_unsignd */
    int32_t     s32;                     /* mqi_integer */
    double      dbl;                     /* mqi_floating */
} mrp_pep_value_t;


/*
 * table data
 */

typedef struct {
    int               id;                /* table id */
    mrp_pep_value_t  *columns;           /* table data */
    mqi_column_def_t *coldefs;           /* column definitions */
    int               ncolumn;           /* columns per row */
    int               nrow;              /* number of rows */
} mrp_pep_data_t;



/** Opaque policy enforcement point type. */
typedef struct mrp_pep_s mrp_pep_t;

/** Callback type for connection state notifications. */
typedef void (*mrp_pep_connect_cb_t)(mrp_pep_t *pep, int connection,
                                     int errcode, const char *errmsg,
                                     void *user_data);

/** Callback type for request status notifications. */
typedef void (*mrp_pep_status_cb_t)(mrp_pep_t *pep, int errcode,
                                    const char *errmsg, void *user_data);

/** Callback type for data change notifications. */
typedef void (*mrp_pep_data_cb_t)(mrp_pep_t *pep, mrp_pep_data_t *tables,
                                  int ntable, void *user_data);

/** Create a new policy enforcement point. */
mrp_pep_t *mrp_pep_create(const char *name, mrp_mainloop_t *ml,
                          mrp_pep_table_t *owned_tables, int nowned_table,
                          mrp_pep_table_t *watched_tables, int nwatched_table,
                          mrp_pep_connect_cb_t connect, mrp_pep_data_cb_t data,
                          void *user_data);

/** Destroy the given policy enforcement point. */
void mrp_pep_destroy(mrp_pep_t *pep);

/** Connect and register the given client to the server at the given address. */
int mrp_pep_connect(mrp_pep_t *pep, const char *address);

/** Close the connection to the server. */
void mrp_pep_disconnect(mrp_pep_t *pep);

/** Set the content of the given tables to the given data. */
int mrp_pep_set_data(mrp_pep_t *pep, mrp_pep_data_t *tables, int ntable,
                     mrp_pep_status_cb_t cb, void *user_data);

#endif /* __MURPHY_DECISION_CLIENT_H__ */
