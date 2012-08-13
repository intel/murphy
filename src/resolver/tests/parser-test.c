#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>
#include <murphy/resolver/resolver.h>


typedef struct {
    const char     *file;
    mrp_resolver_t *r;
    int             log_mask;
    const char     *log_target;
    int             debug;
} context_t;


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    printf("usage: %s [options] [transport-address]\n\n"
           "The possible options are:\n"
           "  -f, --file                     input file to user\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable given debug confguration\n"
           "  -D, --list-debug               list known debug sites\n"
           "  -h, --help                     show help on usage\n",
           argv0);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void config_set_defaults(context_t *ctx)
{
    mrp_clear(ctx);
    ctx->file = "test-input";
    ctx->log_mask   = MRP_LOG_UPTO(MRP_LOG_DEBUG);
    ctx->log_target = MRP_LOG_TO_STDERR;
}


int parse_cmdline(context_t *ctx, int argc, char **argv)
{
#   define OPTIONS "f:l:t:d:vDh"
    struct option options[] = {
        { "file"      , required_argument, NULL, 'f' },
        { "log-level" , required_argument, NULL, 'l' },
        { "log-target", required_argument, NULL, 't' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "debug"     , required_argument, NULL, 'd' },
        { "list-debug", no_argument      , NULL, 'D' },
        { "help"      , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int  opt;

    config_set_defaults(ctx);

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'f':
            ctx->file = optarg;
            break;

        case 'v':
            ctx->log_mask <<= 1;
            ctx->log_mask  |= 1;
            break;

        case 'l':
            ctx->log_mask = mrp_log_parse_levels(optarg);
            if (ctx->log_mask < 0)
                print_usage(argv[0], EINVAL, "invalid log level '%s'", optarg);
            break;

        case 't':
            ctx->log_target = mrp_log_parse_target(optarg);
            if (!ctx->log_target)
                print_usage(argv[0], EINVAL, "invalid log target '%s'", optarg);
            break;

        case 'd':
            ctx->debug = TRUE;
            mrp_debug_set_config(optarg);
            break;

        case 'D':
            printf("Known debug sites:\n");
            mrp_debug_dump_sites(stdout, 4);
            exit(0);
            break;

        case 'h':
            print_usage(argv[0], -1, "");
            exit(0);
            break;

        default:
            print_usage(argv[0], EINVAL, "invalid option '%c'", opt);
        }
    }

    return TRUE;
}


int main(int argc, char *argv[])
{
    context_t  c;
    int        i;
    char      *target;

    if (!parse_cmdline(&c, argc, argv))
        exit(1);

    mrp_log_set_mask(c.log_mask);
    mrp_log_set_target(c.log_target);

    if (c.debug)
        mrp_debug_enable(TRUE);

    c.r = mrp_resolver_parse(c.file);

    if (c.r == NULL)
        mrp_log_error("Failed to parse input file '%s'.", c.file);
    else {
        mrp_log_info("Input file '%s' parsed successfully.", c.file);
        mrp_resolver_dump_targets(c.r, stdout);
        mrp_resolver_dump_facts(c.r, stdout);
    }

    for (i = optind; i < argc; i++) {
        target = argv[i];
        printf("========== Target %s ==========\n", target);
        if (mrp_resolver_update(c.r, argv[i], NULL))
            printf("Resolved OK.\n");
        else
            printf("Resolving FAILED.\n");
    }

    mrp_resolver_cleanup(c.r);
    c.r = NULL;

    return 0;
}
