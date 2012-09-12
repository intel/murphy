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

#ifndef __MURPHY_SIMPLE_SCRIPT_TOKEN_H__
#define __MURPHY_SIMPLE_SCRIPT_TOKEN_H__

#include <stdint.h>
#include <stdlib.h>

/*
 * common token fields
 */

#define SIMPLE_TOKEN_FIELDS                                               \
    const char *token;                   /* token string */               \
    int         line;                    /* and on this line */           \
    size_t      size                     /* token size */

/*
 * a generic token
 */

typedef struct {
    SIMPLE_TOKEN_FIELDS;
} tkn_any_t;


/*
 * a string token
 */

typedef struct {
    SIMPLE_TOKEN_FIELDS;
    char *value;
} tkn_string_t;


#define DEFINE_INTEGER_TOKEN(_ttype, _ctype)    \
    typedef struct {                            \
        SIMPLE_TOKEN_FIELDS;                    \
        _ctype value;                           \
    } tkn_##_ttype##_t

#define DEFINE_ARRAY_TOKEN(_ttype, _cype)       \
    typedef struct {                            \
        SIMPLE_TOKEN_FIELDS;                    \
        _ctype *values;                         \
        int     nvalue;                         \
    } tkn_##_ttype##_arr


DEFINE_INTEGER_TOKEN(u8 , uint8_t );
DEFINE_INTEGER_TOKEN(s8 ,  int8_t );
DEFINE_INTEGER_TOKEN(u16, uint16_t);
DEFINE_INTEGER_TOKEN(s16,  int16_t);
DEFINE_INTEGER_TOKEN(u32, uint32_t);
DEFINE_INTEGER_TOKEN(s32,  int32_t);
DEFINE_INTEGER_TOKEN(u64, uint64_t);
DEFINE_INTEGER_TOKEN(s64,  int64_t);

typedef struct {
    SIMPLE_TOKEN_FIELDS;
    double value;
} tkn_dbl_t;

/*
DEFINE_ARRAY_TOKEN(u8 , uint8_t );
DEFINE_ARRAY_TOKEN(s8 ,  int8_t );
DEFINE_ARRAY_TOKEN(u16, uint16_t);
DEFINE_ARRAY_TOKEN(u16, uint16_t);
DEFINE_ARRAY_TOKEN(s16,  int16_t);
DEFINE_ARRAY_TOKEN(u32, uint32_t);
DEFINE_ARRAY_TOKEN(s32,  int32_t);
DEFINE_ARRAY_TOKEN(u64, uint64_t);
DEFINE_ARRAY_TOKEN(s64,  int64_t);
DEFINE_ARRAY_TOKEN(str, char *  );
*/


/*
 * string array tokens
 */

typedef struct {
    SIMPLE_TOKEN_FIELDS;
    int    nstr;
    char **strs;
} tkn_strarr_t;


/*
 * function call arguments
 */

typedef struct {
    SIMPLE_TOKEN_FIELDS;
    arg_t *args;
    int    narg;
} tkn_args_t;


/*
 * a constant value
 */

typedef struct {
    SIMPLE_TOKEN_FIELDS;
    mrp_script_value_t value;
} tkn_value_t;

/*
 * an expression
 */

typedef enum {
    EXPR_UNKNOWN = 0,
    EXPR_CONSTANT,                       /* a constant value */
    EXPR_CONTEXT_VALUE,                  /* a context variable value */
    EXPR_CONTEXT_SET                     /* a context variable assignment */
} expr_type_t;


typedef struct {
    expr_type_t        type;             /* EXPR_CONSTANT */
    mrp_script_value_t value;            /* constant value with a type */
} const_expr_t;

typedef struct {
    expr_type_t  type;                   /* EXPR_CONTEXT_VALUE */
    char        *name;                   /* context variable name */
} ctx_val_expr_t;

typedef struct {
    expr_type_t         type;            /* EXPR_CONTEXT_SET */
    char               *name;            /* context variable name */
    mrp_script_value_t  value;           /* value to set */
} ctx_set_expr_t;

typedef union {
    expr_type_t    type;                 /* expression type, EXPR_* */
    const_expr_t   cst;                  /* constant expression */
    ctx_val_expr_t val;                  /* context variable value */
    ctx_set_expr_t set;                  /* context variable assignment */
} tkn_expr_t;

#ifdef __MURPHY_SIMPLE_SCRIPT_CHECK_RINGBUF__
#    define SIMPLE_TOKEN_DONE(t)         memset((t).token, 0, (t).size)
#else
#    define SIMPLE_TOKEN_DONE(t)         do {} while (0)
#endif

#define SIMPLE_TOKEN_SAVE(str, size) save_token((str), (size))


#endif /* __MURPHY_SIMPLE_SCRIPT_TOKEN_H__ */
