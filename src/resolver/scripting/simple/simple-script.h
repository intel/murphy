/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MURPHY_SIMPLE_SCRIPT_H__
#define __MURPHY_SIMPLE_SCRIPT_H__

#include <murphy/core/plugin.h>
#include <murphy/core/scripting.h>

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
    arg_type_t          type;            /* ARG_CONTEXT_SET */
    char               *name;            /* name of variable */
    int                 id;              /* variable id */
    mrp_script_value_t  value;           /* value to set to */
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

    int          (*script_ptr)(mrp_plugin_t *plugin, const char *name,
                               mrp_script_env_t *env);
    mrp_plugin_t  *plugin;
} function_call_t;


#endif /* __MURPHY_SIMPLE_SCRIPT_H__ */
