#ifndef __MURPHY_RESOLVER_SCANNER_H__
#define __MURPHY_RESOLVER_SCANNER_H__

#include "murphy/resolver/parser-api.h"

int scanner_push_file(yy_res_parser_t *parser, const char *path);
void scanner_free_input(yy_res_input_t *in);

int yy_res_lex(yy_res_parser_t *parser);

#endif /* __MURPHY_RESOLVER_SCANNER_H__ */
