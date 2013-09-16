#include <murphy/core/console.h>
#include <murphy/resolver/resolver.h>

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

#define RESOLVER_DESCRIPTION                                              \
    "Resolver commands provide runtime diagnostics and debugging for\n"   \
    "the Murphy resolver.\n"

#define DUMP_SYNTAX  "dump"
#define DUMP_SUMMARY "dump the resolver facts and targets"
#define DUMP_DESCRIPTION                        \
    "Dump the resolver facts and targets.\n"

MRP_CORE_CONSOLE_GROUP(resolver_group, "resolver", RESOLVER_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("dump", dump, FALSE,
                          DUMP_SYNTAX, DUMP_SUMMARY, DUMP_DESCRIPTION),
});
