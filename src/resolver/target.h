#ifndef __MURPHY_RESOLVER_TARGET_H__
#define __MURPHY_RESOLVER_TARGET_H__

#include "resolver-types.h"
#include "resolver.h"
#include "parser-api.h"

int create_targets(mrp_resolver_t *r, yy_res_parser_t *parser);
void destroy_targets(mrp_resolver_t *r);
int compile_target_scripts(mrp_resolver_t *r);

int update_target_by_name(mrp_resolver_t *r, const char *name);
int update_target_by_id(mrp_resolver_t *r, int id);

void dump_targets(mrp_resolver_t *r, FILE *fp);

#endif /* __MURPHY_RESOLVER_TARGET_H__ */
