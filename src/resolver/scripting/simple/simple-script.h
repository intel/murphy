#ifndef __MURPHY_SIMPLE_SCRIPT_H__
#define __MURPHY_SIMPLE_SCRIPT_H__

#include <murphy/resolver/script.h>

typedef struct {
    mrp_list_hook_t statements;          /* list of (call) statements */
} simple_script_t;


typedef enum {
    ARG_UNKNOWN = 0,
    ARG_CONST_VALUE,
    ARG_CONTEXT_VAR,
    ARG_CONTEXT_SET,
} arg_type_t;


typedef struct {
    arg_type_t         type;             /* ARG_CONST_VALUE */
    mrp_script_value_t value;            /* actual value */
} const_arg_t;


typedef struct {
    arg_type_t  type;                    /* ARG_CONTEXT_VAR */
    char       *name;                    /* name of variable */
    int         id;                      /* variable id */
} ctx_val_arg_t;


typedef struct {
    arg_type_t                type;      /* ARG_CONTEXT_SET */
    char                     *name;      /* name of variable */
    int                       id;        /* variable id */
    mrp_script_typed_value_t  value;     /* value to set to */
} ctx_set_arg_t;


typedef union {
    arg_type_t     type;                 /* argument type */
    const_arg_t    cst;                  /* constant argument */
    ctx_val_arg_t  val;                  /* context variable value */
    ctx_set_arg_t  set;                  /* context variable assignment */
} arg_t;


typedef struct {
    char                     *name;      /* name of the function to call */
    arg_t                    *args;      /* arguments to pass */
    int                       narg;      /* number of arguments */
    mrp_list_hook_t           hook;      /* to list of statements */
} function_call_t;


#endif /* __MURPHY_SIMPLE_SCRIPT_H__ */
