#ifndef __MURPHY_RESOLVER_H__
#define __MURPHY_RESOLVER_H__

#include <stdio.h>
#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/core/scripting.h>

MRP_CDECL_BEGIN

typedef struct mrp_resolver_s mrp_resolver_t;

/** Parse the given resolver input file into a resolver context. */
mrp_resolver_t *mrp_resolver_parse(const char *path);

/** Destroy the given resolver context, freeing all associated resources. */
void mrp_resolver_cleanup(mrp_resolver_t *r);

/** Update the given target. The NULL-terminated variable argument list
    after the target name sepcifies the resolver context variables to
    set during the update. Use a single NULL to omit variables. */
int mrp_resolver_update_targetl(mrp_resolver_t *r,
                                const char *target, ...) MRP_NULLTERM;

/** Update the given target. The variable name and type/value arrays
    specify the resolver context variables to set during the update. */
int mrp_resolver_update_targetv(mrp_resolver_t *r, const char *target,
                                const char **variables,
                                mrp_script_value_t *values,
                                int nvariable);

/** Declare a context variable with a given type. */
int mrp_resolver_declare_variable(mrp_resolver_t *r, const char *name,
                                  mrp_script_type_t type);


/** Get the value of a context variable by id. */
int mrp_resolver_get_value(mrp_resolver_t *r, int id, mrp_script_value_t *v);
#define mrp_resolver_get_value_by_id mrp_resolver_get_value

/** Get the value of a context variable by name. */
int mrp_resolver_get_value_by_name(mrp_resolver_t *r, const char *name,
                                   mrp_script_value_t *v);

/** Print the given value to the given buffer. */
char *mrp_print_value(char *buf, size_t size, mrp_script_value_t *value);

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
