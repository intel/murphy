/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common/log.h>
#include <murphy/core/context.h>
#include <murphy/core/plugin.h>
#include <murphy/daemon/config.h>

#ifndef PATH_MAX
#    define PATH_MAX 1024
#endif
#define MAX_ARGS 64

static void valgrind(const char *vg_path, int argc, char **argv, int vg_offs,
                     int saved_argc, char **saved_argv, char **envp);

/*
 * command line processing
 */

static void print_usage(mrp_context_t *ctx, const char *argv0, int exit_code,
                        const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    printf("usage: %s [options] [-V [valgrind-path] [valgrind-options]]\n\n"
           "The possible options are:\n"
           "  -c, --config-file=PATH         main configuration file to use\n"
           "      The default configuration file is '%s'.\n"
           "  -C, --config-dir=PATH          configuration directory to use\n"
           "      If omitted, defaults to '%s'.\n"
           "  -P, --plugin-dir=PATH          load plugins from DIR\n"
           "      The default plugin directory is '%s'.\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable given debug configuration\n"
           "  -D, --list-debug               list known debug sites\n"
           "  -f, --foreground               don't daemonize\n"
           "  -h, --help                     show help on usage\n"
           "  -q, --query-plugins            show detailed information about\n"
           "                                 all the available plugins\n"
           "  -B, --blacklist-plugins <list> disable list of plugins\n"
           "  -I, --blacklist-builtin <list> disable list of builtin plugins\n"
           "  -E, --blacklist-dynamic <list> disable list of dynamic plugins\n"
           "  -w, --whitelist-plugins <list> disable list of plugins\n"
           "  -i, --whitelist-builtin <list> disable list of builtin plugins\n"
           "  -e, --whitelist-dynamic <list> disable list of dynamic plugins\n"
           "  -R, --no-poststart-load        "
                    "disable post-startup plugin loading\n"
           "  -p, --disable-console          disable Murphy debug console\n"
           "  -V, --valgrind                 run through valgrind\n",
           argv0, ctx->config_file, ctx->config_dir, ctx->plugin_dir);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void print_plugin_help(mrp_context_t *ctx, int detailed)
{
#define PRNT(fmt, arg) snprintf(defval, sizeof(defval), fmt, arg)

    mrp_plugin_t       *plugin;
    mrp_plugin_descr_t *descr;
    mrp_plugin_arg_t   *arg;
    mrp_list_hook_t    *p, *n;
    char               *type, defval[64];
    int                 i;

    mrp_load_all_plugins(ctx);

    printf("\nAvailable plugins:\n\n");

    mrp_list_foreach(&ctx->plugins, p, n) {
        plugin = mrp_list_entry(p, typeof(*plugin), hook);
        descr  = plugin->descriptor;

        printf("- %splugin %s:", plugin->handle ? "" : "Builtin ", descr->name);
        if (detailed) {
            printf(" (%s, version %d.%d.%d)\n", plugin->path,
                   MRP_VERSION_MAJOR(descr->version),
                   MRP_VERSION_MINOR(descr->version),
                   MRP_VERSION_MICRO(descr->version));
            printf("  Authors: %s\n", descr->authors);
        }
        else
            printf("\n");

        if (detailed)
            printf("  Description:\n    %s\n", descr->description);

        if (descr->args != NULL) {
            printf("  Arguments:\n");

            for (i = 0, arg = descr->args; i < descr->narg; i++, arg++) {
                printf("    %s: ", arg->key);
                switch (arg->type) {
                case MRP_PLUGIN_ARG_TYPE_STRING:
                    type = "string";
                    PRNT("%s", arg->str ? arg->str : "<none>");
                    break;
                case MRP_PLUGIN_ARG_TYPE_BOOL:
                    type = "boolean";
                    PRNT("%s", arg->bln ? "TRUE" : "FALSE");
                    break;
                case MRP_PLUGIN_ARG_TYPE_UINT32:
                    type = "unsigned 32-bit integer";
                    PRNT("%u", arg->u32);
                    break;
                case MRP_PLUGIN_ARG_TYPE_INT32:
                    type = "signed 32-bit integer";
                    PRNT("%d", arg->i32);
                    break;
                case MRP_PLUGIN_ARG_TYPE_DOUBLE:
                    type = "double-precision floating point";
                    PRNT("%f", arg->dbl);
                    break;
                default:
                    type = "<unknown argument type>";
                    PRNT("%s", "<unknown>");
                }

                printf("%s, default value=%s\n", type, defval);
            }
        }

        if (descr->help != NULL && descr->help[0])
            printf("  Help:\n    %s\n", descr->help);

        printf("\n");
    }

    printf("\n");

#if 0
    printf("Note that you can disable any plugin from the command line by\n");
    printf("using the '-a name:%s' option.\n", MURPHY_PLUGIN_ARG_DISABLED);
#endif
}


static void config_set_defaults(mrp_context_t *ctx, char *argv0)
{
    static char cfg_file[PATH_MAX], cfg_dir[PATH_MAX], plugin_dir[PATH_MAX];
    char *e;
    int   l;

    if ((e = strstr(argv0, "/src/murphyd")) != NULL ||
        (e = strstr(argv0, "/src/.libs/lt-murphyd")) != NULL) {
        mrp_log_mask_t saved = mrp_log_set_mask(MRP_LOG_MASK_WARNING);
        mrp_log_warning("***");
        mrp_log_warning("*** Looks like we are run from the source tree.");
        mrp_log_warning("*** Runtime defaults will be set accordingly...");
        mrp_log_warning("***");
        mrp_log_set_mask(saved);

        l = e - argv0;
        snprintf(cfg_dir, sizeof(cfg_dir), "%*.*s/src/daemon", l, l, argv0);
        snprintf(cfg_file, sizeof(cfg_file), "%s/murphy-lua.conf", cfg_dir);
        snprintf(plugin_dir, sizeof(plugin_dir), "%*.*s/src/.libs",
                 l, l, argv0);

        ctx->config_file = cfg_file;
        ctx->config_dir  = cfg_dir;
        ctx->plugin_dir  = plugin_dir;
        ctx->log_mask    = MRP_LOG_UPTO(MRP_LOG_INFO);
        ctx->log_target  = MRP_LOG_TO_STDERR;
        ctx->foreground  = TRUE;
    }
    else {
        ctx->config_file = MRP_DEFAULT_CONFIG_FILE;
        ctx->config_dir  = MRP_DEFAULT_CONFIG_DIR;
        ctx->plugin_dir  = MRP_DEFAULT_PLUGIN_DIR;
        ctx->log_mask    = MRP_LOG_MASK_ERROR;
        ctx->log_target  = MRP_LOG_TO_STDERR;
    }
}


void mrp_parse_cmdline(mrp_context_t *ctx, int argc, char **argv, char **envp)
{
#   define OPTIONS "c:C:l:t:fP:a:vd:hHqB:I:E:w:i:e:RpV"
    struct option options[] = {
        { "config-file"      , required_argument, NULL, 'c' },
        { "config-dir"       , required_argument, NULL, 'C' },
        { "plugin-dir"       , required_argument, NULL, 'P' },
        { "log-level"        , required_argument, NULL, 'l' },
        { "log-target"       , required_argument, NULL, 't' },
        { "verbose"          , optional_argument, NULL, 'v' },
        { "debug"            , required_argument, NULL, 'd' },
        { "foreground"       , no_argument      , NULL, 'f' },
        { "help"             , no_argument      , NULL, 'h' },
        { "more-help"        , no_argument      , NULL, 'H' },
        { "query-plugins"    , no_argument      , NULL, 'q' },
        { "blacklist"        , required_argument, NULL, 'B' },
        { "blacklist-plugins", required_argument, NULL, 'B' },
        { "blacklist-builtin", required_argument, NULL, 'I' },
        { "blacklist-dynamic", required_argument, NULL, 'E' },
        { "whitelist"        , required_argument, NULL, 'w' },
        { "whitelist-plugins", required_argument, NULL, 'w' },
        { "whitelist-builtin", required_argument, NULL, 'i' },
        { "whitelist-dynamic", required_argument, NULL, 'e' },
        { "no-poststart-load", no_argument      , NULL, 'R' },
        { "disable-console"  , no_argument      , NULL, 'p' },
        { "valgrind"         , optional_argument, NULL, 'V' },
        { NULL, 0, NULL, 0 }
    };

#   define SAVE_ARG(a) do {                                     \
        if (saved_argc >= MAX_ARGS)                             \
            print_usage(ctx, argv[0], EINVAL,                   \
                        "too many command line arguments");     \
        else                                                    \
            saved_argv[saved_argc++] = a;                       \
    } while (0)
#   define SAVE_OPT(o)       SAVE_ARG(o)
#   define SAVE_OPTARG(o, a) SAVE_ARG(o); SAVE_ARG(a)
    char *saved_argv[MAX_ARGS];
    int   saved_argc;

    int opt, help;

    config_set_defaults(ctx, argv[0]);
    mrp_log_set_mask(ctx->log_mask);
    mrp_log_set_target(ctx->log_target);

    saved_argc = 0;
    saved_argv[saved_argc++] = argv[0];

    help = FALSE;

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'c':
            SAVE_OPTARG("-c", optarg);
            ctx->config_file = optarg;
            break;

        case 'C':
            SAVE_OPTARG("-C", optarg);
            ctx->config_dir = optarg;
            break;

        case 'P':
            SAVE_OPTARG("-P", optarg);
            ctx->plugin_dir = optarg;
            break;

        case 'v':
            SAVE_OPT("-v");
            ctx->log_mask <<= 1;
            ctx->log_mask  |= 1;
            mrp_log_set_mask(ctx->log_mask);
            break;

        case 'l':
            SAVE_OPTARG("-l", optarg);
            ctx->log_mask = mrp_log_parse_levels(optarg);
            if (ctx->log_mask < 0)
                print_usage(ctx, argv[0], EINVAL,
                            "invalid log level '%s'", optarg);
            else
                mrp_log_set_mask(ctx->log_mask);
            break;

        case 't':
            SAVE_OPTARG("-t", optarg);
            ctx->log_target = optarg;
            break;

        case 'd':
            SAVE_OPTARG("-d", optarg);
            ctx->log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_debug_set_config(optarg);
            mrp_debug_enable(TRUE);
            break;

        case 'f':
            SAVE_OPT("-f");
            ctx->foreground = TRUE;
            break;

        case 'h':
            SAVE_OPT("-h");
            help++;
            break;

        case 'H':
            SAVE_OPT("-H");
            help += 2;
            break;

        case 'q':
            SAVE_OPT("-q");
            print_plugin_help(ctx, TRUE);
            break;

        case 'B':
            if (ctx->blacklist_plugins != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "blacklist option given multiple times");
            SAVE_OPTARG("-B", optarg);
            ctx->blacklist_plugins = optarg;
            break;
        case 'I':
            if (ctx->blacklist_builtin != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "builtin blacklist option given multiple times");
            SAVE_OPTARG("-I", optarg);
            ctx->blacklist_builtin = optarg;
            break;
        case 'E':
            if (ctx->blacklist_dynamic != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "dynamic blacklist option given multiple times");
            SAVE_OPTARG("-E", optarg);
            ctx->blacklist_dynamic = optarg;
            break;
        case 'w':
            if (ctx->whitelist_plugins != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "whitelist option given multiple times");
            SAVE_OPTARG("-w", optarg);
            ctx->whitelist_plugins = optarg;
            break;
        case 'i':
            if (ctx->whitelist_builtin != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "builtin whitelist option given multiple times");
            SAVE_OPTARG("-i", optarg);
            ctx->whitelist_builtin = optarg;
            break;
        case 'e':
            if (ctx->whitelist_dynamic != NULL)
                print_usage(ctx, argv[0], EINVAL,
                            "dynamic whitelist option given multiple times");
            SAVE_OPTARG("-e", optarg);
            ctx->whitelist_dynamic = optarg;
            break;

        case 'R':
            SAVE_OPT("-R");
            ctx->disable_runtime_load = true;
            break;

        case 'p':
            SAVE_OPT("-p");
            ctx->disable_console = TRUE;
            break;
        case 'V':
            valgrind(optarg, argc, argv, optind, saved_argc, saved_argv, envp);
            break;

        default:
            print_usage(ctx, argv[0], EINVAL, "invalid option '%c'", opt);
        }
    }

    if (help) {
        print_usage(ctx, argv[0], -1, "");
        if (help > 1)
            print_plugin_help(ctx, FALSE);
        exit(0);
    }

}



