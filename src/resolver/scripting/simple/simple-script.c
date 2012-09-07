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

    return;
}

MRP_REGISTER_INTERPRETER("simple",
                         simple_compile, simple_prepare,
                         simple_execute, simple_cleanup);
