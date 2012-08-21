#ifndef __MURPHY_RESOLVER_DB_H__
#define __MURPHY_RESOLVER_DB_H__

#define INVALID_TX ((uint32_t)-1)

uint32_t start_transaction(mrp_resolver_t *r);
int commit_transaction(mrp_resolver_t *r);
int rollback_transaction(mrp_resolver_t *r);

#endif /* __MURPHY_RESOLVER_DB_H__ */
