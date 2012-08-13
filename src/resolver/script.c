#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

#include <murphy/common/macros.h>

#include "resolver.h"


int eval_script(mrp_resolver_t *r, char *script, va_list ap)
{
    MRP_UNUSED(r);
    MRP_UNUSED(ap);

    if (script == NULL)
        return TRUE;
    else {
        printf("----- running update script -----\n");
        printf("%s", script);
        printf("---------------------------------\n");

        return TRUE;
    }
}
