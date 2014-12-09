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
    mrp_free(t->directs);

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
    int       i, j, found, nduplicate;

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
            nduplicate = 0;
            for (i = 0; i < ndepend; i++) {
                found = FALSE;
                for (j = 0; j < i; j++)
                    if (!strcmp(depends[i], depends[j]))
                        found = TRUE;
                if (!found) {
                    t->depends[i - nduplicate] = mrp_strdup(depends[i]);

                    if (t->depends[i] == NULL)
                        goto undo_and_fail;
                }
                else
                    nduplicate++;
            }

            t->ndepend = ndepend - nduplicate;

            if (nduplicate > 0) {
                mrp_reallocz(t->depends, ndepend, t->ndepend);
                mrp_log_warning("Filtered out %d duplicate%s dependencies "
                                "from target '%s'.", nduplicate,
                                nduplicate == 1 ? "" : "s", t->name);
            }
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
    mrp_realloc(r->targets, old_size);
    r->ntarget--;

    return NULL;
}


int generate_autoupdate_target(mrp_resolver_t *r, const char *name)
{
    const char **depends;
    int          ndepend, i;
    target_t    *t, *at;

    if (r->auto_update != NULL)
        return FALSE;

    mrp_debug("constructing autoupdate target '%s'...", name);

    if (r->ntarget > 0) {
        depends = alloca(r->ntarget * sizeof(depends[0]));
        ndepend = 0;

        if (sort_targets(r) != 0) {
            mrp_debug("failed to sort dependency graph");
            return FALSE;
        }

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
    }
    else {
        depends = NULL;
        ndepend = 0;
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
#ifdef CHECK_TRANSITIVE_CLOSURE_OF_FACTS
        for (i = 0; (id = t->update_facts[i]) >= 0; i++) {
            if (fact_stamp(r, id) > t->fact_stamps[i])
                return TRUE;
        }
#else
        for (i = 0; i < t->ndirect; i++) {
            id = t->directs[i];

            if (id < r->nfact) {
                if (fact_stamp(r, id) > t->fact_stamps[i])
                    return TRUE;
            }
        }
#endif
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

    t->stamp = r->stamp;
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

    r->stamp = r->stamp + 1;

    level = r->level++;
    emit_resolver_event(r, RESOLVER_UPDATE_STARTED, t->name, level);

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
        emit_resolver_event(r, RESOLVER_UPDATE_FAILED, t->name, level);
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
        emit_resolver_event(r, RESOLVER_UPDATE_FAILED, t->name, level);
    else
        emit_resolver_event(r, RESOLVER_UPDATE_DONE  , t->name, level);

    r->level--;

    return status;
}


target_t *lookup_target(mrp_resolver_t *r, const char *name)
{
   target_t *t;
    int       i;

    for (i = 0, t = r->targets; i < r->ntarget; i++, t++) {
        if (!strcmp(t->name, name))
            return t;
    }

    return NULL;
}


int update_target_by_name(mrp_resolver_t *r, const char *name)
{
    target_t *t = lookup_target(r, name);

    if (t != NULL)
        return update_target(r, t);
    else
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


static void autoupdate_cb(mrp_deferred_t *d, void *user_data)
{
    mrp_resolver_t *r = (mrp_resolver_t *)user_data;

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

    for (i = 0; i < r->ntarget; i++) {
        t = r->targets + i;
        fprintf(fp, "#%d: %s (@%u)\n", i, t->name, t->stamp);

        fprintf(fp, "  dependencies:");
        if (t->depends != NULL) {
            for (j = 0; j < t->ndepend; j++)
                fprintf(fp, " %s", t->depends[j]);
            fprintf(fp, "\n");

            fprintf(fp, "  facts to check:");
            if (t->update_facts != NULL) {
                for (j = 0; (idx = t->update_facts[j]) >= 0; j++)
                    fprintf(fp, " %s (@%u)", r->facts[idx].name,
                            t->fact_stamps[j]);
                fprintf(fp, "\n");
            }
            else
                fprintf(fp, "<none>\n");

            fprintf(fp, "  target update order:");
            if (t->update_targets != NULL) {
                for (j = 0; (idx = t->update_targets[j]) >= 0; j++)
                    fprintf(fp, " %s (@%u)", r->targets[idx].name,
                            r->targets[idx].stamp);
                fprintf(fp, "\n");
            }
            else
                fprintf(fp, "<none>\n");

            fprintf(fp, "  direct dependencies:");
            if (t->ndirect > 0) {
                for (j = 0; j < t->ndirect; j++) {
                    idx = t->directs[j];
                    if (idx < r->nfact)
                        fprintf(fp, " %s", r->facts[idx].name);
                    else
                        fprintf(fp, " %s", r->targets[idx-r->nfact].name);
                }
                fprintf(fp, "\n");
            }
            else
                fprintf(fp, "<none>\n");
        }
        else
            fprintf(fp, " <none>\n");

        if (t->script != NULL) {
            if (t->script->source != NULL) {
                fprintf(fp, "  update script (%s):\n",
                        t->script->interpreter->name);
                fprintf(fp, "%s", t->script->source);
                fprintf(fp, "  end script\n");
            }
            else if (t->script->data != NULL) {
                fprintf(fp, "  precompiled update (%s):\n",
                        t->script->interpreter->name);
                fprintf(fp, "    %p\n", t->script->data);
                fprintf(fp, "  end script\n");
            }
        }
        else
            fprintf(fp, "  no update script\n");
    }
}

typedef enum {
    DOT_NODE_TYPE_FACT,
    DOT_NODE_TYPE_TABLE,
    DOT_NODE_TYPE_SINK,
    DOT_NODE_TYPE_SELECT,
    DOT_NODE_TYPE_OTHER,
} dot_node_type_t;


static dot_node_type_t dot_node_type(char *name)
{
    if (!name)
        return DOT_NODE_TYPE_OTHER;

    if (name[0] == '$')
        return DOT_NODE_TYPE_FACT;

    if (strncmp(name, "_table_", 7) == 0)
        return DOT_NODE_TYPE_TABLE;

    if (strncmp(name, "_sink_", 6) == 0)
        return DOT_NODE_TYPE_SINK;

    if (strncmp(name, "_select_", 8) == 0)
        return DOT_NODE_TYPE_SELECT;

    return DOT_NODE_TYPE_OTHER;
}


static char *dot_fix(char *name)
{
    dot_node_type_t t = dot_node_type(name);

    switch(t) {
        case DOT_NODE_TYPE_FACT:
            /* remove illegal characters */
            return &name[1];

        case DOT_NODE_TYPE_TABLE:
            return &name[7];

        case DOT_NODE_TYPE_SINK:
            return &name[6];

        case DOT_NODE_TYPE_SELECT:
            return &name[8];

        default:
            break;
    }
    return name;
}


static char *dot_get_shape(dot_node_type_t t)
{
    switch(t) {
        case DOT_NODE_TYPE_FACT:
        case DOT_NODE_TYPE_TABLE:
            return "box";

        case DOT_NODE_TYPE_SINK:
            return "trapezium";

        case DOT_NODE_TYPE_SELECT:
            return "diamond";

        default:
            break;
    }
    return "ellipse";
}


void mrp_resolver_dump_dot_graph(mrp_resolver_t *r, FILE *fp)
{
    int i, j;
    target_t *t;

    fprintf(fp, "digraph decision_graph {\n");

    /* vertexes */
    for (i = 0; i < r->ntarget; i++) {
        dot_node_type_t i_type;
        char *name;

        t = r->targets + i;
        name = dot_fix(t->name);
        if (strcmp(name, "autoupdate") == 0)
            continue;

        i_type = dot_node_type(t->name);
        fprintf(fp, "    %s [shape=%s];\n", name, dot_get_shape(i_type));
    }

    fprintf(fp, "\n");

    /* edges */
    for (i = 0; i < r->ntarget; i++) {
        t = r->targets + i;
        char *i_name = dot_fix(t->name);

        if (strcmp(i_name, "autoupdate") == 0)
            continue;

        if (t->depends != NULL) {
            for (j = 0; j < t->ndepend; j++) {
                char *j_name = dot_fix(t->depends[j]);

                fprintf(fp, "    %s -> %s;\n", i_name, j_name);
            }
        }
    }

    fprintf(fp, "}\n");
}