/*
 * configuration file processing
 */

typedef struct {
    char  buf[MRP_CFG_MAXLINE];          /* input buffer */
    char *token;                         /* current token */
    char *in;                            /* filling pointer */
    char *out;                           /* consuming pointer */
    char *next;                          /* next token buffer position */
    int   fd;                            /* input file */
    int   error;                         /* whether has encounted and error */
    char *file;                          /* file being processed */
    int   line;                          /* line number */
    int   next_newline;
    int   was_newline;
} input_t;


#define COMMON_ACTION_FIELDS                                                \
    action_type_t   type;                /* action to execute */            \
    mrp_list_hook_t hook                 /* to command sequence */

typedef enum {                           /* action types */
    ACTION_UNKNOWN = 0,
    ACTION_LOAD,                         /* load a plugin */
    ACTION_TRYLOAD,                      /* load a plugin, ignore errors */
    ACTION_IF,                           /* if-else branch */
    ACTION_SETCFG,                       /* set a config variable */
    ACTION_INFO,                         /* emit an info message */
    ACTION_WARNING,                      /* emit a warning message */
    ACTION_ERROR,                        /* emit and error message and exit */
    ACTION_MAX,
} action_type_t;

typedef enum {                           /* branch operators */
    BR_UNKNOWN = 0,
    BR_PLUGIN_EXISTS,                    /* test if a plugin exists */
} branch_op_t;

