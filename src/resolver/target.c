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

#include <stdarg.h>
#include <errno.h>
#include <alloca.h>

#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include <murphy/core/scripting.h>

#include "resolver-types.h"
#include "resolver.h"
#include "fact.h"
#include "events.h"
#include "target-sorter.h"
#include "target.h"


int create_targets(mrp_resolver_t *r, yy_res_parser_t *parser)
{
    mrp_list_hook_t *lp, *ln;
    yy_res_target_t *pt;
    int              auto_update;

    auto_update = -1;

    mrp_list_foreach(&parser->targets, lp, ln) {
        pt = mrp_list_entry(lp, typeof(*pt), hook);

        if (create_target(r, pt->name, (const char **)pt->depends, pt->ndepend,
                          pt->script_type, pt->script_source) == NULL)

            return -1;

        if (parser->auto_update != NULL) {
            if (!strcmp(parser->auto_update, pt->name))
                auto_update = r->ntarget - 1;
        }
    }

    if (auto_update >= 0)
        r->auto_update = r->targets + auto_update;
    else {
        if (parser->auto_update != NULL) {
            mrp_log_error("Auto-update target '%s' does not exist.",
                          parser->auto_update);
            errno = ENOENT;
            return -1;
        }
    }

    return 0;
}


static void purge_target(target_t *t)
{
    int i;

    mrp_free(t->name);
    mrp_free(t->update_facts);
    mrp_free(t->update_targets);
    mrp_free(t->fact_stamps);

    for (i = 0; i < t->ndepend; i++)
        mrp_free(t->depends[i]);
    mrp_free(t->depends);

    mrp_destroy_script(t->script);
}


void destroy_targets(mrp_resolver_t *r)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++)
        purge_target(t);

    mrp_free(r->targets);

    if (r->auto_scheduled != NULL) {
        mrp_del_deferred(r->auto_scheduled);
        r->auto_scheduled = NULL;
    }
}


target_t *create_target(mrp_resolver_t *r, const char *target,
                        const char **depends, int ndepend,
                        const char *script_type, const char *script_source)
{
    target_t *t;
    size_t    old_size, new_size;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!strcmp(t->name, target)) {
            errno = EEXIST;
            return NULL;
        }
    }

    old_size = sizeof(*r->targets) *  r->ntarget;
    new_size = sizeof(*r->targets) * (r->ntarget + 1);

    if (!mrp_reallocz(r->targets, old_size, new_size))
        return NULL;

    t       = r->targets + r->ntarget++;
    t->name = mrp_strdup(target);

    if (t->name == NULL)
        goto undo_and_fail;

    if (depends != NULL) {
        t->depends = mrp_allocz_array(char *, ndepend);

        if (t->depends != NULL) {
            for (i = 0; i < ndepend; i++) {
                t->depends[i] = mrp_strdup(depends[i]);

                if (t->depends[i] == NULL)
                    goto undo_and_fail;
            }

            t->ndepend = ndepend;
        }
        else
            goto undo_and_fail;
    }

    for (i = 0; i < t->ndepend; i++) {
        if (*t->depends[i] == '$')
            if (!create_fact(r, t->depends[i]))
                goto undo_and_fail;
    }

    if (script_source != NULL) {
        t->script = mrp_create_script(script_type, script_source);

        if (t->script == NULL) {
            if (errno == ENOENT)
                mrp_log_error("Unsupported script type '%s' used in "
                              "target '%s'.", script_type, t->name);
            else
                mrp_log_error("Failed to set up script for target '%s'.",
                              t->name);

            goto undo_and_fail;
        }
    }

    return t;


 undo_and_fail:
    purge_target(t);
    old_size = new_size;
    new_size = old_size - 1;
    mrp_reallocz(r->targets, old_size, new_size);

    return NULL;
}


int generate_autoupdate_target(mrp_resolver_t *r, const char *name)
{
    const char **depends;
    int          ndepend, i;
    target_t    *t, *at;

    if (r->auto_update != NULL)
        return FALSE;

    depends = alloca(r->ntarget * sizeof(depends[0]));
    ndepend = 0;

    mrp_debug("constructing autoupdate target '%s'...", name);

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (t->update_facts != NULL && t->update_facts[0] >= 0) {
            mrp_debug("  including target '%s' (%s)", t->name,
                      fact_name(r, t->update_facts[0]));
            depends[ndepend] = t->name;
            ndepend++;
        }
        else
            mrp_debug("  excluding target '%s'", t->name);
    }

    at = create_target(r, name, depends, ndepend, NULL, NULL);

    if (at != NULL) {
        r->auto_update = at;

        return (sort_targets(r) == 0);
    }
    else
        return FALSE;
}


int compile_target_scripts(mrp_resolver_t *r)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!t->prepared) {
            if (mrp_compile_script(t->script) < 0) {
                mrp_log_error("Failed to compile script for target '%s'.",
                              t->name);
                return -1;
            }
        }
    }

    return 0;
}


