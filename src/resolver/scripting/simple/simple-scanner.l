%{ /* -*- c -*- */

#define YY_DECL int yy_smpl_lex(yy_smpl_parser_t *parser)

#include <stdio.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include "murphy/resolver/resolver.h"
#include "murphy/resolver/scripting/simple/simple-scanner.h"
#include "murphy/resolver/scripting/simple/token.h"
#include "murphy/resolver/scripting/simple/simple-parser-api.h"
#include "murphy/resolver/scripting/simple/simple-parser.h"

#define yy_smpl_create_buffer    yy_smpl__create_buffer
#define yy_smpl_delete_buffer    yy_smpl__delete_buffer
#define yy_smpl_switch_to_buffer yy_smpl__switch_to_buffer
#define yy_smpl_scan_buffer      yy_smpl__scan_buffer
#define yy_smpl_scan_string      yy_smpl__scan_string
#define yy_smpl_input            input
#define yy_smpl_unput            unput

/*
 * lexical analyser input sources
 */

int simple_scanner_setup(yy_smpl_parser_t *parser, const char *script)
{
    parser->yybuf = yy_smpl_scan_string(script);

    if (parser->yybuf != NULL)
        return TRUE;
    else
        return FALSE;
}


void simple_scanner_cleanup(yy_smpl_parser_t *parser)
{
    if (parser->yybuf != NULL) {
        yy_smpl_delete_buffer(parser->yybuf);
        parser->yybuf = NULL;
    }
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

static char *save_token(yy_smpl_parser_t *parser, char *str, size_t size)
{
    char *token;

    if (!size)
        size = strlen(str);

    if (parser->offs + size + 1 >= YY_SMPL_RINGBUF_SIZE)
        parser->offs = 0;

    token = parser->ringbuf + parser->offs;
    parser->offs += size + 1;

#ifdef __MURPHY_SIMPLE_SCRIPT_CHECK_RINGBUF__
    if (*token != '\0') {
        mrp_log_error("Token ring buffer overflow in simple-script lexical "
                      "analyser.");
        exit(1);
    }
#endif

    strncpy(token, str, size);
    token[size] = '\0';

    yy_smpl_lval.any.token = token;
    yy_smpl_lval.any.line  = parser->line;
    yy_smpl_lval.any.size  = size;

    return token;
}


/*
 * string token types (must include all token types passed via STRING_TOKEN)
 */

typedef enum {
    STRING_TYPE_IDENT,
    STRING_TYPE_CONTEXT_VAR,
    STRING_TYPE_STRING,
} string_type_t;


#define KEYWORD_TOKEN(tkn) do {                         \
        save_token(parser, yy_smpl_text, yy_smpl_leng); \
                                                        \
        mrp_debug("KEY_%s", #tkn);                      \
                                                        \
        return KEY_##tkn;                               \
    } while (0)


#define STRING_TOKEN(tkn) do {                          \
        char *_t, *_v;                                  \
        int   _l;                                       \
                                                        \
        switch (STRING_TYPE_##tkn) {                    \
        case STRING_TYPE_STRING:                        \
            _v = yy_smpl_text + 1;                      \
            _l = yy_smpl_leng - 2;                      \
            break;                                      \
        case STRING_TYPE_CONTEXT_VAR:                   \
            _v = yy_smpl_text + 1;                      \
            _l = yy_smpl_leng - 1;                      \
            break;                                      \
        default:                                        \
            _v = yy_smpl_text;                          \
            _l = yy_smpl_leng;                          \
        }                                               \
                                                        \
        _t = save_token(parser, _v, _l);                \
        yy_smpl_lval.string.value = _t;                 \
                                                        \
        mrp_debug("TKN_%s: '%s'", #tkn, _t);            \
                                                        \
        return TKN_##tkn;                               \
    } while (0)


#define OTHER_TOKEN(tkn) do {                           \
        char *_t, *_v;                                  \
        int   _l;                                       \
                                                        \
        _v = yy_smpl_text;                              \
        _l = yy_smpl_leng;                              \
                                                        \
        _t = save_token(parser, _v, _l);                \
        yy_smpl_lval.string.value = _t;                 \
                                                        \
        mrp_debug("TKN_%s: '%s'", #tkn, _t);            \
                                                        \
        return TKN_##tkn;                               \
    } while (0)


#define INTEGER_TOKEN(tkn, type) do {                                  \
        typeof(yy_smpl_lval.type.value)  _value;                       \
        char *_tkn, *_end;                                             \
                                                                       \
        if (#type[0] == 'u')                                           \
            _value = (typeof(_value))                                  \
                strtoull(yy_smpl_text, &_end, 0);                      \
        else                                                           \
            _value = (typeof(_value))                                  \
                strtoll(yy_smpl_text, &_end, 0);                       \
                                                                       \
        _tkn = save_token(parser, yy_smpl_text, yy_smpl_leng);         \
                                                                       \
        if (!*_end) {                                                  \
            yy_smpl_lval.type.value = _value;                          \
            mrp_debug("TKN_%s: '%s'", #tkn, _tkn);                     \
                                                                       \
            return TKN_##tkn;                                          \
        }                                                              \
        else {                                                         \
            if ((_end[0] == 'S' || _end[0] == 'U') &&                  \
                ((_end[1] == '8' && !_end[2])                    ||    \
                 (_end[1] == '1' && _end[2] == '6' && !_end[3])  ||    \
                 (_end[1] == '3' && _end[2] == '2' && !_end[3])  ||    \
                 (_end[1] == '6' && _end[2] == '4' && !_end[3]))) {    \
                yy_smpl_lval.type.value = _value;                      \
                mrp_debug("TKN_%s: '%s'", #tkn, _tkn);                 \
                                                                       \
                return TKN_##tkn;                                      \
            }                                                          \
            else {                                                     \
                yy_smpl_lval.error.value = "couldn't parse integer";   \
                mrp_debug("TKN_ERROR: failed to parse integer.");      \
                                                                       \
                return TKN_ERROR;                                      \
            }                                                          \
        }                                                              \
    } while (0)


#define DOUBLE_TOKEN() do {                                            \
        char *_tkn, *_end;                                             \
                                                                       \
        yy_smpl_lval.dbl.value = strtod(yy_smpl_text, &_end);          \
                                                                       \
        _tkn = save_token(parser, yy_smpl_text, yy_smpl_leng);         \
                                                                       \
        if (!*_end) {                                                  \
            mrp_debug("TKN_DOUBLE: '%s'", _tkn);                       \
                                                                       \
            return TKN_DOUBLE;                                         \
        }                                                              \
        else {                                                         \
            yy_smpl_lval.error.value = "couldn't parse integer";       \
            mrp_debug("TKN_ERROR: failed to parse integer.");          \
                                                                       \
            return TKN_ERROR;                                          \
        }                                                              \
    } while (0)


#define IGNORE_TOKEN(tkn) do {                                         \
        mrp_debug("ignore %s ('%s')", #tkn, yy_smpl_text);             \
    } while (0)


#define PROCESS_ESCAPE() do {                                           \
        int _c;                                                         \
                                                                        \
        switch ((_c = yy_smpl_input())) {                               \
        case '\n':                                                      \
            mrp_debug("ignore escaped '\\n'");                          \
            parser->line++;                                             \
            break;                                                      \
        default:                                                        \
            mrp_debug("escaped '%c'", _c);                              \
            yy_smpl_unput(_c);                                          \
        }                                                               \
    } while (0)

%}

%option warn
%option batch
%option noyywrap


WS                    [ \t]+
EMPTY_LINE            [ \t]*$
ESCAPE                \\
IDENT                 [a-zA-Z_][a-zA-Z0-9_]+
CONTEXT_VAR           &{IDENT}
EOL                   \n
STRING                ('[^\n']*')|(\"[^\n\"]*\")
INTEGER               [+-]?[0-9]+
HEXAINT               [+-]?0x[0-9a-fA-F]+
DOUBLE                [+-]?[0-9]+\.[0-9]+
SINT8                 ({INTEGER}|{HEXAINT})S8
UINT8                 ({INTEGER}|{HEXAINT})U8
SINT16                ({INTEGER}|{HEXAINT})S16
UINT16                ({INTEGER}|{HEXAINT})U16
SINT32                ({INTEGER}|{HEXAINT})S32
UINT32                ({INTEGER}|{HEXAINT})U32
SINT64                ({INTEGER}|{HEXAINT})S64
UINT64                ({INTEGER}|{HEXAINT})U64

%%

{IDENT}               { STRING_TOKEN(IDENT);           }
{CONTEXT_VAR}         { STRING_TOKEN(CONTEXT_VAR);     }
{STRING}              { STRING_TOKEN(STRING);          }

{SINT8}               { INTEGER_TOKEN(SINT8 , sint8 ); }
{UINT8}               { INTEGER_TOKEN(UINT8 , uint8 ); }
{SINT16}              { INTEGER_TOKEN(SINT16, sint16); }
{UINT16}              { INTEGER_TOKEN(UINT16, uint16); }
{SINT32}              { INTEGER_TOKEN(SINT32, sint32); }
{UINT32}              { INTEGER_TOKEN(UINT32, uint32); }
{SINT64}              { INTEGER_TOKEN(SINT64, sint64); }
{UINT64}              { INTEGER_TOKEN(UINT64, uint64); }
{INTEGER}             { INTEGER_TOKEN(SINT32, sint32); }
{HEXAINT}             { INTEGER_TOKEN(SINT32, sint32); }
{DOUBLE}              { DOUBLE_TOKEN();                }


\(                    { OTHER_TOKEN(PARENTH_OPEN);     }
\)                    { OTHER_TOKEN(PARENTH_CLOSE);    }
,                     { OTHER_TOKEN(COMMA);            }
=                     { OTHER_TOKEN(EQUAL);            }

{WS}                  { /*IGNORE_TOKEN(WS);*/                         }
{EOL}                 { parser->line++; /*IGNORE_TOKEN(EOL);*/        }
{EMPTY_LINE}          { parser->line++; /*IGNORE_TOKEN(EMPTY_LINE);*/ }
{ESCAPE}              { PROCESS_ESCAPE();                             }

<<EOF>>               { yy_smpl_pop_buffer_state();
                        parser->yybuf = NULL;
                        yyterminate();                                }

.                     { mrp_log_error("Unhandled token '%s'",
                                      yy_smpl_text);                  }
