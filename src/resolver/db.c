#include <stdint.h>

#include <murphy/common/macros.h>

#include "resolver-types.h"
#include "resolver.h"

uint32_t start_transaction(mrp_resolver_t *r)
{
    return r->stamp++;
}


int commit_transaction(mrp_resolver_t *r)
{
    MRP_UNUSED(r);

    return TRUE;
}


int rollback_transaction(mrp_resolver_t *r)
{
    r->stamp--;

    return TRUE;
}
