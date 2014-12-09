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

#include <stdlib.h>
#include <signal.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/utils.h>
#include <murphy/core/context.h>
#include <murphy/core/plugin.h>
#include <murphy/resolver/resolver.h>
#include <murphy/daemon/config.h>
#include <murphy/daemon/daemon.h>


/*
 * daemon-related events
 */

enum {
    DAEMON_EVENT_LOADING  = 0,           /* daemon loading configuration */
    DAEMON_EVENT_STARTING,               /* daemon starting plugins */
    DAEMON_EVENT_RUNNING,                /* daemon entering mainloop */
    DAEMON_EVENT_STOPPING                /* daemon shutting down */
};


MRP_REGISTER_EVENTS(daemon_events,
                    MRP_EVENT(MRP_DAEMON_LOADING , DAEMON_EVENT_LOADING ),
                    MRP_EVENT(MRP_DAEMON_STARTING, DAEMON_EVENT_STARTING),
                    MRP_EVENT(MRP_DAEMON_RUNNING , DAEMON_EVENT_RUNNING ),
                    MRP_EVENT(MRP_DAEMON_STOPPING, DAEMON_EVENT_STOPPING));


static int emit_daemon_event(mrp_context_t *ctx, int idx)
{
    mrp_event_bus_t *bus   = ctx->daemon_bus;
    uint32_t         id    = daemon_events[idx].id;
    int              flags = MRP_EVENT_SYNCHRONOUS;

    return mrp_event_emit_msg(bus, id, flags, MRP_MSG_END);
}


static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    mrp_mainloop_t *ml  = mrp_get_sighandler_mainloop(h);
    mrp_context_t  *ctx = (mrp_context_t *)user_data;

    MRP_UNUSED(ctx);

    switch (signum) {
    case SIGINT:
        mrp_log_info("Got SIGINT, stopping...");
        if (ml != NULL)
            mrp_mainloop_quit(ml, 0);
        else
            exit(0);
        break;

    case SIGTERM:
        mrp_log_info("Got SIGTERM, stopping...");
        mrp_mainloop_quit(ml, 0);
        break;
    }
}


static mrp_context_t *create_context(void)
{
    mrp_context_t *ctx;

    ctx = mrp_context_create();

    if (ctx != NULL) {
        ctx->daemon_bus = mrp_event_bus_get(ctx->ml, MRP_DAEMON_BUS);
        return ctx;
    }
    else
        mrp_log_error("Failed to create murphy main context.");

    exit(1);
}


static void setup_signals(mrp_context_t *ctx)
{
    mrp_add_sighandler(ctx->ml, SIGINT , signal_handler, ctx);
    mrp_add_sighandler(ctx->ml, SIGTERM, signal_handler, ctx);
}


static void parse_cmdline(mrp_context_t *ctx, int argc, char **argv, char **env)
{
    mrp_parse_cmdline(ctx, argc, argv, env);
}


static void load_configuration(mrp_context_t *ctx)
{
    mrp_cfgfile_t *cfg;

    mrp_context_setstate(ctx, MRP_STATE_LOADING);
    emit_daemon_event(ctx, DAEMON_EVENT_LOADING);

    cfg = mrp_parse_cfgfile(ctx->config_file);

    if (cfg != NULL) {
        mrp_log_info("Blacklisted plugins of any type: %s",
                     ctx->blacklist_plugins ? ctx->blacklist_plugins:"<none>");
        mrp_log_info("Blacklisted builtin plugins: %s",
                     ctx->blacklist_builtin ? ctx->blacklist_builtin:"<none>");
        mrp_log_info("Blacklisted dynamic plugins: %s",
                     ctx->blacklist_dynamic ? ctx->blacklist_dynamic:"<none>");
        mrp_log_info("Whitelisted plugins of any type: %s",
                     ctx->whitelist_plugins ? ctx->whitelist_plugins:"<none>");
        mrp_log_info("Whitelisted builtin plugins: %s",
                     ctx->whitelist_builtin ? ctx->whitelist_builtin:"<none>");
        mrp_log_info("Whitelisted dynamic plugins: %s",
                     ctx->whitelist_dynamic ? ctx->whitelist_dynamic:"<none>");

        mrp_block_blacklisted_plugins(ctx);

        if (!mrp_exec_cfgfile(ctx, cfg)) {
            mrp_log_error("Failed to execute configuration.");
            exit(1);
        }
    }
    else {
        mrp_log_error("Failed to parse configuration file '%s'.",
                      ctx->config_file);
        exit(1);
    }
}