typedef struct {                         /* a generic action */
    COMMON_ACTION_FIELDS;                /* type, hook */
} any_action_t;

typedef struct {                         /* a command-type of action */
    COMMON_ACTION_FIELDS;                /* type, hook */
    char **args;                         /* arguments for the action */
    int    narg;                         /* number of arguments */
} cmd_action_t;

typedef struct {                         /* a command-type of action */
    COMMON_ACTION_FIELDS;                /* type, hook */
    char             *name;              /* plugin to load */
    char             *instance;          /* load as this instance */
    mrp_plugin_arg_t *args;              /* plugin arguments */
    int               narg;              /* number of arguments */
} load_action_t;

typedef struct {                         /* a branch test action */
    COMMON_ACTION_FIELDS;                /* type, hook */
    branch_op_t      op;                 /* branch operator */
    char            *arg1;               /* argument for the operator */
    char            *arg2;               /* argument for the operator */
    mrp_list_hook_t  pos;                /* postitive branch */
    mrp_list_hook_t  neg;                /* negative branch */
} branch_action_t;

typedef struct {                         /* a branch test action */
    COMMON_ACTION_FIELDS;                /* type, hook */
    char            *message;            /* message to show */
} message_action_t;

typedef enum {
    CFGVAR_UNKNOWN = 0,
    CFGVAR_RESOLVER_RULES,               /* resolver ruleset file */
} cfgvar_t;

typedef struct {
    COMMON_ACTION_FIELDS;
    cfgvar_t  id;                        /* confguration variable */
    char     *value;                     /* value for variable */
} setcfg_action_t;

typedef struct {
    const char   *keyword;
    any_action_t *(*parse)(input_t *in, char **args, int narg);
    int           (*exec)(mrp_context_t *ctx, any_action_t *action);
    void          (*free)(any_action_t *a);
} action_descr_t;



