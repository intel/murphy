#ifndef __MURPHY_RESOLVER_H__
#define __MURPHY_RESOLVER_H__

#include <stdio.h>

typedef struct mrp_resolver_s mrp_resolver_t;

mrp_resolver_t *mrp_resolver_parse(const char *path);
void mrp_resolver_cleanup(mrp_resolver_t *r);
int mrp_resolver_update(mrp_resolver_t *r, const char *target, ...);
void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp);
void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp);

#endif /* __MURPHY_RESOLVER_H__ */
