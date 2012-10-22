#ifndef __MURPHY_DOMAIN_CONTROL_H__
#define __MURPHY_DOMAIN_CONTROL_H__

#include "domain-control-types.h"

pdp_t *create_domain_control(mrp_context_t *ctx, const char *address);
void destroy_domain_control(pdp_t *pdp);

void schedule_notification(pdp_t *pdp);

#endif /* __MURPHY_DOMAIN_CONTROL_H__ */
