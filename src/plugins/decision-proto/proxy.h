#ifndef __MURPHY_DECISION_PROXY_H__
#define __MURPHY_DECISION_PROXY_H__

#include "decision-types.h"

int init_proxies(pdp_t *pdp);
void destroy_proxies(pdp_t *pdp);

pep_proxy_t *create_proxy(pdp_t *pdp);
void destroy_proxy(pep_proxy_t *proxy);

int register_proxy(pep_proxy_t *proxy, char *name,
                   mrp_pep_table_t *tables, int ntable,
                   mrp_pep_table_t *watches, int nwatch,
                   int *error, const char **errmsg);
int unregister_proxy(pep_proxy_t *proxy);

#endif /* __MURPHY_DECISION_PROXY_H__ */
