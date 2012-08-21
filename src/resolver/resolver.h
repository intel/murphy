#ifndef __MURPHY_RESOLVER_H__
#define __MURPHY_RESOLVER_H__

#include <stdio.h>

MRP_CDECL_BEGIN

typedef struct mrp_resolver_s mrp_resolver_t;

#include <murphy/common/macros.h>
#include <murphy/resolver/script.h>

/** Parse the given resolver input file into a resolver context. */
mrp_resolver_t *mrp_resolver_parse(const char *path);

/** Destroy the given resolver context, freeing all associated resources. */
void mrp_resolver_cleanup(mrp_resolver_t *r);

/** Update the given target within the given resolver context. */
int mrp_resolver_update(mrp_resolver_t *r, const char *target, ...);

#define mrp_resolver_resolve mrp_resolver_update

/** Produce a debug dump of all targets. */
void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp);

/** Produce a debug dump of all tracked facts. */
void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp);

/** Register a script interpreter. */
int mrp_resolver_register_interpreter(mrp_interpreter_t *i);

/** Unregister a script interpreter. */
int mrp_resolver_unregister_interpreter(const char *name);

MRP_CDECL_END

#endif /* __MURPHY_RESOLVER_H__ */
