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

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>

#include "scanner.h"
#include "resolver.h"
#include "resolver-types.h"
#include "fact.h"
#include "target.h"
#include "target-sorter.h"


/*
 * dependency graph used to determine target update orders
 */

typedef struct {
    mrp_resolver_t *r;                   /* resolver context */
    int            *edges;               /* edges between nodes */
    int             nnode;               /* number of graph nodes */
} graph_t;


static graph_t *build_graph(mrp_resolver_t *r);
static int sort_graph(graph_t *g, int target_idx);
static void free_graph(graph_t *g);
static void dump_graph(graph_t *g, FILE *fp);


int sort_targets(mrp_resolver_t *r)
{
    graph_t *g;
    int      i, status;

    g = build_graph(r);

    if (g != NULL) {
        dump_graph(g, stdout);

        status = 0;

        for (i = 0; i < r->ntarget; i++) {
            target_t *t = r->targets + i;

            mrp_free(t->update_targets);
            mrp_free(t->update_facts);
            mrp_free(t->fact_stamps);
            mrp_free(t->directs);
            t->update_targets = NULL;
            t->update_facts   = NULL;
            t->fact_stamps    = NULL;
            t->ndirect        = 0;
        }

        for (i = 0; i < r->ntarget; i++) {
            if (sort_graph(g, i) < 0) {
                mrp_log_error("Failed to determine update order for "
                              "resolver target '%s'.", r->targets[i].name);

                if (errno == ELOOP)
                    mrp_log_error("Cyclic dependency detected.");

                status = -1;
                break;
            }
        }

        free_graph(g);
    }
    else
        status = -1;

    return status;
}


static inline int fact_id(graph_t *g, char *fact)
{
    int i;

    for (i = 0; i < g->r->nfact; i++)
        if (!strcmp(fact, g->r->facts[i].name))
            return i;

    return -1;
}


static inline int target_id(graph_t *g, char *target)
{
    int i;

    for (i = 0; i < g->r->ntarget; i++)
        if (!strcmp(target, g->r->targets[i].name))
            return g->r->nfact + i;

    return -1;
}


static inline char *node_name(graph_t *g, int id)
{
    if (id < g->r->nfact)
        return g->r->facts[id].name;
    else
        return g->r->targets[id - g->r->nfact].name;
}


static inline int node_id(graph_t *g, char *name)
{
    if (name[0] == '$')
        return fact_id(g, name);
    else
        return target_id(g, name);
}


static inline int *edge_markp(graph_t *g, int n1, int n2)
{
    static int invalid = 0;

    if (n1 >= 0 && n2 >= 0)
        return g->edges + (n1 * g->nnode) + n2;
    else
        return &invalid;
}


static inline void mark_node(graph_t *g, int node)
{
    *edge_markp(g, node, node) = 1;
}


static inline void unmark_node(graph_t *g, int node)
{
    *edge_markp(g, node, node) = 0;
}


static inline int node_present(graph_t *g, int node)
{
    return *edge_markp(g, node, node) == 1;
}


static graph_t *build_graph(mrp_resolver_t *r)
{
    graph_t  *g;
    int       tid, did, i, j;
    target_t *t;

    g = mrp_allocz(sizeof(*g));

    if (g != NULL) {
        g->r     = r;
        g->nnode = r->nfact + r->ntarget;
        g->edges = mrp_allocz(g->nnode * g->nnode * sizeof(*g->edges));

        if (g->edges == NULL)
            goto fail;

        for (i = 0; i < r->ntarget; i++) {
            t   = r->targets + i;
            tid = r->nfact + i;

            for (j = 0; j < t->ndepend; j++) {
                mrp_debug("adding edge: %s <- %s", t->depends[j], t->name);
                did = node_id(g, t->depends[j]);

                if (did < 0)
                    goto fail;

                *edge_markp(g, did, tid) = 1;
            }
        }
    }

    return g;

 fail:
    if (g != NULL) {
        mrp_free(g->edges);
        mrp_free(g);
    }
    return NULL;
}