static void create_ruleset(mrp_context_t *ctx)
{
    ctx->r = mrp_resolver_create(ctx);
}


static void load_ruleset(mrp_context_t *ctx)
{
    if (ctx->resolver_ruleset != NULL) {
        if (mrp_resolver_parse(ctx->r, ctx, ctx->resolver_ruleset))
            mrp_log_info("Loaded resolver ruleset '%s'.",
                         ctx->resolver_ruleset);
        else {
            mrp_log_error("Failed to load resolver ruleset '%s'.",
                          ctx->resolver_ruleset);
            exit(1);
        }
    }
}


static void start_plugins(mrp_context_t *ctx)
{
    mrp_context_setstate(ctx, MRP_STATE_STARTING);
    emit_daemon_event(ctx, DAEMON_EVENT_STARTING);

    if (mrp_start_plugins(ctx))
        mrp_log_info("Successfully started all loaded plugins.");
    else {
        mrp_log_error("Some plugins failed to start.");
        exit(1);
    }
}


static void setup_logging(mrp_context_t *ctx)
{
    const char *target;

    target = mrp_log_parse_target(ctx->log_target);

    if (!target)
        mrp_log_error("invalid log target '%s'", ctx->log_target);
    else
        mrp_log_set_target(target);
}

static void daemonize(mrp_context_t *ctx)
{
    if (!ctx->foreground) {
        mrp_log_info("Switching to daemon mode.");

        if (!mrp_daemonize("/", "/dev/null", "/dev/null")) {
            mrp_log_error("Failed to daemonize.");
            exit(1);
        }
    }
}


static void prepare_ruleset(mrp_context_t *ctx)
{
    if (ctx->r != NULL) {
        if (mrp_resolver_prepare(ctx->r))
            mrp_log_info("Ruleset prepared for resolution.");
        else {
            mrp_log_error("Failed to prepare ruleset for execution.");
            exit(1);
        }
        if (!mrp_resolver_enable_autoupdate(ctx->r, "autoupdate")) {
            mrp_log_error("Failed to enable resolver autoupdate.");
            exit(1);
        }
    }
}


static void run_mainloop(mrp_context_t *ctx)
{
    mrp_context_setstate(ctx, MRP_STATE_RUNNING);
    emit_daemon_event(ctx, DAEMON_EVENT_RUNNING);
    mrp_mainloop_run(ctx->ml);
}


static void stop_plugins(mrp_context_t *ctx)
{
    MRP_UNUSED(ctx);

    mrp_context_setstate(ctx, MRP_STATE_STOPPING);
    emit_daemon_event(ctx, DAEMON_EVENT_STOPPING);
}


static void cleanup_context(mrp_context_t *ctx)
{
    mrp_log_info("Shutting down...");
    mrp_context_destroy(ctx);
}


static void set_linebuffered(FILE *stream)
{
    fflush(stream);
    setvbuf(stream, NULL, _IOLBF, 0);
}


static void set_nonbuffered(FILE *stream)
{
    fflush(stream);
    setvbuf(stream, NULL, _IONBF, 0);
}


int main(int argc, char *argv[], char *envp[])
{
    mrp_context_t *ctx;

    ctx = create_context();

    setup_signals(ctx);
    create_ruleset(ctx);
    parse_cmdline(ctx, argc, argv, envp);
    load_configuration(ctx);
    start_plugins(ctx);
    load_ruleset(ctx);
    prepare_ruleset(ctx);
    setup_logging(ctx);
    daemonize(ctx);
    set_linebuffered(stdout);
    set_nonbuffered(stderr);
    run_mainloop(ctx);
    stop_plugins(ctx);

    cleanup_context(ctx);

    return 0;
}
