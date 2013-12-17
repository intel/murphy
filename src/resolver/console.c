#include <murphy/core/console.h>
#include <murphy/resolver/resolver.h>
#include <murphy/resolver/target.h>

static void dump(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    mrp_context_t *ctx = c->ctx;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    if (ctx->r != NULL) {
        mrp_resolver_dump_facts(ctx->r, c->stdout);
        mrp_resolver_dump_targets(ctx->r, c->stdout);
    }
}

static void dot(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    mrp_context_t *ctx = c->ctx;

    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    if (ctx->r != NULL) {
        mrp_resolver_dump_dot_graph(ctx->r, c->stdout);
    }
}

#define RESOLVER_DESCRIPTION                                              \
    "Resolver commands provide runtime diagnostics and debugging for\n"   \
    "the Murphy resolver.\n"

#define DUMP_SYNTAX  "dump"
#define DUMP_SUMMARY "dump the resolver facts and targets"
#define DUMP_DESCRIPTION                        \
    "Dump the resolver facts and targets.\n"

#define DOT_SYNTAX  "dot"
#define DOT_SUMMARY "dump the resolver facts and targets in DOT format"
#define DOT_DESCRIPTION                        \
    "Dump the resolver facts and targets in DOT format.\n"

MRP_CORE_CONSOLE_GROUP(resolver_group, "resolver", RESOLVER_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("dump", dump, FALSE,
                          DUMP_SYNTAX, DUMP_SUMMARY, DUMP_DESCRIPTION),
        MRP_TOKENIZED_CMD("dot", dot, FALSE,
                          DOT_SYNTAX, DOT_SUMMARY, DOT_DESCRIPTION),
});
