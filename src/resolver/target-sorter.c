#include <murphy/common/mm.h>
#include <murphy/common/debug.h>

#include "scanner.h"
#include "resolver.h"
#include "resolver-types.h"
#include "target-sorter.h"


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
    int      i, success;

    g = build_graph(r);

    if (g != NULL) {
        dump_graph(g, stdout);

        success = TRUE;
        for (i = 0; i < r->ntarget && success; i++) {
            if (!sort_graph(g, i))
                success = FALSE;
        }

        free_graph(g);
    }

    return success;
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
    if (n1 >= 0 && n2 >= 0)
        return g->edges + (n1 * g->nnode) + n2;
    else
        return NULL;
}


static inline void undelete_node(graph_t *g, int node)
{
    *edge_markp(g, node, node) = 1;
}


static inline void delete_node(graph_t *g, int node)
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


static inline void delete_nodes(graph_t *g)
{
    int i;

    for (i = 0; i < g->nnode; i++)
        g->edges[i * g->nnode + i] = 0;
}


static int undelete_present_nodes(graph_t *g, int target_idx)
{
    int       tid, did, i;
    target_t *t;

    tid = g->r->nfact + target_idx;
    t   = g->r->targets + target_idx;

    if (node_present(g, tid))
        return TRUE;

    undelete_node(g, tid);

    for (i = 0; i < t->ndepend; i++) {
        did = node_id(g, t->depends[i]);

        if (did < 0)
            return FALSE;

        if (*t->depends[i] == '$')
            undelete_node(g, did);
        else {
            if (!undelete_present_nodes(g, did - g->r->nfact))
                return FALSE;
        }
    }

    return TRUE;
}


typedef struct {
    int  size;
    int *items;
    int  head;
    int  tail;
} que_t;


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

#if 0
static que_t *que_alloc(int size)
{
    que_t *q;

    q = mrp_allocz(sizeof(*q));

    if (q != NULL) {
        if (que_init(q, size))
            return q;
        else
            mrp_free(q);
    }

    return NULL;
}
#endif

static void que_cleanup(que_t *q)
{
    mrp_free(q->items);
    q->items = NULL;
}

#if 0
static void que_free(que_t *q)
{
    que_cleanup(q);
    mrp_free(q);
}
#endif

static int que_push(que_t *q, int item)
{
    q->items[q->tail++] = item;
    q->tail            %= q->size;

    return TRUE;
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
    target_t *target;
    int       edges[g->nnode * g->nnode];
    que_t     L = { .items = NULL}, Q = { .items = NULL };
    int       i, j, m, id, node, nedge, nfact, ntarget;

    target = g->r->targets + target_idx;

    memcpy(edges, g->edges, sizeof(edges));

    if (!que_init(&L, g->nnode + 1) || !que_init(&Q, g->nnode))
        goto fail;

    undelete_present_nodes(g, target_idx);

    mrp_debug("-- target %s --", target->name);
    /*dump_graph(g, stdout);*/

    /* push all present facts, they don't depend on anything */
    for (i = 0; i < g->r->nfact; i++) {
        id = i;
        if (node_present(g, id)) {
            que_push(&Q, id);
            delete_node(g, id);
        }
    }

    /* push all present targets that have no dependencies */
    for (i = 0; i < g->r->ntarget; i++) {
        id = g->r->nfact + i;
        if (g->r->targets[i].depends == NULL && node_present(g, id)) {
            que_push(&Q, id);
            delete_node(g, id);
        }
    }

    /* try a topological sort of the nodes present in the graph */
    while (que_pop(&Q, &node)) {
        que_push(&L, node);

        mrp_debug("popped node %s", node_name(g, node));

        /*
         * for each node m with an edge e from node to m do
         *     remove edge e from the graph
         *     if m has no other incoming edges then
         *         insert m into Q
         */
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
                delete_node(g, m);
            }
            else
                mrp_debug("node %s not empty yet", node_name(g, m));
        }
    }

    /* check if graph has still edges */
    nedge = 0;
    for (node = 0; node < g->nnode; node++) {
        if (!node_present(g, node))
            continue;
        for (m = 0; m < g->nnode; m++) {
            if (m == node || !node_present(g, m))
                continue;
            if (*edge_markp(g, node, m) == 1) {
                printf("error: graph has cycles\n");
                printf("error: edge %s <- %s still in graph\n",
                       node_name(g, m), node_name(g, node));
                goto fail;
            }
        }
    }

    mrp_debug("----- %s: graph sorted successfully -----", target->name);

    for (i = 0; i < L.tail; i++)
        mrp_debug(" %s", node_name(g, L.items[i]));
    mrp_debug("-----");

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
            if (target->update_facts != NULL) {
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
    }

    memcpy(g->edges, edges, sizeof(edges));
    que_cleanup(&L);
    que_cleanup(&Q);

    return TRUE;

 fail:
    memcpy(g->edges, edges, sizeof(edges));
    que_cleanup(&L);
    que_cleanup(&Q);

    return FALSE;
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