static any_action_t *parse_action(input_t *in, char **args, int narg);
static any_action_t *parse_load(input_t *in, char **argv, int argc);
static any_action_t *parse_if_else(input_t *in, char **argv, int argc);
static any_action_t *parse_setcfg(input_t *in, char **argv, int argc);
static any_action_t *parse_message(input_t *in, char **argv, int argc);
static int exec_action(mrp_context_t *ctx, any_action_t *action);
static int exec_load(mrp_context_t *ctx, any_action_t *action);
static int exec_if_else(mrp_context_t *ctx, any_action_t *action);
static int exec_setcfg(mrp_context_t *ctx, any_action_t *action);
static int exec_message(mrp_context_t *ctx, any_action_t *action);
static void free_action(any_action_t *action);
static void free_if_else(any_action_t *action);
static void free_load(any_action_t *action);
static void free_setcfg(any_action_t *action);
static void free_message(any_action_t *action);

static char *get_next_token(input_t *in);
static int get_next_line(input_t *in, char **args, size_t size);
static char *replace_tokens(input_t *in, char *first, char *last,
                            char *token, int size);

#define A(type, keyword, parse, exec, free) \
    [ACTION_##type] = { MRP_KEYWORD_##keyword, parse, exec, free }

static action_descr_t actions[] = {
    [ACTION_UNKNOWN] = { NULL, NULL, NULL, NULL },

    A(LOAD   , LOAD   , parse_load   , exec_load   , free_load),
    A(TRYLOAD, TRYLOAD, parse_load   , exec_load   , free_load),
    A(IF     , IF     , parse_if_else, exec_if_else, free_if_else),
    A(SETCFG , SETCFG , parse_setcfg , exec_setcfg , free_setcfg),
    A(INFO   , INFO   , parse_message, exec_message, free_message),
    A(WARNING, WARNING, parse_message, exec_message, free_message),
    A(ERROR  , ERROR  , parse_message, exec_message, free_message),

    [ACTION_MAX]     = { NULL, NULL, NULL, NULL }
};

#undef A



mrp_cfgfile_t *mrp_parse_cfgfile(const char *path)
{
    mrp_cfgfile_t *cfg = NULL;
    input_t        input;
    char          *args[MRP_CFG_MAXARGS];
    int            narg;
    any_action_t  *a;

    memset(&input, 0, sizeof(input));
    input.token  = input.buf;
    input.in     = input.buf;
    input.out    = input.buf;
    input.next   = input.buf;
    input.fd     = open(path, O_RDONLY);
    input.file   = (char *)path;
    input.line   = 1;

    if (input.fd < 0) {
        mrp_log_error("Failed to open configuration file '%s' (%d: %s).",
                      path, errno, strerror(errno));
        goto fail;
    }

    cfg = mrp_allocz(sizeof(*cfg));

    if (cfg == NULL) {
        mrp_log_error("Failed to allocate configuration file buffer.");
        goto fail;
    }

    mrp_list_init(&cfg->actions);

    while ((narg = get_next_line(&input, args, MRP_ARRAY_SIZE(args))) > 0) {
        a = parse_action(&input, args, narg);

        if (a != NULL)
            mrp_list_append(&cfg->actions, &a->hook);
        else
            goto fail;
    }

    if (narg == 0)
        return cfg;

 fail:
    if (input.fd >= 0)
        close(input.fd);
    if (cfg)
        mrp_free_cfgfile(cfg);

    return NULL;
}


void mrp_free_cfgfile(mrp_cfgfile_t *cfg)
{
    mrp_list_hook_t *p, *n;
    any_action_t    *a;

    mrp_list_foreach(&cfg->actions, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);
        free_action(a);
    }

    mrp_free(cfg);
}


int mrp_exec_cfgfile(mrp_context_t *ctx, mrp_cfgfile_t *cfg)
{
    mrp_list_hook_t *p, *n;
    any_action_t    *a;

    mrp_list_foreach(&cfg->actions, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);
        if (!exec_action(ctx, a))
            return FALSE;
    }

    return TRUE;
}


static any_action_t *parse_action(input_t *in, char **args, int narg)
{
    action_descr_t *ad = actions + 1;

    while (ad->keyword != NULL) {
        if (!strcmp(args[0], ad->keyword))
            return ad->parse(in, args, narg);
        ad++;
    }

    mrp_log_error("Unknown command '%s' in file '%s'.", args[0], in->file);
    return NULL;
}


static void free_action(any_action_t *action)
{
    mrp_list_delete(&action->hook);

    if (ACTION_UNKNOWN < action->type && action->type < ACTION_MAX)
        actions[action->type].free(action);
    else {
        mrp_log_error("Unknown configuration action of type 0x%x.",
                      action->type);
        mrp_free(action);
    }
}


static int exec_action(mrp_context_t *ctx, any_action_t *action)
{
    if (ACTION_UNKNOWN < action->type && action->type < ACTION_MAX)
        return actions[action->type].exec(ctx, action);
    else {
        mrp_log_error("Unknown configuration action of type 0x%x.",
                      action->type);
        return FALSE;
    }
}


