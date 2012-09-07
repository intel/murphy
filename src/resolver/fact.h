#ifndef __MURPHY_RESOLVER_FACT_H__
#define __MURPHY_RESOLVER_FACT_H__

#include <murphy-db/mqi.h>
#include "resolver.h"

int create_fact(mrp_resolver_t *r, char *name);
void destroy_facts(mrp_resolver_t *r);

int fact_changed(mrp_resolver_t *r, int id);
uint32_t fact_stamp(mrp_resolver_t *r, int id);

fact_t *lookup_fact(mrp_resolver_t *r, const char *name);


mqi_handle_t start_transaction(mrp_resolver_t *r);
int commit_transaction(mrp_resolver_t *r, mqi_handle_t tx);
int rollback_transaction(mrp_resolver_t *r, mqi_handle_t tx);

#endif /* __MURPHY_RESOLVER_FACT_H__ */
