#ifndef __MQL_STATEMENT_H__
#define __MQL_STATEMENT_H__

#include <murphy-db/mql-result.h>


typedef enum {
    mql_statement_unknown = 0,
    mql_statement_show_tables,
    mql_statement_describe,
    mql_statement_create_table,
    mql_statement_create_index,
    mql_statement_drop_table,
    mql_statement_drop_index,
    mql_statement_begin,
    mql_statement_commit,
    mql_statement_rollback,
    mql_statement_insert,
    mql_statement_update,
    mql_statement_delete,
    mql_statement_select,
    /* do not add anything after this */
    mql_statement_last
} mql_statement_type_t;

typedef struct mql_statement_s {
    mql_statement_type_t  type;
    uint8_t               data[0];
} mql_statement_t;


mql_result_t *mql_exec_statement(mql_result_type_t, mql_statement_t *);
int mql_bind_value(mql_statement_t *, int, mqi_data_type_t, ...);
void mql_statement_free(mql_statement_t *);


#endif /* __MQL_STATEMENT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