static any_action_t *parse_load(input_t *in, char **argv, int argc)
{
    load_action_t    *action;
    action_type_t     type;
    mrp_plugin_arg_t *args, *a;
    int               narg, i, start;
    char             *k, *v;

    MRP_UNUSED(in);

    if (!strcmp(argv[0], MRP_KEYWORD_LOAD))
        type = ACTION_LOAD;
    else
        type = ACTION_TRYLOAD;

    if (argc < 2 || (action = mrp_allocz(sizeof(*action))) == NULL) {
        mrp_log_error("Failed to allocate load config action.");
        return NULL;
    }

    mrp_list_init(&action->hook);
    action->type = type;
    action->name = mrp_strdup(argv[1]);

    if (action->name == NULL) {
        mrp_log_error("Failed to allocate load config action.");
        mrp_free(action);
        return NULL;
    }

    args = NULL;

    if (argc > 3 && !strcmp(argv[2], MRP_KEYWORD_AS)) {
        /* [try-]load-plugin name as instance [args...] */
        action->instance = mrp_strdup(argv[3]);
        start = 4;

        if (action->instance == NULL) {
            mrp_log_error("Failed to allocate load config action.");
            mrp_free(action->name);
            mrp_free(action);
            goto fail;
        }
    }
    else {
        /* [try-]load-plugin name [args...] */
        start = 2;
    }

    narg = 0;
    if (start < argc) {
        if ((args = mrp_allocz_array(typeof(*args), argc - 1)) != NULL) {
            for (i = start, a = args; i < argc; i++, a++) {
                if (*argv[i] == MRP_START_COMMENT)
                    break;

                mrp_debug("argument #%d: '%s'", i - start, argv[i]);

                k = argv[i];
                v = strchr(k, '=');

                if (v != NULL)
                    *v++ = '\0';
                else {
                    if (i + 2 < argc) {
                        if (argv[i+1][0] == '=' && argv[i+1][1] == '\0') {
                            v  = argv[i + 2];
                            i += 2;
                        }
                    }
                    else {
                        mrp_log_error("Invalid plugin load argument '%s'.", k);
                        goto fail;
                    }
                }

                a->type = MRP_PLUGIN_ARG_TYPE_STRING;
                a->key  = mrp_strdup(k);
                a->str  = v ? mrp_strdup(v) : NULL;
                narg++;

                if (a->key == NULL || (a->str == NULL && v != NULL)) {
                    mrp_log_error("Failed to allocate plugin arg %s%s%s.",
                                  k, v ? "=" : "", v ? v : "");
                    goto fail;
                }
            }
        }
    }

    action->args = args;
    action->narg = narg;

    return (any_action_t *)action;


 fail:
    if (args != NULL) {
        for (i = 1; i < argc && args[i].key != NULL; i++) {
            mrp_free(args[i].key);
            mrp_free(args[i].str);
        }
        mrp_free(args);
    }

    return NULL;
}


static void free_load(any_action_t *action)
{
    load_action_t *load = (load_action_t *)action;
    int            i;

    if (load != NULL) {
        mrp_free(load->name);

        for (i = 0; i < load->narg; i++) {
            mrp_free(load->args[i].key);
            mrp_free(load->args[i].str);
        }

        mrp_free(load->args);
    }
}


static int exec_load(mrp_context_t *ctx, any_action_t *action)
{
    load_action_t *load = (load_action_t *)action;
    mrp_plugin_t  *plugin;

    plugin = mrp_load_plugin(ctx, load->name, load->instance,
                             load->args, load->narg);

    if (plugin != NULL) {
        plugin->may_fail = (load->type == ACTION_TRYLOAD);

        return TRUE;
    }
    else
        return (load->type == ACTION_TRYLOAD);
}


static any_action_t *parse_if_else(input_t *in, char **argv, int argc)
{
    branch_action_t *branch;
    mrp_list_hook_t *actions;
    any_action_t    *a;
    char            *args[MRP_CFG_MAXARGS], *op, *name;
    int              start, narg, pos;

    if (argc < 2) {
        mrp_log_error("%s:%d: invalid use of if-conditional.",
                      in->file, in->line - 1);
        return NULL;
    }

    start = in->line - 1;
    op    = argv[1];
    name  = argv[2];

    if (strcmp(op, MRP_KEYWORD_EXISTS)) {
        mrp_log_error("%s:%d: unknown operator '%s' in if-conditional.",
                      in->file, in->line - 1, op);
    }

    branch = mrp_allocz(sizeof(*branch));

    if (branch != NULL) {
        mrp_list_init(&branch->hook);
        mrp_list_init(&branch->pos);
        mrp_list_init(&branch->neg);

        branch->type = ACTION_IF;
        branch->op   = BR_PLUGIN_EXISTS;
        branch->arg1 = mrp_strdup(name);

        if (branch->arg1 == NULL) {
            mrp_log_error("Failed to allocate configuration if-conditional.");
            goto fail;
        }

        pos     = TRUE;
        actions = &branch->pos;
        while ((narg = get_next_line(in, args, sizeof(args))) > 0) {
            if (narg == 1) {
                if (!strcmp(args[0], MRP_KEYWORD_END))
                    return (any_action_t *)branch;

                if (!strcmp(args[0], MRP_KEYWORD_ELSE)) {
                    if (pos) {
                        actions = &branch->neg;
                        pos = FALSE;
                    }
                    else {
                        mrp_log_error("%s:%d: extra else without if.",
                                      in->file, in->line - 1);
                        goto fail;
                    }
                }
            }
            else {
                a = parse_action(in, args, narg);

                if (a != NULL)
                    mrp_list_append(actions, &a->hook);
                else
                    goto fail;
            }
        }
    }
    else {
        mrp_log_error("Failed to allocate configuration if-conditional.");
        return NULL;
    }

    mrp_log_error("%s:%d: unterminated if-conditional (missing 'end')",
                  in->file, start);

 fail:
    free_action((any_action_t *)branch);
    return NULL;
}


