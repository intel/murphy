#ifndef __MURPHY_SIMPLE_SCRIPT_PARSER_API_H__
#define __MURPHY_SIMPLE_SCRIPT_PARSER_API_H__

#include <stdio.h>

#include <murphy/common/list.h>

#include "simple-script.h"

#define YY_SMPL_RINGBUF_SIZE (8 * 1024)            /* token buffer size */

/*
 * a parsed script
 */

typedef struct {
    mrp_list_hook_t  statements;                    /* list of statements */
    void            *yybuf;                         /* scanner buffer */
    int              line;                          /* input line number */
    char             ringbuf[YY_SMPL_RINGBUF_SIZE]; /* token ringbuffer */
    int              offs;                          /* buffer insert offset */
} yy_smpl_parser_t;





int simple_parser_setup(yy_smpl_parser_t *parser, const char *script);
void simple_parser_cleanup(yy_smpl_parser_t *parser);
int simple_parser_parse(yy_smpl_parser_t *parser, const char *script);

#endif /* __MURPHY_SIMPLE_SCRIPT_PARSER_API_H__ */
