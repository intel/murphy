#ifndef __MURPHY_SIMPLE_SCRIPT_CALL_H__
#define __MURPHY_SIMPLE_SCRIPT_CALL_H__

#include <murphy/core/scripting.h>

#include "simple-script.h"

function_call_t *create_call(char *name, arg_t *args, int narg);
int link_call(function_call_t *c);
void destroy_call(function_call_t *c);
void dump_call(FILE *fp, function_call_t *c);

int set_constant_value_arg(arg_t *arg, mrp_script_value_t *value);
int set_context_value_arg(arg_t *arg, char *name);
int set_context_set_arg(arg_t *arg, char *name,
                        mrp_script_value_t *value);
void destroy_arguments(arg_t *args, int narg);
int execute_call(function_call_t *c, mrp_context_tbl_t *tbl);

#endif /* __MURPHY_SIMPLE_SCRIPT_CALL_H__ */
