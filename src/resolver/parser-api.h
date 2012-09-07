#ifndef __MURPHY_RESOLVER_PARSER_TYPES_H__
#define __MURPHY_RESOLVER_PARSER_TYPES_H__

#include <stdio.h>

#include <murphy/common/list.h>

#define YY_RES_RINGBUF_SIZE (8 * 1024)            /* token buffer size */

/*
 * a parsed target definition
 */

typedef struct {
    char *type;                                   /* script type */
    char *source;                                 /* script source */
} yy_res_script_t;

typedef struct {
    mrp_list_hook_t  hook;                        /* to list of targets */
    char            *name;                        /* target name */
    char           **depends;                     /* target dependencies */
    int              ndepend;                     /* number of dependencies */
    char            *script_type;                 /* update script type */
    char            *script_source;               /* update script source */
} yy_res_target_t;


typedef struct yy_res_input_s yy_res_input_t;

struct yy_res_input_s {
    yy_res_input_t *prev;                         /* previous input */
    void           *yybuf;                        /* scanner buffer */
    char           *name;                         /* name of this input */
    int             line;                         /* line number in input */
    FILE           *fp;                           /* input stream */
};


typedef struct {
    mrp_list_hook_t targets;                      /* list of targets */
    char           *auto_update;                  /* auto-update target */
    char            ringbuf[YY_RES_RINGBUF_SIZE]; /* token ringbuffer */
    int             offs;                         /* buffer insert offset */
    yy_res_input_t *in;                           /* current input */
    yy_res_input_t *done;                         /* processed inputs */
} yy_res_parser_t;


int parser_setup(yy_res_parser_t *parser, const char *path);
void parser_cleanup(yy_res_parser_t *parser);
int parser_parse_file(yy_res_parser_t *parser, const char *path);

#endif /* __MURPHY_RESOLVER_PARSER_TYPES_H__ */
