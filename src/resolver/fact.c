#include <murphy/common/mm.h>

#include "resolver-types.h"
#include "resolver.h"
#include "fact.h"


int create_facts(mrp_resolver_t *r)
{
    target_t *t;
    int       i, j;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        for (j = 0; i < t->ndepend; j++) {
            if (*t->depends[j] == '$')
                if (!create_fact(r, t->depends[j]))
                    return FALSE;
        }
    }

    return TRUE;
}


int create_fact(mrp_resolver_t *r, char *fact)
{
    int     i;
    fact_t *f;

    for (i = 0; i < r->nfact; i++) {
        if (!strcmp(r->facts[i].name, fact))
            return TRUE;
    }

    if (!mrp_reallocz(r->facts, r->nfact * sizeof(*r->facts),
                      (r->nfact + 1) * sizeof(*r->facts)))
        return FALSE;

    f = r->facts + r->nfact++;
    f->name = mrp_strdup(fact);

    if (f->name != NULL)
        return TRUE;
    else
        return FALSE;
}


void destroy_facts(mrp_resolver_t *r)
{
    fact_t *f;
    int     i;

    for (i = 0, f = r->facts; i < r->nfact; i++, f++)
        mrp_free(f->name);

    mrp_free(r->facts);
}


int fact_changed(mrp_resolver_t *r, int id)
{
    MRP_UNUSED(r);
    MRP_UNUSED(id);

    return TRUE;
}


uint32_t fact_stamp(mrp_resolver_t *r, int id)
{
    fact_t *fact = r->facts + id;

    return fact->stamp;
}
