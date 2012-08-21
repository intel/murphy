#include <stdarg.h>
#include <errno.h>

#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "resolver-types.h"
#include "resolver.h"
#include "db.h"
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
            return -1;

        pt = mrp_list_entry(lp, typeof(*pt), hook);
        t  = r->targets + r->ntarget;
        r->ntarget++;

        t->name     = pt->name;
        pt->name    = NULL;
        t->depends  = pt->depends;
        t->ndepend  = pt->ndepend;
        pt->depends = NULL;
        pt->ndepend = 0;

        if (pt->script_source != NULL) {
            t->script = create_script(pt->script_type, pt->script_source);

            if (t->script == NULL) {
                if (errno == ENOENT)
                    mrp_log_error("Unsupported script type '%s' used in "
                                  "target '%s'.", t->name, pt->script_type);
                else
                    mrp_log_error("Failed to set up script for target '%s'.",
                                  t->name);

                return -1;
            }
        }

        for (i = 0; i < t->ndepend; i++) {
            if (*t->depends[i] == '$')
                if (!create_fact(r, t->depends[i]))
                    return -1;
        }
    }

    return 0;
}


void destroy_targets(mrp_resolver_t *r)
{
    target_t *t;
    int       i, j;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        mrp_free(t->name);
        mrp_free(t->update_facts);
        mrp_free(t->update_targets);

        for (j = 0; j < t->ndepend; j++)
            mrp_free(t->depends[j]);
        mrp_free(t->depends);

        destroy_script(t->script);
    }

    mrp_free(r->targets);
}


int compile_target_scripts(mrp_resolver_t *r)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (t->script != NULL) {
            if (compile_script(t->script) < 0) {
                mrp_log_error("Failed to compile script for target '%s'.",
                              t->name);
                return -1;
            }
        }
    }

    return 0;
}


static int older_than_facts(mrp_resolver_t *r, target_t *t)
{
    int i, id;

    /*
     * If a target does not depend directly or indirectly on any
     * facts, it always needs to be updated and is considered to
     * be older than its (nonexistent) fact dependencies even if
     * this seems a bit unintuitive at first. If there are fact
     * dependencies the target is considered older if any of the
     * facts have a newer stamp than the target.
     */

    if (t->update_facts != NULL)
        return TRUE;
    else {
        for (i = 0; (id = t->update_facts[i]) >= 0; i++) {
            if (fact_stamp(r, id) > t->stamp)
                return TRUE;
        }
    }

    return FALSE;
}


static int older_than_targets(mrp_resolver_t *r, target_t *t)
{
    int       i, id;
    target_t *dep;

    /*
     * Although the target itself is always the last item in its own
     * sorted target dependencies (as the list is the topologically
     * sorted dependency graph) we don't special case this out here as
     * a target cannot be newer than itself by definition.
     */

    for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
        dep = r->targets + id;

        if (dep->stamp > t->stamp)
            return TRUE;
    }

    return FALSE;
}


static void save_target_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    target_t *dep;
    int       i, id;

    if (t != NULL) {
        memset(buf, (uint32_t)-1, r->ntarget * sizeof(*buf));

        for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
            dep     = r->targets + id;
            buf[id] = dep->stamp;
        }
    }
    else {
        for (id = 0; id < r->ntarget; id++) {
            dep     = r->targets + id;
            buf[id] = dep->stamp;
        }
    }
}


static void restore_target_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    target_t *dep;
    int       i, id;

    if (t != NULL) {
        memset(buf, (uint32_t)-1, r->ntarget * sizeof(*buf));

        for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
            dep        = r->targets + id;
            dep->stamp = buf[id];
        }
    }
    else {
        for (id = 0; id < r->ntarget; id++) {
            dep        = r->targets + id;
            dep->stamp = buf[id];
        }
    }
}


static int update_target(mrp_resolver_t *r, target_t *t, va_list ap)
{
    target_t *dep;
    uint32_t  stamps[r->ntarget];
    int       i, id, status, needs_update, tx_owner;

    save_target_stamps(r, t, stamps);
    if (r->stamp == INVALID_TX) {
        r->stamp = start_transaction(r);
        tx_owner = TRUE;
    }
    else
        tx_owner = FALSE;

    status       = TRUE;
    needs_update = older_than_facts(r, t);

    for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
        dep = r->targets + id;

        if (dep == t)
            break;

        /*                              hmm... is this really needed? */
        if (older_than_facts(r, dep) || older_than_targets(r, dep)) {
            needs_update = TRUE;
            status       = execute_script(r, dep->script, ap);

            if (status <= 0)
                break;
            else
                dep->stamp = r->stamp;
        }
    }

    if (needs_update && status > 0) {
        status = execute_script(r, t->script, ap);

        if (status > 0)
            t->stamp = r->stamp;
    }

    if (status <= 0) {
        restore_target_stamps(r, t, stamps);
        if (tx_owner)
            rollback_transaction(r);
    }

    return status;
}


int update_target_by_name(mrp_resolver_t *r, const char *name, va_list ap)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!strcmp(t->name, name))
            return update_target(r, t, ap);
    }

    return FALSE;
}


int update_target_by_id(mrp_resolver_t *r, int id, va_list ap)
{
    if (id < r->ntarget)
        return update_target(r, r->targets + id, ap);
    else
        return FALSE;
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
            fprintf(fp, "  update script (%s):\n",
                    t->script->interpreter->name);
            fprintf(fp, "%s", t->script->source);
            fprintf(fp, "  end script\n");
        }
        else
            fprintf(fp, "  no update script\n");
    }
}
