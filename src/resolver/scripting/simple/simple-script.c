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

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>

#include <murphy/resolver/resolver.h>
#include "simple-parser-api.h"
#include "call.h"
#include "builtins.h"


static void simple_dump(FILE *fp, simple_script_t *ss)
{
    mrp_list_hook_t *p, *n;
    function_call_t *c;

    if (ss != NULL) {
        mrp_list_foreach(&ss->statements, p, n) {
            c = mrp_list_entry(p, typeof(*c), hook);

            dump_call(fp, c);
        }
    }
}


static int simple_compile(mrp_scriptlet_t *script)
{
    static int builtins_exported = FALSE;

    yy_smpl_parser_t  parser;
    simple_script_t  *ss;

    if (!builtins_exported) {
        if (!export_builtins())
            return -1;
        else
            builtins_exported = TRUE;
    }

    if (simple_parser_parse(&parser, script->source)) {
        ss = mrp_allocz(sizeof(*ss));

        if (ss != NULL) {
            script->compiled = ss;

            mrp_list_move(&ss->statements, &parser.statements);
            simple_parser_cleanup(&parser);

            return 0;
        }
        else
            simple_parser_cleanup(&parser);
    }

    return -1;
}


static int simple_prepare(mrp_scriptlet_t *s)
{
    simple_script_t *ss = s->compiled;
    mrp_list_hook_t *p, *n;
    function_call_t *c;

    if (ss != NULL) {
        mrp_list_foreach(&ss->statements, p, n) {
            c = mrp_list_entry(p, typeof(*c), hook);

            if (!link_call(c)) {
                errno = ENOENT;
                return -1;
            }
        }

        return 0;
    }
    else {
        errno = EINVAL;
        return -1;
    }
}


static int simple_execute(mrp_scriptlet_t *s, mrp_context_tbl_t *tbl)
{
    simple_script_t *ss = s->compiled;
    mrp_list_hook_t *p, *n;
    function_call_t *c;
    int              status;

    if (ss != NULL) {
        mrp_list_foreach(&ss->statements, p, n) {
            c = mrp_list_entry(p, typeof(*c), hook);

            status = execute_call(c, tbl);

            if (status <= 0)
                return status;
        }
    }

    return TRUE;
}


static void simple_cleanup(mrp_scriptlet_t *s)
{
    MRP_UNUSED(s);
    MRP_UNUSED(simple_dump);

    return;
}

MRP_REGISTER_INTERPRETER("simple",
                         simple_compile, simple_prepare,
                         simple_execute, simple_cleanup);
