%{
#include <stdlib.h>
#include <stdio.h>

#include "mql-parser.h"

#if 0
#define DEBUG_SCANNER
#endif

#ifdef DEBUG_SCANNER
#define PRINT(fmt, args...) printf(fmt, args) 
#else
#define PRINT(fmt, args...)
#endif

#define YY_SKIP_YYWRAP
#define YY_NO_INPUT

#define EOF_TOKEN  \
    YY_FLUSH_BUFFER; \
    yy_mql_lex_destroy(); \
    yyterminate()

#define ARGLESS_TOKEN(t) \
     do { \
         PRINT("%s-", #t); \
         yy_mql_lval.string = #t; \
         return TKN_##t; \
     } while (0)

#define STRING_TOKEN(t) \
     do { \
         PRINT("%s(%s)-", #t, yytext); \
         yy_mql_lval.string = copy_to_ringbuf(yytext);  \
         return TKN_##t; \
     } while (0)

#define NUMBER_TOKEN \
    do { \
        yy_mql_lval.number = strtoul(yytext, NULL, 10); \
        PRINT("NUMBER(%lld)-", yy_mql_lval.number); \
        return TKN_NUMBER; \
    } while(0)

#define FLOATING_TOKEN \
    do { \
       yy_mql_lval.floating = strtod(yytext, NULL); \
       return TKN_FLOATING; \
    } while (0)

#define SEMICOLON_TOKEN \
     do { \
         PRINT("%s\n", "SEMICOLON"); \
         yy_mql_lval.string = "SEMICOLON"; \
         return TKN_SEMICOLON; \
     } while (0)

#define PARAMETER_TOKEN \
    do { \
        mqi_data_type_t type; \
        switch(yytext[1]) { \
        case 's': type = mqi_varchar;  break; \
        case 'd': type = mqi_integer;  break; \
        case 'u': type = mqi_unsignd;  break; \
        case 'f': type = mqi_floating; break; \
        default : type = mqi_unknown;  break; \
        } \
        yy_mql_lval.type = type; \
        PRINT("PARAMETER(%d)-", yy_mql_lval.type); \
        return TKN_PARAMETER; \
    } while(0)


#define YY_INPUT(buf, result, max_size) \
    do { \
        int n; \
        if ((n = yy_mql_input(buf, max_size)) >= 0) \
            result = n; \
        else { \
            result = 0; \
            YY_FATAL_ERROR( "mql input failed" ); \
        } \
    } while (0)


YYSTYPE  yy_mql_lval;



static char  ringbuf[4096];
static char *bufptr = ringbuf;
static char *bufend = ringbuf + sizeof(ringbuf) - 1;

static char *copy_to_ringbuf(const char *);


%}

%option prefix="yy_mql_"
%option batch
%option yylineno
%option case-insensitive
%option nounput noyymore noyywrap

WHITESPACE        [\ \t\n]+

SHOW              show
BEGIN             begin
COMMIT            commit
ROLLBACK          rollback
TRANSACTION       transaction
TRANSACTIONS      transactions
CREATE            create
UPDATE            update
REPLACE           replace
DELETE            delete
DROP              drop
DESCRIBE          describe
TABLE             table
TABLES            tables
INDEX             index
ROWS              rows
COLUMN            column
TRIGGER           trigger
INSERT            insert
SELECT            select
INTO              into
FROM              from
WHERE             where
VALUES            values
SET               set
ON                on
IN                in
OR                or
PERSISTENT        persistent
TEMPORARY         temporary
CALLBACK          callback

VARCHAR           varchar
INTEGER           integer
UNSIGNED          unsigned
REAL              real
BLOB              blob
PARAMETER         \%[sduf]

LOGICAL_AND       \&
LOGICAL_OR        \|
LESS              <
LESS_OR_EQUAL     <=
EQUAL             =
GREATER_OR_EQUAL  >=
GREATER           >
NOT               \!

NOT_SQUOTE        [^\n'\;]
NOT_DQUOTE        [^\n\"\;]

NUMBER            [0-9]+
FLOATING          [0-9]\.[0-9]*
IDENTIFIER        [a-zA-Z]([a-zA-Z0-9_-]*[a-zA-Z0-9])*
QUOTED_STRING     (('{NOT_SQUOTE}*')|(\"{NOT_DQUOTE}*\"))

LEFT_PAREN        \(
RIGHT_PAREN       \)
COMMA             ,
SEMICOLON         ;
PLUS              \+
MINUS             \-
STAR              \*
SLASH             \/




%%

<<EOF>>            { EOF_TOKEN;                        }
{WHITESPACE}       {                                   }

{SHOW}             { ARGLESS_TOKEN (SHOW);             }
{BEGIN}            { ARGLESS_TOKEN (BEGIN);            }
{COMMIT}           { ARGLESS_TOKEN (COMMIT);           }
{ROLLBACK}         { ARGLESS_TOKEN (ROLLBACK);         }
{TRANSACTION}      { ARGLESS_TOKEN (TRANSACTION);      }
{TRANSACTIONS}     { ARGLESS_TOKEN (TRANSACTIONS);     }
{CREATE}           { ARGLESS_TOKEN (CREATE);           }
{UPDATE}           { ARGLESS_TOKEN (UPDATE);           }
{REPLACE}          { ARGLESS_TOKEN (REPLACE);          }
{DELETE}           { ARGLESS_TOKEN (DELETE);           }
{DROP}             { ARGLESS_TOKEN (DROP);             }
{DESCRIBE}         { ARGLESS_TOKEN (DESCRIBE);         }
{TABLE}            { ARGLESS_TOKEN (TABLE);            }
{TABLES}           { ARGLESS_TOKEN (TABLES);           }
{INDEX}            { ARGLESS_TOKEN (INDEX);            }
{ROWS}             { ARGLESS_TOKEN (ROWS);             }
{COLUMN}           { ARGLESS_TOKEN (COLUMN);           }
{TRIGGER}          { ARGLESS_TOKEN (TRIGGER);          }
{INSERT}           { ARGLESS_TOKEN (INSERT);           }
{SELECT}           { ARGLESS_TOKEN (SELECT);           }
{INTO}             { ARGLESS_TOKEN (INTO);             }
{FROM}             { ARGLESS_TOKEN (FROM);             }
{WHERE}            { ARGLESS_TOKEN (WHERE);            }
{VALUES}           { ARGLESS_TOKEN (VALUES);           }
{SET}              { ARGLESS_TOKEN (SET);              }
{ON}               { ARGLESS_TOKEN (ON);               }
{IN}               { ARGLESS_TOKEN (IN);               }
{OR}               { ARGLESS_TOKEN (OR);               }
{PERSISTENT}       { ARGLESS_TOKEN (PERSISTENT);       }
{TEMPORARY}        { ARGLESS_TOKEN (TEMPORARY);        }
{CALLBACK}         { ARGLESS_TOKEN (CALLBACK);         }

{VARCHAR}          { ARGLESS_TOKEN (VARCHAR);          }
{INTEGER}          { ARGLESS_TOKEN (INTEGER);          }
{UNSIGNED}         { ARGLESS_TOKEN (UNSIGNED);         }
{REAL}             { ARGLESS_TOKEN (REAL);             }
{BLOB}             { ARGLESS_TOKEN (BLOB);             }
{PARAMETER}        { PARAMETER_TOKEN;                  }

{LOGICAL_AND}      { ARGLESS_TOKEN (LOGICAL_AND);      }
{LOGICAL_OR}       { ARGLESS_TOKEN (LOGICAL_OR);       }
{LESS}             { ARGLESS_TOKEN (LESS);             }
{LESS_OR_EQUAL}    { ARGLESS_TOKEN (LESS_OR_EQUAL);    }
{EQUAL}            { ARGLESS_TOKEN (EQUAL);            }
{GREATER_OR_EQUAL} { ARGLESS_TOKEN (GREATER_OR_EQUAL); }
{GREATER}          { ARGLESS_TOKEN (GREATER);          }
{NOT}              { ARGLESS_TOKEN (NOT);              }

{LEFT_PAREN}       { ARGLESS_TOKEN (LEFT_PAREN);       }
{RIGHT_PAREN}      { ARGLESS_TOKEN (RIGHT_PAREN);      }
{COMMA}            { ARGLESS_TOKEN (COMMA);            }
{SEMICOLON}        { SEMICOLON_TOKEN;                  }
{PLUS}             { ARGLESS_TOKEN (PLUS);             }
{MINUS}            { ARGLESS_TOKEN (MINUS);            }
{STAR}             { ARGLESS_TOKEN (STAR);             }
{SLASH}            { ARGLESS_TOKEN (SLASH);            }

{NUMBER}           { NUMBER_TOKEN;                     }
{FLOATING}         { FLOATING_TOKEN;                   }
{IDENTIFIER}       { STRING_TOKEN (IDENTIFIER);        }
{QUOTED_STRING}    { STRING_TOKEN (QUOTED_STRING);     }

%%



static char *copy_to_ringbuf(const char *string)
{
   const char *src;
   char       *copy;
   char        qt;

   for (;;) {
        if (bufptr >= bufend)
           bufptr = ringbuf;

        copy = bufptr;

        switch (*(src = string)) {
        case '\'':  qt = *src++;   break;
        case '\"':  qt = *src++;   break;
        default:    qt =  0xff;    break;
        }

        while (bufptr < bufend) {
            if (!(*bufptr++ = *src++)) {
                if (bufptr[-2] == qt)
                  (--bufptr)[-1] = '\0';
                return copy;
            }
        }
   }
}



/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */
