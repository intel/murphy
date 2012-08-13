#include <stdarg.h>

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>

#include "scanner.h"
#include "resolver-types.h"
#include "target.h"
#include "target-sorter.h"
#include "resolver.h"

mrp_resolver_t *mrp_resolver_parse(const char *path)
{
    yy_res_parser_t  parser;
    mrp_resolver_t  *r;

    mrp_clear(&parser);
    r = mrp_allocz(sizeof(*r));

    if (r != NULL) {
        if (parser_parse_file(&parser, path)) {
            if (create_targets(r, &parser) && sort_targets(r)) {
                parser_cleanup(&parser);

                return r;
            }
        }
    }

    mrp_resolver_cleanup(r);
    parser_cleanup(&parser);

    return NULL;
}


void mrp_resolver_cleanup(mrp_resolver_t *r)
{
    int       i, j;
    target_t *t;
    fact_t   *f;

    if (r != NULL) {
        for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
            mrp_free(t->name);

            for (j = 0; j < t->ndepend; j++)
                mrp_free(t->depends[j]);
            mrp_free(t->depends);

            mrp_free(t->update_facts);
            mrp_free(t->update_targets);

            mrp_free(t->script);
        }

        mrp_free(r->targets);

        for (i = 0, f = r->facts; i < r->nfact; i++, f++) {
            mrp_free(f->name);
        }

        mrp_free(r->facts);
    }

    mrp_free(r);
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
