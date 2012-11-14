%{ /* -*- c -*- */

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>

#include "simple-parser-api.h"
#include "token.h"
#include "simple-scanner.h"
#include "call.h"

void yy_smpl_error(yy_smpl_parser_t *parser, const char *msg);

static tkn_strarr_t *strarr_append(tkn_strarr_t *arr, char *str);
static int initialize_argument(tkn_args_t *args, tkn_expr_t *expr);
static int append_argument(tkn_args_t *args, tkn_expr_t *expr);
static void cleanup_arguments(tkn_args_t *args);

%}

%union {
    tkn_any_t       any;
    tkn_string_t    string;
    tkn_s8_t        sint8;
    tkn_u8_t        uint8;
    tkn_s16_t       sint16;
    tkn_u16_t       uint16;
    tkn_s32_t       sint32;
    tkn_u32_t       uint32;
    tkn_s64_t       sint64;
    tkn_u64_t       uint64;
    tkn_dbl_t       dbl;
    tkn_strarr_t   *strarr;
    tkn_string_t    error;
    tkn_args_t      args;
    tkn_expr_t      expr;
    tkn_value_t     value;
}

%defines
%parse-param { yy_smpl_parser_t *parser }
%lex-param   { yy_smpl_parser_t *parser }

%token          KEY_TARGET
%token          KEY_DEPENDS_ON
%token          KEY_UPDATE_SCRIPT
%token          KEY_END_SCRIPT

%token <string> TKN_IDENT
%token <string> TKN_CONTEXT_VAR
%token <string> TKN_STRING

%token <sint8>  TKN_SINT8
%token <uint8>  TKN_UINT8
%token <sint16> TKN_SINT16
%token <uint16> TKN_UINT16
%token <sint32> TKN_SINT32
%token <uint32> TKN_UINT32
%token <sint64> TKN_SINT64
%token <uint64> TKN_UINT64
%token <dbl>    TKN_DOUBLE

%token <string> TKN_PARENTH_OPEN  "("
%token <string> TKN_PARENTH_CLOSE ")"
%token <string> TKN_COMMA         ","
%token <string> TKN_EQUAL         "="

%token <string> TKN_ERROR

%type <value>  constant_expression
%type <string> context_value
%type <expr>   context_assign
%type <expr>   expression
%type <args>   arguments

%%

input: statements
  ;

statements:
  statement
| statements statement
| statements error     { YYABORT; }
  ;

statement: function_call
  ;

function_call:
  TKN_IDENT "(" ")" {
      function_call_t *c;

      c = create_call($1.value, NULL, 0);

      if (c != NULL)
          mrp_list_append(&parser->statements, &c->hook);
      else {
          mrp_log_error("Failed to create new simple-script call.");
          YYABORT;
      }
  }
| TKN_IDENT "(" arguments ")" {
      function_call_t *c;

      c = create_call($1.value, $3.args, $3.narg);

      if (c != NULL)
          mrp_list_append(&parser->statements, &c->hook);
      else {
          mrp_log_error("Failed to create new simple-script call.");
          YYABORT;
      }
  }
  ;

arguments:
  expression {
      if (!initialize_argument(&$$, &$1))
          YYABORT;
  }
| arguments "," expression {
      if (!append_argument(&$$, &$3)) {
          cleanup_arguments(&$$);
          YYABORT;
      }
  }
| arguments error { YYABORT; }
  ;

expression:
  constant_expression {
      $$.cst.type  = EXPR_CONSTANT;
      $$.cst.value = $1.value;
  }
| context_value {
      $$.val.type  = EXPR_CONTEXT_VALUE;
      $$.val.name  = $1.value;
  }
| context_assign {
      $$ = $1;
  }
  ;

constant_expression:
  TKN_STRING {
      $$.value.type = MRP_SCRIPT_TYPE_STRING;
      $$.value.str  = $1.value;
  }
| TKN_SINT8 {
      $$.value.type = MRP_SCRIPT_TYPE_SINT8;
      $$.value.s8   = $1.value;
  }
| TKN_UINT8 {
      $$.value.type = MRP_SCRIPT_TYPE_UINT8;
      $$.value.u8   = $1.value;
  }
| TKN_SINT16 {
      $$.value.type = MRP_SCRIPT_TYPE_SINT16;
      $$.value.s16  = $1.value;
  }
| TKN_UINT16 {
      $$.value.type = MRP_SCRIPT_TYPE_UINT16;
      $$.value.u16  = $1.value;
  }
