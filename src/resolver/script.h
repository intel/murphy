#ifndef __MURPHY_RESOLVER_SCRIPT_H__
#define __MURPHY_RESOLVER_SCRIPT_H__

#include <stdarg.h>

#include "resolver.h"

int eval_script(mrp_resolver_t *r, char *script, va_list ap);

#endif /* __MURPHY_RESOLVER_SCRIPT_H__ */
