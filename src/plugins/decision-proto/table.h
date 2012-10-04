#ifndef __MURPHY_DECISION_TABLE_H__
#define __MURPHY_DECISION_TABLE_H__

#include "client.h"
#include "decision-types.h"

int copy_pep_table(mrp_pep_table_t *src, mrp_pep_table_t *dst);
void free_pep_table(mrp_pep_table_t *t);

int copy_pep_tables(mrp_pep_table_t *src, mrp_pep_table_t *dst, int n);
void free_pep_tables(mrp_pep_table_t *tables, int n);

int init_tables(pdp_t *pdp);
void destroy_tables(pdp_t *pdp);

int create_proxy_table(pep_table_t *t, mrp_pep_table_t *def,
                       int *errcode, const char **errmsg);

int create_proxy_watch(pep_proxy_t *proxy, int id, mrp_pep_table_t *def,
                       int *errcode, const char **errmsg);

void destroy_watch_table(pdp_t *pdp, pep_table_t *t);

void destroy_proxy_table(pep_table_t *t);
void destroy_proxy_tables(pep_proxy_t *proxy);

void destroy_proxy_watches(pep_proxy_t *proxy);

int set_proxy_tables(pep_proxy_t *proxy, mrp_pep_data_t *tables, int ntable,
                     int *error, const char **errmsg);

#endif /* __MURPHY_DECISION_TABLE_H__ */
