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

#include <stdio.h>
#include <errno.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>
#include <murphy/core/method.h>
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
#   define OPTIONS "f:l:t:d:vh"
    struct option options[] = {
        { "file"      , required_argument, NULL, 'f' },
        { "log-level" , required_argument, NULL, 'l' },
        { "log-target", required_argument, NULL, 't' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "debug"     , required_argument, NULL, 'd' },
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

    c.r = mrp_resolver_parse(NULL, NULL, c.file);

    if (c.r == NULL)
        mrp_log_error("Failed to parse input file '%s'.", c.file);
    else {
        mrp_log_info("Input file '%s' parsed successfully.", c.file);
        mrp_resolver_dump_targets(c.r, stdout);
        mrp_resolver_dump_facts(c.r, stdout);

        mrp_resolver_declare_variable(c.r, "var1", MRP_SCRIPT_TYPE_STRING);
        mrp_resolver_declare_variable(c.r, "var2", MRP_SCRIPT_TYPE_STRING);
        mrp_resolver_declare_variable(c.r, "var3", MRP_SCRIPT_TYPE_BOOL);
        mrp_resolver_declare_variable(c.r, "var4", MRP_SCRIPT_TYPE_SINT32);
        mrp_resolver_declare_variable(c.r, "var5", MRP_SCRIPT_TYPE_UINT32);
        mrp_resolver_declare_variable(c.r, "var6", MRP_SCRIPT_TYPE_UNKNOWN);

        for (i = optind; i < argc; i++) {
            target = argv[i];
            printf("========== Target %s ==========\n", target);

            if (mrp_resolver_update_targetl(c.r, argv[i],
                                            "var1", MRP_SCRIPT_STRING("foo"),
                                            "var2", MRP_SCRIPT_STRING("bar"),
                                            "var3", MRP_SCRIPT_BOOL(TRUE),
                                            "var4", MRP_SCRIPT_SINT32(-1),
                                            "var5", MRP_SCRIPT_UINT32(123),
                                            "var6", MRP_SCRIPT_DOUBLE(3.141),
                                            NULL) > 0)
                printf("Resolved OK.\n");
            else
                printf("Resolving FAILED.\n");

#if 0
            {
                int nvariable = 6;
                const char *variables[nvariable];
                mrp_script_value_t values[nvariable];

                variables[0] = "var1";
                values[0]    = MRP_SCRIPT_VALUE_STRING("foo");
                variables[1] = "var2";
                values[1]    = MRP_SCRIPT_VALUE_STRING("bar");
                variables[2] = "var3";
                values[2]    = MRP_SCRIPT_VALUE_BOOL(TRUE);
                variables[3] = "var4";
                values[3]    = MRP_SCRIPT_VALUE_SINT32(-3);
                variables[4] = "var5";
                values[4]    = MRP_SCRIPT_VALUE_UINT32(369);
                variables[5] = "var6";
                values[5]    = MRP_SCRIPT_VALUE_SINT32(-3141);

            if (mrp_resolver_update_targetv(c.r, argv[i], variables, values,
                                            nvariable) > 0)
                printf("Resolved OK.\n");
            else
                printf("Resolving FAILED.\n");
            }
#endif
        }
    }

    mrp_resolver_destroy(c.r);
    c.r = NULL;

    return 0;
}
