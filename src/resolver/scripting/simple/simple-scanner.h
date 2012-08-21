#ifndef __MURPHY_SIMPLE_SCRIPT_SCANNER_H__
#define __MURPHY_SIMPLE_SCRIPT_SCANNER_H__

#include "murphy/resolver/scripting/simple/simple-parser-api.h"

int simple_scanner_setup(yy_smpl_parser_t *parser, const char *script);
void simple_scanner_cleanup(yy_smpl_parser_t *parser);
int yy_smpl_lex(yy_smpl_parser_t *parser);

#endif /* __MURPHY_RESOLVER_SCANNER_H__ */