static void free_if_else(any_action_t *action)
{
    branch_action_t *branch = (branch_action_t *)action;
    any_action_t    *a;
    mrp_list_hook_t *p, *n;

    if (branch != NULL) {
        mrp_free(branch->arg1);
        mrp_free(branch->arg2);

        mrp_list_foreach(&branch->pos, p, n) {
            a = mrp_list_entry(p, typeof(*a), hook);
            free_action(a);
        }

        mrp_list_foreach(&branch->neg, p, n) {
            a = mrp_list_entry(p, typeof(*a), hook);
            free_action(a);
        }

        mrp_free(branch);
    }
}


static int exec_if_else(mrp_context_t *ctx, any_action_t *action)
{
    branch_action_t *branch = (branch_action_t *)action;
    mrp_list_hook_t *p, *n, *actions;
    any_action_t    *a;

    if (branch->op != BR_PLUGIN_EXISTS || branch->arg1 == NULL)
        return FALSE;

    if (mrp_plugin_exists(ctx, branch->arg1))
        actions = &branch->pos;
    else
        actions = &branch->neg;

    mrp_list_foreach(actions, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);

        if (!exec_action(ctx, a))
            return FALSE;
    }

    return TRUE;
}


static any_action_t *parse_setcfg(input_t *in, char **argv, int argc)
{
    setcfg_action_t *action;
    struct {
        const char *name;
        cfgvar_t    id;
    } *var, vartbl[] = {
        { MRP_CFGVAR_RESOLVER, CFGVAR_RESOLVER_RULES },
        { NULL               , 0                     },
    };

    if (argc < 3) {
        mrp_log_error("%s:%d: configuration directive %s requires two "
                      "arguments, %d given.", in->file, in->line,
                      MRP_KEYWORD_SETCFG, argc - 1);
        return NULL;
    }

    for (var = vartbl; var->name != NULL; var++)
        if (!strcmp(var->name, argv[1]))
            break;

    if (var->name == NULL) {
        mrp_log_error("%s:%d: unknown configuration variable '%s'.",
                      in->file, in->line, argv[1]);
        return NULL;
    }

    if ((action = mrp_allocz(sizeof(*action))) == NULL) {
        mrp_log_error("Failed to allocate %s %s configuration action.",
                      MRP_KEYWORD_SETCFG, argv[1]);
        return NULL;
    }

    mrp_list_init(&action->hook);
    action->type  = ACTION_SETCFG;
    action->id    = var->id;
    action->value = mrp_strdup(argv[2]);

    if (action->value == NULL) {
        mrp_log_error("Failed to allocate %s %s configuration action.",
                      MRP_KEYWORD_SETCFG, argv[1]);
        mrp_free(action);
        return NULL;
    }

    return (any_action_t *)action;
}


static int exec_setcfg(mrp_context_t *ctx, any_action_t *action)
{
    setcfg_action_t *setcfg = (setcfg_action_t *)action;

    switch (setcfg->id) {
    case CFGVAR_RESOLVER_RULES:
        if (ctx->resolver_ruleset == NULL) {
            ctx->resolver_ruleset = setcfg->value;
            setcfg->value = NULL;
            return TRUE;
        }
        else {
            mrp_log_error("Multiple resolver rulesets specified (%s, %s).",
                          ctx->resolver_ruleset, setcfg->value);
            return FALSE;
        }
        break;
    default:
        mrp_log_error("Invalid configuration setting.");
    }

    return FALSE;
}


static void free_setcfg(any_action_t *action)
{
    setcfg_action_t *setcfg = (setcfg_action_t *)action;

    if (setcfg != NULL) {
        mrp_free(setcfg->value);
        mrp_free(setcfg);
    }
}


static any_action_t *parse_message(input_t *in, char **argv, int argc)
{
    message_action_t *msg;
    action_type_t     type;
    char              buf[4096], *p;
    const char       *t;
    int               i, l, n;

    MRP_UNUSED(in);

    if (argc < 2) {
        mrp_log_error("%s requires at least one argument.", argv[0]);
        return NULL;
    }

    if (!strcmp(argv[0], MRP_KEYWORD_ERROR))
        type = ACTION_ERROR;
    else if (!strcmp(argv[0], MRP_KEYWORD_WARNING))
        type = ACTION_WARNING;
    else if (!strcmp(argv[0], MRP_KEYWORD_INFO))
        type = ACTION_INFO;
    else
        return NULL;

    p = buf;
    n = sizeof(buf);
    if ((msg = mrp_allocz(sizeof(*msg))) != NULL) {
        for (i = 1, t=""; i < argc && n > 0; i++, t=" ") {
            l  = snprintf(p, n, "%s%s", t, argv[i]);
            p += l;
            n -= l;
        }

        msg->type    = type;
        msg->message = mrp_strdup(buf);

        if (msg->message == NULL) {
            mrp_log_error("Failed to allocate %s config action.", argv[0]);
            mrp_free(msg);
            msg = NULL;
        }
    }

    return (any_action_t *)msg;
}


static int exec_message(mrp_context_t *ctx, any_action_t *action)
{
    message_action_t *msg = (message_action_t *)action;

    MRP_UNUSED(ctx);

    switch (action->type) {
    case ACTION_ERROR:   mrp_log_error("%s", msg->message);   exit(1);
    case ACTION_WARNING: mrp_log_warning("%s", msg->message); return TRUE;
    case ACTION_INFO:    mrp_log_info("%s", msg->message);    return TRUE;
    default:
        return FALSE;
    }
}


