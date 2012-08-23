#ifndef __MRP_RESOLVER_SCRIPT_H__
#define __MRP_RESOLVER_SCRIPT_H__

#include <stdint.h>
#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/common/list.h>

MRP_CDECL_BEGIN

#include "resolver.h"


/** Set the default interpreter type. */
void set_default_interpreter(const char *type);

/** Register the given script interpreter. */
int register_interpreter(mrp_interpreter_t *i);

/** Unregister the given interpreter. */
void unregister_interpreter(mrp_interpreter_t *i);

/** Lookup an interpreter by name. */
mrp_interpreter_t *lookup_interpreter(const char *name);

/** Create (prepare) a script of the given type with the given source. */
mrp_script_t *create_script(char *type, const char *source);

/** Destroy the given script freeing all associated resources. */
void destroy_script(mrp_script_t *script);

/** Compile the given script, preparing it for execution. */
int compile_script(mrp_script_t *script);

/** Execute the given script. */
int execute_script(mrp_resolver_t *r, mrp_script_t *s);

/** Dummy routine that just prints the script to be evaluated. */
int eval_script(mrp_resolver_t *r, char *script);

MRP_CDECL_END

#endif /* __MRP_RESOLVER_SCRIPT_H__ */