static int mark_present_nodes(graph_t *g, int target_idx)
{
    int       tid, did, i;
    target_t *t;

    tid = g->r->nfact + target_idx;
    t   = g->r->targets + target_idx;

    if (node_present(g, tid))
        return TRUE;

    mark_node(g, tid);

    for (i = 0; i < t->ndepend; i++) {
        did = node_id(g, t->depends[i]);

        if (did < 0)
            return FALSE;

        if (*t->depends[i] == '$')
            mark_node(g, did);
        else {
            if (!mark_present_nodes(g, did - g->r->nfact))
                return FALSE;
        }
    }

    return TRUE;
}


/*
 * queues we use for topological sorting
 */

typedef struct {
    int  size;                           /* max queue capacity */
    int *items;                          /* queue item buffer */
    int  head;                           /* push index */
    int  tail;                           /* pop index */
} que_t;

#define EMPTY_QUE {.items = NULL }       /* initializer for empty queue */


static int que_init(que_t *q, int size)
{
    mrp_free(q->items);

    q->items = mrp_alloc(size * sizeof(*q->items));

    if (q->items != NULL) {
        q->size = size;
        q->head = 0;
        q->tail = 0;

        return TRUE;
    }
    else
        return FALSE;
}


static void que_cleanup(que_t *q)
{
    mrp_free(q->items);
    q->items = NULL;
}


static void que_push(que_t *q, int item)
{
    /* we know the max size, so we don't check for overflow here */
    q->items[q->tail++] = item;
    q->tail            %= q->size;
}


static int que_pop(que_t *q, int *itemp)
{
    if (q->head != q->tail) {
        *itemp   = q->items[q->head++];
        q->head %= q->size;

        return TRUE;
    }
    else {
        *itemp = -1;
        return FALSE;
    }
}


