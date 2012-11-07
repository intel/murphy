%{ /* -*- c -*- */

#define YY_DECL int yy_res_lex(yy_res_parser_t *parser)

#include <stdio.h>
#include <limits.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include "murphy/resolver/resolver.h"
#include "murphy/resolver/scanner.h"
#include "murphy/resolver/token.h"
#include "murphy/resolver/parser-api.h"
#include "murphy/resolver/parser.h"

#define YY_NO_INPUT

#define yy_res_create_buffer    yy_res__create_buffer
#define yy_res_delete_buffer    yy_res__delete_buffer
#define yy_res_switch_to_buffer yy_res__switch_to_buffer
#define yy_res_scan_buffer      yy_res__scan_buffer


/*
 * lexical analyser input sources
 *
 * We support an include mechanism similar to the #include directive
 * of the C-preprocessor. When one input file includes another, this
 * is treated as if the latter file was verbatim copied in place of
 * the include directive in the former file.
 *
 * The include mechanism is (almost) entirely implemented in the lexical
 * analyser and is transparet to/hidden from the parser. The functions
 * below and the macro CHECK_EOF take care of inclusion.
 *
 * Note that currently there is no any attempt to check for and prevent
 * circular inclusion loops...
 */

int scanner_push_file(yy_res_parser_t *parser, const char *file)
{
    yy_res_input_t *in;
    char            buf[PATH_MAX], *base;
    const char     *path;
    int             len;
    FILE           *fp;

    if (*file != '/' && parser->in != NULL) {
        base = strrchr(parser->in->name, '/');
        if (base != NULL) {
            len = base - parser->in->name;
            snprintf(buf, sizeof(buf), "%*.*s/%s",
                     len, len, parser->in->name, file);
            path = buf;
        }
        else
            path = file;
    }
    else
        path = file;

    fp = fopen(path, "r");

    if (fp != NULL) {
        in = mrp_allocz(sizeof(*in));

        if (in != NULL) {
            in->fp    = fp;
            in->name  = mrp_strdup(path);
            in->line  = 1;
            in->yybuf = yy_res_create_buffer(in->fp, YY_BUF_SIZE);
            in->prev  = parser->in;

            yy_res_switch_to_buffer(in->yybuf);

            parser->in = in;

            return TRUE;
        }
        else
            fclose(fp);
    }
    else
        mrp_log_error("Failed to open input file '%s' ('%s', error: %s).",
                      file, path, strerror(errno));

    return FALSE;
}


void scanner_free_input(yy_res_input_t *in)
{
    if (in != NULL) {
        if (in->fp != NULL)
            fclose(in->fp);
        mrp_free(in->name);
        yy_res_delete_buffer(in->yybuf);

        mrp_free(in);
    }
}


int scanner_pop_input(yy_res_parser_t *parser)
{
    yy_res_input_t *in, *prev;

    if (parser->in != NULL) {
        in   = parser->in;
        prev = in->prev;

        in->prev     = parser->done;
        parser->done = in;

        parser->in = prev;
        if (prev != NULL) {
            yy_res_switch_to_buffer(prev->yybuf);

            return TRUE;                 /* more input to process */
        }
    }

    return FALSE;                        /* no more input */
}


/*
 * ringbuffer of tokens
 *
 * To simplify the lifecycle management of tokens passed between the
 * lexical analyser and the parser we collect them into a ring buffer
 * instead of dynamic allocation. This simplifies both the lexical
 * analyser and the parser and allows us to have sane owner allocates /
 * owner frees allocation semantics. The price we pay for this is that
 * the ring buffer must be big enough to accomodate all the unprocessed
 * tokens between bison rule reductions.
 */

static char *save_token(yy_res_parser_t *parser, char *str, size_t size)
{
    char *token;

    if (!size)
        size = strlen(str);

    if (parser->offs + size + 1 >= YY_RES_RINGBUF_SIZE)
        parser->offs = 0;

    token = parser->ringbuf + parser->offs;
    parser->offs += size + 1;

#ifdef __MURPHY_RESOLVER_CHECK_RINGBUF__
    if (*token != '\0') {
        mrp_log_error("Token ring buffer overflow in resolver lexical "
                      "analyser.");
        exit(1);
    }
#endif

    strncpy(token, str, size);
    token[size] = '\0';

    yy_res_lval.any.token  = token;
    yy_res_lval.any.source = parser->in->name;
    yy_res_lval.any.line   = parser->in->line;
    yy_res_lval.any.size   = size;

    return token;
}


/*
 * string token types (must include all token types passed via STRING_TOKEN)
 */

typedef enum {
    STRING_TYPE_IDENT,
    STRING_TYPE_FACT,
    STRING_TYPE_SCRIPT_LINE,
} string_type_t;


#define KEYWORD_TOKEN(tkn) do {                         \
        save_token(parser, yy_res_text, yy_res_leng);   \
                                                        \
        mrp_debug("KEY_%s", #tkn);                      \
                                                        \
        return KEY_##tkn;                               \
    } while (0)

#define KEYWORD_TOKENV(tkn, val, size) do {             \
        size_t  _size;                                  \
        char   *_t;                                     \
                                                        \
        _size = size ? size : strlen(val);              \
        _t    = save_token(parser, val, _size);         \
        yy_res_lval.string.value = _t;                  \
                                                        \
        mrp_debug("KEY_%s ('%s')", #tkn, val);          \
                                                        \
        return KEY_##tkn;                               \
    } while (0)


#define STRING_TOKEN(tkn) do {                          \
        char *_t, *_v;                                  \
        int   _l;                                       \
                                                        \
        switch (STRING_TYPE_##tkn) {                    \
        case STRING_TYPE_FACT:                          \
            _v = yy_res_text;                           \
            _l = yy_res_leng;                           \
            break;                                      \
        default:                                        \
            _v = yy_res_text;                           \
            _l = yy_res_leng;                           \
        }                                               \
                                                        \
        _t = save_token(parser, _v, _l);                \
        yy_res_lval.string.value = _t;                  \
                                                        \
        mrp_debug("TKN_%s ('%s')", #tkn, _t);           \
                                                        \
        return TKN_##tkn;                               \
    } while (0)


#define IGNORE_TOKEN(tkn) do {                          \
        mrp_debug("ignore %s ('%s')", #tkn,             \
                  yy_res_text);                         \
    } while (0)


