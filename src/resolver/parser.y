%{ /* -*- c -*- */

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>

#include "murphy/resolver/resolver.h"
#include "murphy/resolver/parser-api.h"
#include "murphy/resolver/token.h"
#include "murphy/resolver/scanner.h"

void yy_res_error(yy_res_parser_t *parser, const char *msg);

static tkn_strarr_t *strarr_append(tkn_strarr_t *arr, char *str);

%}

%union {
    tkn_any_t        any;
    tkn_string_t     string;
    tkn_s16_t        s16;
    tkn_u16_t        u16;
    tkn_s32_t        s32;
    tkn_u32_t        u32;
    yy_res_target_t *target;
    yy_res_script_t  script;
    tkn_strarr_t    *strarr;
}

%defines
%parse-param { yy_res_parser_t *parser }
%lex-param   { yy_res_parser_t *parser }

%token          KEY_TARGET
%token          KEY_DEPENDS_ON
%token <string> KEY_UPDATE_SCRIPT
%token          KEY_END_SCRIPT
%token          KEY_AUTOUPDATE
%token <string> TKN_IDENT
%token <string> TKN_FACT
%token <string> TKN_SCRIPT_LINE
%token <string> TKN_LEX_ERROR

%type  <target> targets
%type  <target> target
%type  <strarr> optional_dependencies
%type  <strarr> dependencies
%type  <script> optional_script
%type  <string> script_source

%%

input: optional_autoupdate targets
    ;

optional_autoupdate:
/* no autoupdate target */ { parser->auto_update = NULL;                 }
| KEY_AUTOUPDATE TKN_IDENT { parser->auto_update = mrp_strdup($2.value); }
;

targets:
  target         { mrp_list_append(&parser->targets, &$1->hook); }
| targets target { mrp_list_append(&parser->targets, &$2->hook); }
| targets error  { YYABORT; }
;

target: KEY_TARGET TKN_IDENT optional_dependencies optional_script {
    yy_res_target_t *t;
    int i;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        mrp_list_init(&t->hook);

        t->name = mrp_strdup($2.value);
        if ($3 != NULL) {
            t->depends = $3->strs;
            t->ndepend = $3->nstr;
        }

        if ($4.source != NULL) {
            t->script_type   = mrp_strdup($4.type);
            t->script_source = mrp_strdup($4.source);
        }

        if (t->name != NULL) {
            mrp_list_append(&parser->targets, &t->hook);
            $$ = t;
        }
        else
            YYABORT;
    }

    mrp_log_info("target '%s':", $2.value);

    if ($3 != NULL) {
        for (i = 0; i < $3->nstr; i++)
            mrp_log_info("    depends on '%s'", $3->strs[i]);
    }
    else
        mrp_log_info("    no dependencies");

    if ($4.source != NULL)
        mrp_log_info("    update script (%s): %s", $4.type, $4.source);
    else
        mrp_log_info("    no update script");
  }
;

optional_dependencies:
  /* no dependencies */       { $$ = NULL; }
| KEY_DEPENDS_ON dependencies { $$ = $2;   }
;

dependencies:
  TKN_IDENT                   { $$ = strarr_append(NULL, $1.value); }
| TKN_FACT                    { $$ = strarr_append(NULL, $1.value); }
| dependencies TKN_IDENT {
    if (!strarr_append($1, $2.value))
        YYABORT;
    else
        $$ = $1;
  }
| dependencies TKN_FACT {
    if (!strarr_append($1, $2.value))
        YYABORT;
    else
        $$ = $1;
  }
;

optional_script:
  /* no script */ {
    $$.type   = NULL;
    $$.source = NULL;
  }
| KEY_UPDATE_SCRIPT script_source KEY_END_SCRIPT {
    $$.type   = $1.value;
    $$.source = $2.value;
  }
;

script_source:
  TKN_SCRIPT_LINE {
      int n;

      n = strlen($1.value) + 2;
      $$.value = mrp_allocz(n);

      if ($$.value != NULL) {
          strcpy($$.value, $1.value);
          $$.value[n - 2] = '\n';
          $$.value[n - 1] = '\0';
      }
      else
          YYABORT;
  }
| script_source TKN_SCRIPT_LINE {
    int o, n;

    o = strlen($1.value);
    n = o + strlen($2.value) + 2;
    $$.value = mrp_reallocz($1.value, o, n);

    if ($$.value == NULL)
        YYABORT;

    strcat($$.value, $2.value);
    $$.value[n - 2] = '\n';
    $$.value[n - 1] = '\0';
  }
;

%%

void yy_res_error(yy_res_parser_t *parser, const char *msg)
{
    MRP_UNUSED(parser);

    mrp_log_error("parse error at %s:%d near token '%s': %s",
                  yy_res_lval.any.source, yy_res_lval.any.line,
                  yy_res_lval.any.token, msg);
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


int parser_setup(yy_res_parser_t *parser, const char *path)
{
    mrp_clear(parser);
    mrp_list_init(&parser->targets);

    if (path != NULL)
        return scanner_push_file(parser, path);
    else
        return TRUE;
}


void parser_cleanup(yy_res_parser_t *parser)
{
    mrp_list_hook_t  *tp, *tn;
    yy_res_target_t  *t;
    yy_res_input_t   *ip, *in;
    char            **dep;
    int               i;

    mrp_list_foreach(&parser->targets, tp, tn) {
        t = mrp_list_entry(tp, typeof(*t), hook);

        mrp_free(t->name);
        mrp_free(t->script_type);
        mrp_free(t->script_source);

        if (t->depends != NULL) {
            for (i = 0, dep = t->depends; i < t->ndepend; i++, dep++)
                mrp_free(*dep);

            mrp_free(t->depends);
        }

        mrp_free(t);
    }

    ip = parser->in;
    while (ip != NULL) {
        in = ip->prev;
        scanner_free_input(ip);
        ip = in;
    }

    ip = parser->done;
    while (ip != NULL) {
        in = ip->prev;
        scanner_free_input(ip);
        ip = in;
    }

    mrp_free(parser->auto_update);
}


int parser_parse_file(yy_res_parser_t *parser, const char *path)
{
    if (parser_setup(parser, path))
        return yy_res_parse(parser) == 0;
    else
        return FALSE;
}