static int sort_graph(graph_t *g, int target_idx)
{
    /*
     * Notes:
     *
     *   We perform a topological sort of the dependency graph here
     *   for our target with the given idx. We include only nodes
     *   for facts and targets which are relevant for our target.
     *   These are the ones which our target directly or indirectly
     *   depends on. We use the otherwise unused diagonal (no target
     *   can depend on itself) of our edge matrix to mark which nodes
     *   are present in the graph. Then we use the following algorithm
     *   to sort the subgraph of relevant nodes:
     *
     *       initialize que L to be empty
     *       initialize que Q with all nodes without incoming edges
     *       while Q is not empty
     *           pop a node <n> from Q
     *           push <n> to L
     *           remove all edges that start from <n>
     *           for all nodes <m> where we removed an incoming node
     *               if <m> has no more incoming edges
     *                   push <m> to Q
     *
     *       if there are any remaining edges
     *           return an error about cyclic dependency
     *       else
     *           L is the sorted subgraph (our target is the last item in L)
     *
     *   The resulted sort order of our target is then used as the
     *   dependency check/update order when the resolver is asked to
     *   update that target.
     */

    target_t *target;
    int       edges[g->nnode * g->nnode];
    que_t     L = EMPTY_QUE, Q = EMPTY_QUE;
    int       i, j, m, id, node, nedge, nfact, ntarget;

    target = g->r->targets + target_idx;

    /* save full graph */
    memcpy(edges, g->edges, sizeof(edges));

    if (!que_init(&L, g->nnode + 1) || !que_init(&Q, g->nnode))
        goto fail;

    /* find and mark relevant nodes in the graph */
    mark_present_nodes(g, target_idx);

    mrp_debug("-- target %s --", target->name);
    /*dump_graph(g, stdout);*/

    /* push all relevant facts, they do not depend on anything */
    for (i = 0; i < g->r->nfact; i++) {
        id = i;
        if (node_present(g, id)) {
            que_push(&Q, id);
            unmark_node(g, id);
        }
    }

    /* push all relevant targets that have no dependencies */
    for (i = 0; i < g->r->ntarget; i++) {
        id = g->r->nfact + i;
        if (g->r->targets[i].depends == NULL && node_present(g, id)) {
            que_push(&Q, id);
            unmark_node(g, id);
        }
    }

    /* try sorting the marked subgraph */
    while (que_pop(&Q, &node)) {
        que_push(&L, node);

        mrp_debug("popped node %s", node_name(g, node));

        for (m = 0; m < g->nnode; m++) {
            if (m == node || !node_present(g, m))
                continue;
            *edge_markp(g, node, m) = 0;
            nedge = 0;
            for (j = 0; j < g->nnode; j++) {
                if (j == m)
                    continue;
                if (node_present(g, j) && *edge_markp(g, j, m))
                    nedge++;
            }
            if (nedge == 0) {
                mrp_debug("node %s empty, pushing it", node_name(g, m));
                que_push(&Q, m);
                unmark_node(g, m);
            }
            else
                mrp_debug("node %s not empty yet", node_name(g, m));
        }
    }

    /* check if the subgraph has any remaining edges */
    nedge = 0;
    for (node = 0; node < g->nnode; node++) {
        if (!node_present(g, node))
            continue;
        for (m = 0; m < g->nnode; m++) {
            if (m == node || !node_present(g, m))
                continue;
            if (*edge_markp(g, node, m) == 1) {
                errno = ELOOP;
                goto fail;
            }
        }
    }

    mrp_debug("----- %s: graph sorted successfully -----", target->name);

    for (i = 0; i < L.tail; i++)
        mrp_debug(" %s", node_name(g, L.items[i]));
    mrp_debug("-----");

    /* save the result in the given target */
    if (L.tail > 0) {
        nfact   = 0;
        ntarget = 0;

        for (i = 0; i < L.tail; i++) {
            if (L.items[i] < g->r->nfact)
                nfact++;
            else
                ntarget++;
        }

        if (nfact > 0) {
            target->update_facts = mrp_alloc_array(int, nfact + 1);
            target->fact_stamps  = mrp_allocz_array(uint32_t, nfact);

            if (target->update_facts != NULL && target->fact_stamps != NULL) {
                for (i = 0; i < nfact; i++)
                    target->update_facts[i] = L.items[i];
                target->update_facts[i] = -1;
            }
            else
                goto fail;
        }

        if (ntarget > 0) {
            target->update_targets = mrp_alloc_array(int, ntarget + 1);
            if (target->update_targets != NULL) {
                for (i = 0; i < ntarget; i++)
                    target->update_targets[i] = L.items[nfact+i] - g->r->nfact;
                target->update_targets[i] = -1;
            }
            else
                goto fail;
        }

        target->ndirect = 0;
        target->directs = mrp_allocz_array(int, target->ndepend);

        if (target->ndepend == 0 || target->directs != NULL) {
            fact_t   *f;
            target_t *t;

            for (i = 0; i < target->ndepend; i++) {
                if (*target->depends[i] != '$')
                    continue;

                f = lookup_fact(g->r, target->depends[i]);

                if (f != NULL)
                    target->directs[target->ndirect++] = f - g->r->facts;
                else
                    target->directs[target->ndirect++] = -1;
            }

            for (i = 0; i < target->ndepend; i++) {
                if (*target->depends[i] == '$')
                    continue;

                t = lookup_target(g->r, target->depends[i]);

                if (t != NULL)
                    target->directs[target->ndirect++] = g->r->nfact +
                        t - g->r->targets;
                else
                    target->directs[target->ndirect++] = -1;
            }
        }
        else
            goto fail;
    }

    que_cleanup(&L);
    que_cleanup(&Q);

    /* restore the original full graph */
    memcpy(g->edges, edges, sizeof(edges));

    return 0;

 fail:
    que_cleanup(&L);
    que_cleanup(&Q);

    memcpy(g->edges, edges, sizeof(edges));

    return -1;
}


static void free_graph(graph_t *g)
{
    if (g != NULL) {
        mrp_free(g->edges);
        mrp_free(g);
    }
}


static void dump_graph(graph_t *g, FILE *fp)
{
    int i, j;

    fprintf(fp, "Graph edges:\n");

    fprintf(fp, "  %20.20s ", "");
    for (i = 0; i < g->nnode; i++)
        fprintf(fp, " %d", i % 10);
    fprintf(fp, "\n");

    for (i = 0; i < g->nnode; i++) {
        fprintf(fp, "  %20.20s: ", node_name(g, i));
        for (j = 0; j < g->nnode; j++)
            fprintf(fp, "%d ", *edge_markp(g, i, j));
        fprintf(fp, "\n");
    }
}