#define INCLUDE_FILE() do {                             \
        char *_p;                                       \
        int   _l;                                       \
                                                        \
        _p = strchr(yy_res_text, '"');                  \
        if (_p != NULL) {                               \
            _p++;                                       \
            _l = yy_res_leng - (_p - yy_res_text);      \
            _p = save_token(parser, _p, _l - 1);        \
                                                        \
            mrp_debug("including file '%s'...", _p);    \
                                                        \
            if (!scanner_push_file(parser, _p))         \
                return TKN_LEX_ERROR;                   \
        }                                               \
    } while (0)

#define CHECK_EOF() do {                                \
        if (!scanner_pop_input(parser))                 \
            yyterminate();                              \
    } while (0)

%}

%option warn
%option batch
%option noyywrap
%option nounput

WS                    [ \t]+
OWS                   [ \t]*
ESCAPED_EOL           \\\n
EOL                   \n
COMMENT               #.*

INCLUDE               ^include{WS}\"[^\"]*\"

TARGET                ^target
AUTOUPDATE            ^auto-update-target
DEPENDS_ON            depends\ on
IDENT                 [a-zA-Z_][a-zA-Z0-9_]+
FACT                  \${IDENT}(\.{IDENT})*
UPDATE_SCRIPT         ^{WS}(update\ script|update\ script{OWS}\({OWS}{IDENT}{OWS}\))
END_SCRIPT            ^{WS}end\ script
PAREN_OPEN            \(
PAREN_CLOSE           \)
SCRIPT_LINE           ^{WS}.*

%x SCRIPT

%%

{TARGET}              { KEYWORD_TOKEN(TARGET);                         }
{AUTOUPDATE}          { KEYWORD_TOKEN(AUTOUPDATE);                     }
{DEPENDS_ON}          { KEYWORD_TOKEN(DEPENDS_ON);                     }
{IDENT}               { STRING_TOKEN(IDENT);                           }
{FACT}                { STRING_TOKEN(FACT);                            }

{UPDATE_SCRIPT}       {
                        char   *type, *end, *p;
                        size_t  len;

                        BEGIN(SCRIPT);

                        type = strchr(yy_res_text, '(');

                        if (type != NULL) {
                            end = strchr(type, ')');
                            len = end - type - 1;
                            p   = type + 1;
                            while (*p == ' ' || *p == '\t')
                                p++, len--;
                            while (len > 0 &&
                                   (p[len-1] == ' ' || p[len-1] == '\t'))
                                len--;

                            KEYWORD_TOKENV(UPDATE_SCRIPT, p, len);
                        }
                        else
                            KEYWORD_TOKENV(UPDATE_SCRIPT, "simple", 0);
                      }
<SCRIPT>{END_SCRIPT}  { BEGIN(INITIAL); KEYWORD_TOKEN(END_SCRIPT);     }
<SCRIPT>{SCRIPT_LINE} { STRING_TOKEN(SCRIPT_LINE);                     }

<*>{WS}               { /*IGNORE_TOKEN(WS);*/                          }
<*>{EOL}              { parser->in->line++; /*IGNORE_TOKEN(EOL);*/     }
{ESCAPED_EOL}         { parser->in->line++; /*IGNORE_TOKEN(EOL);*/     }
<INITIAL>{COMMENT}    { parser->in->line++; /*IGNORE_TOKEN(COMMENT);*/ }
{INCLUDE}             { INCLUDE_FILE();                                }
<<EOF>>               { CHECK_EOF();                                   }

.                     { mrp_log_error("Unhandled token '%s'",
                                      yy_res_text);                    }
