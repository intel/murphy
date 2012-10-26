#include <stdlib.h>
#include <signal.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/utils.h>
#include <murphy/core/context.h>
#include <murphy/core/plugin.h>
#include <murphy/daemon/config.h>


static void signal_handler(mrp_mainloop_t *ml, mrp_sighandler_t *h,
                           int signum, void *user_data)
{
    mrp_context_t *ctx = (mrp_context_t *)user_data;

    MRP_UNUSED(ctx);
    MRP_UNUSED(h);

    switch (signum) {
    case SIGINT:
        mrp_log_info("Got SIGINT, stopping...");
        mrp_mainloop_quit(ml, 0);
        break;

    case SIGTERM:
        mrp_log_info("Got SIGTERM, stopping...");
        mrp_mainloop_quit(ml, 0);
        break;
    }
}


int main(int argc, char *argv[])
{
    mrp_context_t *ctx;
    mrp_cfgfile_t *cfg;

    ctx = mrp_context_create();

    if (ctx != NULL) {
        if (!mrp_parse_cmdline(ctx, argc, argv)) {
            mrp_log_error("Failed to parse command line.");
            exit(1);
        }

        mrp_add_sighandler(ctx->ml, SIGINT , signal_handler, ctx);
        mrp_add_sighandler(ctx->ml, SIGTERM, signal_handler, ctx);

        mrp_log_set_mask(ctx->log_mask);
        mrp_log_set_target(ctx->log_target);

        cfg = mrp_parse_cfgfile(ctx->config_file);

        if (cfg == NULL) {
            mrp_log_error("Failed to parse configuration file '%s'.",
                          ctx->config_file);
            exit(1);
        }

        if (!mrp_exec_cfgfile(ctx, cfg)) {
            mrp_log_error("Failed to execute configuration.");
            exit(1);
        }

        if (!mrp_start_plugins(ctx))
            exit(1);

        if (!ctx->foreground) {
            if (!mrp_daemonize("/", "/dev/null", "/dev/null"))
                exit(1);
        }

        mrp_mainloop_run(ctx->ml);

        mrp_log_info("Exiting...");
        mrp_context_destroy(ctx);
    }
    else {
        mrp_log_error("Failed to create murphy context.");
        exit(1);
    }

    return 0;
}
