#ifndef __MURPHY_RESOLVER_FACT_H__
#define __MURPHY_RESOLVER_FACT_H__

#include "resolver.h"

int create_facts(mrp_resolver_t *r);
int create_fact(mrp_resolver_t *r, char *name);
void destroy_facts(mrp_resolver_t *r);

int fact_changed(mrp_resolver_t *r, int id);
uint32_t fact_stamp(mrp_resolver_t *r, int id);

#endif /* __MURPHY_RESOLVER_FACT_H__ */