| TKN_SINT32 {
      $$.value.type = MRP_SCRIPT_TYPE_SINT32;
      $$.value.s32  = $1.value;
  }
| TKN_UINT32 {
      $$.value.type = MRP_SCRIPT_TYPE_UINT32;
      $$.value.u32  = $1.value;
  }
| TKN_SINT64 {
      $$.value.type = MRP_SCRIPT_TYPE_SINT64;
      $$.value.s64  = $1.value;
  }
| TKN_UINT64 {
      $$.value.type = MRP_SCRIPT_TYPE_UINT64;
      $$.value.u64  = $1.value;
  }
| TKN_DOUBLE {
      $$.value.type = MRP_SCRIPT_TYPE_DOUBLE;
      $$.value.dbl = $1.value;
  }
  ;

context_value: TKN_CONTEXT_VAR { $$ = $1; }
  ;

context_assign: TKN_CONTEXT_VAR "=" constant_expression {
      mrp_debug("context_assign");
      $$.set.type  = EXPR_CONTEXT_SET;
      $$.set.name  = $1.value;
      $$.set.value = $3.value;
  }
  ;


%%

static int initialize_argument(tkn_args_t *args, tkn_expr_t *expr)
{
    arg_t *arg;

    args->args = mrp_allocz(sizeof(*args->args));

    if (args->args != NULL) {
        arg        = args->args;
        args->narg = 1;

        switch (expr->type) {
        case EXPR_CONSTANT:
            set_constant_value_arg(arg, &expr->cst.value);
            break;
        case EXPR_CONTEXT_VALUE:
            set_context_value_arg(&args->args[0], expr->val.name);
            break;

        case EXPR_CONTEXT_SET:
            set_context_set_arg(&args->args[0],
                                expr->set.name, &expr->set.value);
            break;

        default:
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}


static int append_argument(tkn_args_t *args, tkn_expr_t *expr)
{
    arg_t *arg;

    if (!mrp_reallocz(args->args, args->narg, args->narg + 1))
        return FALSE;

    arg = args->args + args->narg;
    args->narg++;

    switch (expr->type) {
    case EXPR_CONSTANT:
        set_constant_value_arg(arg, &expr->cst.value);
        break;
    case EXPR_CONTEXT_VALUE:
        set_context_value_arg(&args->args[0], expr->val.name);
        break;
    case EXPR_CONTEXT_SET:
        set_context_set_arg(&args->args[0], expr->set.name, &expr->set.value);

    default:
        return FALSE;
    }

    return TRUE;
}


static void cleanup_arguments(tkn_args_t *args)
{
    destroy_arguments(args->args, args->narg);
}


void yy_smpl_error(yy_smpl_parser_t *parser, const char *msg)
{
    MRP_UNUSED(parser);

    mrp_log_error("parse error at line %d near token '%s': %s",
                  yy_smpl_lval.any.line, yy_smpl_lval.any.token, msg);
}


static tkn_strarr_t *strarr_append(tkn_strarr_t *arr, char *str)
{
    int n;

    if (arr == NULL) {
        arr = mrp_allocz(sizeof(*arr));
        if (arr == NULL)
            return NULL;
    }

    if (!mrp_reallocz(arr->strs, arr->nstr, arr->nstr + 1))
        return NULL;

    n = arr->nstr++;
    arr->strs[n] = mrp_strdup(str);

    if (arr->strs[n] != NULL)
        return arr;
    else {
        if (n == 0) {
            mrp_free(arr->strs);
            mrp_free(arr);
        }

        return NULL;
    }
}


int simple_parser_setup(yy_smpl_parser_t *parser, const char *script)
{
    MRP_UNUSED(strarr_append);

    mrp_clear(parser);

    mrp_list_init(&parser->statements);
    parser->line = 1;

    return simple_scanner_setup(parser, script);
}


void simple_parser_cleanup(yy_smpl_parser_t *parser)
{
    mrp_list_hook_t *p, *n;
    function_call_t *c;

    mrp_list_foreach(&parser->statements, p, n) {
        c = mrp_list_entry(p, typeof(*c), hook);

        destroy_call(c);
    }

    simple_scanner_cleanup(parser);
}


int simple_parser_parse(yy_smpl_parser_t *parser, const char *script)
{
    if (simple_parser_setup(parser, script))
        if (yy_smpl_parse(parser) == 0)
            return TRUE;

    return FALSE;
}