int prepare_target_scripts(mrp_resolver_t *r)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!t->prepared) {
            if (mrp_prepare_script(t->script) == 0)
                t->prepared = TRUE;
            else {
                mrp_log_error("Failed to prepare script for target '%s'.",
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

    if (t->update_facts == NULL)
        return TRUE;
    else {
        for (i = 0; (id = t->update_facts[i]) >= 0; i++) {
            if (fact_stamp(r, id) > t->fact_stamps[i])
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


static void save_fact_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    int id, idx, i;

    if (t->update_facts != NULL) {
        id  = t - r->targets;
        idx = id * r->nfact;

        for (i = 0; (id = t->update_facts[i]) >= 0; i++, idx++)
            buf[idx] = t->fact_stamps[i];
    }
}


static void restore_fact_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    int id, idx, i;

    if (t->update_facts != NULL) {
        id  = t - r->targets;
        idx = id * r->nfact;

        for (i = 0; (id = t->update_facts[i]) >= 0; i++, idx++)
            t->fact_stamps[i] = buf[idx];
    }
}


static void save_target_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    target_t *dep;
    int       i, id;

    for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
        dep = r->targets + id;
        save_fact_stamps(r, dep, buf);
    }
}


static void restore_target_stamps(mrp_resolver_t *r, target_t *t, uint32_t *buf)
{
    target_t *dep;
    int       i, id;

    for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
        dep = r->targets + id;
        restore_fact_stamps(r, dep, buf);
    }
}


static void update_target_stamps(mrp_resolver_t *r, target_t *t)
{
    int i, id;

    if (t->update_facts != NULL)
        for (i = 0; (id = t->update_facts[i]) >= 0; i++)
            t->fact_stamps[i] = fact_stamp(r, id);
}


static int update_target(mrp_resolver_t *r, target_t *t)
{
    mqi_handle_t  tx;
    target_t     *dep;
    uint32_t      stamps[r->ntarget * r->nfact];
    int           i, id, status, needs_update, level;

    tx = start_transaction(r);

    if (tx == MQI_HANDLE_INVALID) {
        if (errno != 0)
            return -errno;
        else
            return -EINVAL;
    }

    level = r->level++;
    emit_resolver_event(RESOLVER_UPDATE_STARTED, t->name, level);

    save_target_stamps(r, t, stamps);

    status       = TRUE;
    needs_update = older_than_facts(r, t);

    for (i = 0; (id = t->update_targets[i]) >= 0; i++) {
        dep = r->targets + id;

        if (dep == t)
            break;

        /*                              hmm... is this really needed? */
        if (older_than_facts(r, dep) || older_than_targets(r, dep)) {
            needs_update = TRUE;
            status       = mrp_execute_script(dep->script, r->ctbl);

            if (status <= 0)
                break;
            else
                update_target_stamps(r, dep);
        }
    }

    if (needs_update && status > 0) {
        status = mrp_execute_script(t->script, r->ctbl);

        if (status > 0)
            update_target_stamps(r, t);
    }

    if (status <= 0) {
        rollback_transaction(r, tx);
        restore_target_stamps(r, t, stamps);
        emit_resolver_event(RESOLVER_UPDATE_FAILED, t->name, level);
    }
    else {
        if (!commit_transaction(r, tx)) {
            restore_target_stamps(r, t, stamps);
            if (errno != 0)
                status = -errno;
            else
                status = -EINVAL;
        }
    }

    if (status <= 0)
        emit_resolver_event(RESOLVER_UPDATE_FAILED, t->name, level);
    else
        emit_resolver_event(RESOLVER_UPDATE_DONE  , t->name, level);

    r->level--;

    return status;
}


int update_target_by_name(mrp_resolver_t *r, const char *name)
{
    target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!strcmp(t->name, name))
            return update_target(r, t);
    }

    return FALSE;
}


int update_target_by_id(mrp_resolver_t *r, int id)
{
    if (id < r->ntarget)
        return update_target(r, r->targets + id);
    else
        return FALSE;
}


static int autoupdate_target(mrp_resolver_t *r)
{
    if (r->auto_update != NULL)
        return mrp_resolver_update_targetl(r, r->auto_update->name, NULL);
    else
        return TRUE;
}


static void autoupdate_cb(mrp_mainloop_t *ml, mrp_deferred_t *d,
                          void *user_data)
{
    mrp_resolver_t *r = (mrp_resolver_t *)user_data;

    MRP_UNUSED(ml);

    mrp_debug("running scheduled target autoupdate");
    mrp_disable_deferred(d);
    autoupdate_target(r);
}


int schedule_target_autoupdate(mrp_resolver_t *r)
{
    if (r->auto_update != NULL) {
        if (r->ctx != NULL && r->auto_scheduled == NULL)
            r->auto_scheduled = mrp_add_deferred(r->ctx->ml, autoupdate_cb, r);

        if (r->auto_scheduled != NULL)
            mrp_enable_deferred(r->auto_scheduled);
        else
            return FALSE;

        mrp_debug("scheduled target autoupdate (%s)", r->auto_update->name);
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
            fprintf(fp, "  update script (%s):\n",
                    t->script->interpreter->name);
            fprintf(fp, "%s", t->script->source);
            fprintf(fp, "  end script\n");
        }
        else
            fprintf(fp, "  no update script\n");
    }
}