static void free_message(any_action_t *action)
{
    message_action_t *msg = (message_action_t *)action;

    if (msg != NULL) {
        mrp_free(msg->message);
        mrp_free(msg);
    }
}


static int get_next_line(input_t *in, char **args, size_t size)
{
#define BLOCK_START(s)                                                   \
    ((s[0] == '{' || s[0] == '[') && (s[1] == '\0' || s[1] == '\n'))
#define BLOCK_END(s)                                                     \
    ((s[0] == '}' || s[0] == ']') && (s[1] == '\0' || s[1] == '\n'))

    char *token, *p;
    char  block[2], json[MRP_CFG_MAXLINE];
    int   narg, nest, beg;
    int   i, n, l, tot;

    narg     = 0;
    nest     = 0;
    beg      = -1;
    tot      = 0;
    block[0] = block[1] = '\0';
    while ((token = get_next_token(in)) != NULL && narg < (int)size) {
        if (in->error)
            return -1;

        mrp_debug("read input token '%s'", token);

        if (token[0] != '\n') {
            if (BLOCK_START(token)) {
                if (!nest) {
                    mrp_debug("collecting JSON argument");

                    block[0] = token[0];
                    block[1] = (block[0] == '{' ? '}' : ']');
                    nest = 1;
                    beg  = narg;
                    tot  = 1;
                }
                else {
                    if (token[0] == block[0])
                        nest++;
                }
            }

            args[narg++] = token;

            if (beg >= 0) {              /* if collecting, update length */
                tot += strlen(token) + 1;
                if (strchr(token, ' ') || strchr(token, '\t'))
                    tot += 2;            /* will need quoting */
            }

            if (BLOCK_END(token) && nest > 0) {
                if (token[0] == block[1])
                    nest--;

                if (nest == 0) {
                    mrp_debug("finished collecting JSON argument");

                    if (tot > (int)sizeof(json) - 1) {
                        mrp_log_error("Maximum token length exceeded.");
                        return -1;
                    }

                    p = json;
                    l = tot;
                    for (i = beg; i < narg; i++) {
                        if (strchr(args[i], ' ') || strchr(args[i], '\t'))
                            n = snprintf(p, l, "'%s'", args[i]);
                        else
                            n = snprintf(p, l, "%s", args[i]);
                        if (n >= l)
                            return -1;
                        p += n;
                        l -= n;
                    }

                    mrp_debug("collected JSON token: '%s'", json);

                    args[beg] = replace_tokens(in, args[beg], args[narg-1],
                                               json, (int)(p - json));

                    if (args[beg] == NULL) {
                        mrp_log_error("Failed to replace block of tokens.");
                        return -1;
                    }
                    else
                        narg = beg + 1;

                    block[0] = '\0';
                    block[1] = '\0';
                    beg      = -1;
                }
            }
        }
        else {
            if (narg && *args[0] != MRP_START_COMMENT && *args[0] != '\n')
                return narg;
            else
                narg = 0;
        }
    }

    if (in->error)
        return -1;

    if (narg >= (int)size) {
        mrp_log_error("Too many tokens on line %d of %s.",
                      in->line - 1, in->file);
        return -1;
    }
    else {
        if (*args[0] != MRP_START_COMMENT && *args[0] != '\n')
            return narg;
        else
            return 0;
    }
}


static inline void skip_whitespace(input_t *in)
{
    while ((*in->out == ' ' || *in->out == '\t') && in->out < in->in)
        in->out++;
}


static inline void skip_rest_of_line(input_t *in)
{
    while (*in->out != '\n' && in->out < in->in)
        in->out++;
}


static char *replace_tokens(input_t *in, char *first, char *last,
                            char *token, int size)
{
    char *beg = first;
    char *end = last + strlen(last) + 1;

    if (!(in->buf < beg && end < in->out))
        return NULL;

    if ((end - beg) < size)
        return NULL;

    strcpy(first, token);

    return first;
}


