#ifndef __MURPHY_DOMAIN_CONTROL_TABLE_H__
#define __MURPHY_DOMAIN_CONTROL_TABLE_H__

#include <murphy-db/mql-result.h>

#include "client.h"
#include "domain-control-types.h"

int init_tables(pdp_t *pdp);
void destroy_tables(pdp_t *pdp);

int create_proxy_table(pep_table_t *t, int *errcode, const char **errmsg);

int create_proxy_watch(pep_proxy_t *proxy, int id,
                       const char *table, const char *mql_columns,
                       const char *mql_where, int max_rows,
                       int *error, const char **errmsg);

void destroy_watch_table(pdp_t *pdp, pep_table_t *t);

void destroy_proxy_table(pep_table_t *t);
void destroy_proxy_tables(pep_proxy_t *proxy);

void destroy_proxy_watches(pep_proxy_t *proxy);

int set_proxy_tables(pep_proxy_t *proxy, mrp_domctl_data_t *tables, int ntable,
                     int *error, const char **errmsg);

int exec_mql(mql_result_type_t type, mql_result_t **resultp,
             const char *format, ...);


#endif /* __MURPHY_DOMAIN_CONTROL_TABLE_H__ */
