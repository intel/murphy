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