static char *get_next_token(input_t *in)
{
    ssize_t len;
    int     diff, size;
    int     quote, quote_line;
    char   *p, *q;

    /*
     * Newline:
     *
     *     If the previous token was terminated by a newline,
     *     take care of properly returning and administering
     *     the newline token here.
     */

    if (in->next_newline) {
        in->next_newline = FALSE;
        in->was_newline  = TRUE;
        in->line++;

        return "\n";
    }


    /*
     * if we just finished a line, discard all old data/tokens
     */

    if (*in->token == '\n' || in->was_newline) {
        diff = in->out - in->buf;
        size = in->in - in->out;
        memmove(in->buf, in->out, size);
        in->out  -= diff;
        in->in   -= diff;
        in->next  = in->buf;
        *in->in   = '\0';
    }

    /*
     * refill the buffer if we're empty or just flushed all tokens
     */

    if (in->token == in->buf && in->fd != -1) {
        size = sizeof(in->buf) - 1 - (in->in - in->buf);
        len  = read(in->fd, in->in, size);

        if (len < size) {
            close(in->fd);
            in->fd = -1;
        }

        if (len < 0) {
            mrp_log_error("Failed to read from config file (%d: %s).",
                          errno, strerror(errno));
            in->error = TRUE;
            close(in->fd);
            in->fd = -1;

            return NULL;
        }

        in->in += len;
        *in->in = '\0';
    }

    if (in->out >= in->in)
        return NULL;

    skip_whitespace(in);

    quote = FALSE;
    quote_line = 0;

    p = in->out;
    q = in->next;
    in->token = q;

    while (p < in->in) {
        /*printf("[%c]\n", *p == '\n' ? '.' : *p);*/
        switch (*p) {
            /*
             * Quoting:
             *
             *     If we're not within a quote, mark a quote started.
             *     Otherwise if quote matches, close quoting. Otherwise
             *     copy the quoted quote verbatim.
             */
        case '\'':
        case '\"':
            in->was_newline = FALSE;
            if (!quote) {
                quote      = *p++;
                quote_line = in->line;
            }
            else {
                if (*p == quote) {
                    quote      = FALSE;
                    quote_line = 0;
                    p++;
                    *q++ = '\0';

                    in->out  = p;
                    in->next = q;

                    return in->token;
                }
                else {
                    *q++ = *p++;
                }
            }
            break;

            /*
             * Whitespace:
             *
             *     If we're quoting, copy verbatim. Otherwise mark the end
             *     of the token.
             */
        case ' ':
        case '\t':
            if (quote)
                *q++ = *p++;
            else {
                p++;
                *q++ = '\0';

                in->out  = p;
                in->next = q;

                return in->token;
            }
            in->was_newline = FALSE;
            break;

            /*
             * Escaping:
             *
             *     If the last character in the input, copy verbatim.
             *     Otherwise if it escapes a '\n', skip both. Otherwise
             *     copy the escaped character verbatim.
             */
        case '\\':
            if (p < in->in - 1) {
                p++;
                if (*p != '\n')
                    *q++ = *p++;
                else {
                    p++;
                    in->line++;
                    in->out = p;
                    skip_whitespace(in);
                    p = in->out;
                }
            }
            else
                *q++ = *p++;
            in->was_newline = FALSE;
            break;

            /*
             * Newline:
             *
             *     We don't allow newlines to be quoted. Otherwise
             *     if the token is not the newline itself, we mark
             *     the next token to be newline and return the token
             *     it terminated.
             */
        case '\n':
            if (quote) {
                mrp_log_error("%s:%d: Unterminated quote (%c) started "
                              "on line %d.", in->file, in->line, quote,
                              quote_line);
                in->error = TRUE;

                return NULL;
            }
            else {
                *q = '\0';
                p++;

                in->out  = p;
                in->next = q;

                if (in->token == q) {
                    in->line++;
                    in->was_newline = TRUE;
                    return "\n";
                }
                else {
                    in->next_newline = TRUE;
                    return in->token;
                }
            }
            break;

            /*
             * Comments:
             *
             *     Attempt to allow and filter out partial-line comments.
             *
             *     This has not been thoroughly thought through. Probably
             *     there are broken border-cases. The whole tokenizing loop
             *     has not been written so that it could grow the buffer and
             *     refill it even if even we run out of input and we're not
             *     sure whehter a full token has been consumed... beware.
             *     To be sure, we bail out here if it looks like we exhausted
             *     the input buffer while skipping a comment. This needs to
             *     be thought through properly.
             */
        case MRP_START_COMMENT:
            skip_rest_of_line(in);
            if (in->out == in->in) {
                mrp_log_error("%s:%d Exhausted input buffer while skipping "
                              "a comment.", in->file, in->line);
                in->error = TRUE;
                return NULL;
            }
            else {
                p = in->out;
                in->line++;
            }
            break;

        default:
            *q++ = *p++;
            in->was_newline = FALSE;
        }
    }

    if (in->fd == -1) {
        *q = '\0';
        in->out = p;
        in->in = q;

        return in->token;
    }
    else {
        mrp_log_error("Input line %d of file %s exceeds allowed length.",
                      in->line, in->file);
        return NULL;
    }
}


/*
 * bridging to valgrind
 */

static void valgrind(const char *vg_path, int argc, char **argv, int vg_offs,
                     int saved_argc, char **saved_argv, char **envp)
{
#define VG_ARG(a) vg_argv[vg_argc++] = a
    char *vg_argv[MAX_ARGS + 1];
    int   vg_argc, normal_offs, i;

    vg_argc = 0;

    /* set valgrind binary */
    VG_ARG(vg_path ? (char *)vg_path : "/usr/bin/valgrind");

    /* add valgrind arguments */
    for (i = vg_offs; i < argc; i++)
        VG_ARG(argv[i]);

    /* save offset to normal argument list for fallback */
    normal_offs = vg_argc;

    /* add our binary and our arguments */
    for (i = 0; i < saved_argc; i++)
        vg_argv[vg_argc++] = saved_argv[i];

    /* terminate argument list */
    VG_ARG(NULL);

    /* try executing through valgrind */
    mrp_log_warning("Executing through valgrind (%s)...", vg_argv[0]);
    execve(vg_argv[0], vg_argv, envp);

    /* try falling back to normal execution */
    mrp_log_error("Executing through valgrind failed (error %d: %s), "
                  "retrying without...", errno, strerror(errno));
    execve(vg_argv[normal_offs], vg_argv + normal_offs, envp);

    /* can't do either, so just give up */
    mrp_log_error("Fallback to normal execution failed (error %d: %s).",
                  errno, strerror(errno));
    exit(1);
}
