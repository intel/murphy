#include <stdarg.h>

#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "resolver-types.h"
#include "resolver.h"
#include "fact.h"
#include "target.h"
#include "script.h"


int create_targets(mrp_resolver_t *r, yy_res_parser_t *parser)
{
    mrp_list_hook_t *lp, *ln;
    yy_res_target_t *pt;
    target_t        *t;
    int              i;

    r->ntarget = 0;
    r->targets = NULL;
    mrp_list_foreach(&parser->targets, lp, ln) {
        if (!mrp_reallocz(r->targets, r->ntarget * sizeof(*r->targets),
                          (r->ntarget + 1) * sizeof(*r->targets)))
            return FALSE;

        pt = mrp_list_entry(lp, typeof(*pt), hook);
        t  = r->targets + r->ntarget;
        r->ntarget++;

        t->name     = pt->name;
        pt->name    = NULL;

        t->depends  = pt->depends;
        t->ndepend  = pt->ndepend;
        pt->depends = NULL;
        pt->ndepend = 0;

        t->script   = pt->script;
        pt->script  = NULL;

        for (i = 0; i < t->ndepend; i++) {
            if (*t->depends[i] == '$')
                if (!create_fact(r, t->depends[i]))
                    return FALSE;
        }
    }

    return TRUE;
}


void dump_targets(mrp_resolver_t *r, FILE *fp)
{
    int       i, j, idx;
    target_t *t;

    fprintf(fp, "%d targets\n", r->ntarget);
    for (i = 0; i < r->ntarget; i++) {
        t = r->targets + i;
        fprintf(fp, "#%d: %s\n", i, t->name);

        fprintf(fp, "  dependencies:");
        if (t->depends != NULL) {
            for (j = 0; j < t->ndepend; j++)
                fprintf(fp, " %s", t->depends[j]);
            fprintf(fp, "\n");

            fprintf(fp, "  facts to check:");
            if (t->update_facts != NULL) {
                for (j = 0; (idx = t->update_facts[j]) >= 0; j++)
                    fprintf(fp, " %s", r->facts[idx].name);
                fprintf(fp, "\n");
            }
            else
                fprintf(fp, "<none>\n");

            fprintf(fp, "  target update order:");
            if (t->update_targets != NULL) {
                for (j = 0; (idx = t->update_targets[j]) >= 0; j++)
                    fprintf(fp, " %s", r->targets[idx].name);
                fprintf(fp, "\n");
            }
            else
                fprintf(fp, "<none>\n");
        }
        else
            fprintf(fp, " <none>\n");

        if (t->script != NULL) {
            fprintf(fp, "  update script:\n");
            fprintf(fp, "%s", t->script);
            fprintf(fp, "  end script\n");
        }
        else
            fprintf(fp, "  no update script\n");
    }
}


static int update_target(mrp_resolver_t *r, target_t *t, va_list ap)
{
    target_t *dep;
    int       needs_update, status;
    int       i, j, fid, tid;

    if (t->update_facts == NULL)
        needs_update = TRUE;
    else {
        needs_update = FALSE;
        for (i = 0; (fid = t->update_facts[i]) >= 0; i++) {
            if (fact_changed(r, fid)) {
                needs_update = TRUE;
                break;
            }
        }
    }

    if (!needs_update)
        return TRUE;

    status = TRUE;

    for (i = 0; (tid = t->update_targets[i]) >= 0; i++) {
        /* XXX TODO
         *     Yuck, need to change how update_{target,fact}s are administered.
         *     Add an nupdate_{target,fact} field as well...
         */
        if (tid == t - r->targets)
            break;

        dep = r->targets + tid;

        if (dep->update_facts == NULL)
            needs_update = TRUE;
        else {
            needs_update = FALSE;
            for (j = 0; (fid = dep->update_facts[j]) >= 0; j++) {
                if (fact_changed(r, dep->update_facts[j])) {
                    needs_update = TRUE;
                    break;
                }
            }
        }

        if (needs_update) {
            status = eval_script(r, dep->script, ap);

            if (status <= 0)
                break;
        }
    }

    if (status > 0)
        status = eval_script(r, t->script, ap);

    return status;
}


int update_target_by_name(mrp_resolver_t *r, const char *name, va_list ap)
{
    target_t *t;
    int       i, status;

    status = FALSE;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!strcmp(t->name, name)) {
            status = update_target(r, t, ap);
            break;
        }
    }

    return status;
}


int update_target_by_id(mrp_resolver_t *r, int id, va_list ap)
{
    if (id < r->ntarget)
        return update_target(r, r->targets + id, ap);
    else
        return FALSE;
}
