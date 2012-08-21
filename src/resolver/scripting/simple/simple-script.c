#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>

#include <murphy/resolver/resolver.h>
#include "simple-parser-api.h"
#include "call.h"


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


static int simple_compile(mrp_script_t *script)
{
    yy_smpl_parser_t  parser;
    simple_script_t  *ss;

    if (simple_parser_parse(&parser, script->source)) {
        ss = mrp_allocz(sizeof(*ss));

        if (ss != NULL) {
            script->data = ss;

            mrp_list_move(&ss->statements, &parser.statements);
            simple_parser_cleanup(&parser);

            return 0;
        }
        else
            simple_parser_cleanup(&parser);
    }

    return -1;
}


static int simple_execute(mrp_script_t *s)
{
    simple_script_t *ss = s->data;

    if (ss != NULL) {
        printf("----- should execute simple script -----\n");
        simple_dump(stdout, ss);
        printf("----------------------------------------\n");
    }

    return TRUE;
}


static void simple_cleanup(mrp_script_t *s)
{
    MRP_UNUSED(s);

    return;
}

MRP_REGISTER_INTERPRETER("simple",
                         simple_compile, simple_execute, simple_cleanup);
