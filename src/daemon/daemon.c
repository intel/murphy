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
