#ifndef __MURPHY_DECISION_H__
#define __MURPHY_DECISION_H__

#include "decision-types.h"

pdp_t *create_decision(mrp_context_t *ctx, const char *address);
void destroy_decision(pdp_t *pdp);

void schedule_notification(pdp_t *pdp);

#endif /* __MURPHY_DECISION_H__ */
