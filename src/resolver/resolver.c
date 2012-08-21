#include <stdarg.h>

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>

#include "scanner.h"
#include "resolver-types.h"
#include "target.h"
#include "target-sorter.h"
#include "fact.h"
#include "resolver.h"

mrp_resolver_t *mrp_resolver_parse(const char *path)
{
    yy_res_parser_t  parser;
    mrp_resolver_t  *r;

    mrp_clear(&parser);
    r = mrp_allocz(sizeof(*r));

    if (r != NULL) {
        if (parser_parse_file(&parser, path)) {
            if (create_targets(r, &parser) == 0 &&
                sort_targets(r)            == 0 &&
                compile_target_scripts(r)  == 0) {
                parser_cleanup(&parser);
                return r;
            }
        }
        else
            mrp_log_error("Failed to parse resolver input.");
    }

    mrp_resolver_cleanup(r);
    parser_cleanup(&parser);

    return NULL;
}


void mrp_resolver_cleanup(mrp_resolver_t *r)
{
    if (r != NULL) {
        destroy_targets(r);
        destroy_facts(r);

        mrp_free(r);
    }
}


int mrp_resolver_update(mrp_resolver_t *r, const char *target, ...)
{
    va_list ap;
    int     status;

    MRP_UNUSED(r);

    va_start(ap, target);
    status = update_target_by_name(r, target, ap);
    va_end(ap);

    return status;
}


void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp)
{
    dump_targets(r, fp);
}


void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp)
{
    int     i;
    fact_t *f;

    fprintf(fp, "%d facts\n", r->nfact);
    for (i = 0; i < r->nfact; i++) {
        f = r->facts + i;
        fprintf(fp, "  #%d: %s\n", i, f->name);
    }
}


int mrp_resolver_register_interpreter(mrp_interpreter_t *i)
{
    return register_interpreter(i);
}


int mrp_resolver_unregister_interpreter(const char *name)
{
    mrp_interpreter_t *i;

    i = lookup_interpreter(name);

    if (i != NULL) {
        unregister_interpreter(i);

        return TRUE;
    }
    else
        return FALSE;

}
