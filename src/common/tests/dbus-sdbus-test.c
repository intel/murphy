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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common.h>
#include <murphy/common/dbus-sdbus.h>

#define SERVER_NAME      "org.test.murphy-server"
#define SERVER_PATH      "/server"
#define SERVER_INTERFACE "Murphy.Server"
#define PING             "ping"
#define CLIENT_NAME      "org.test.murphy-client"
#define CLIENT_PATH      "/client"
#define CLIENT_INTERFACE "Murphy.Client"
#define PONG             "pong"


typedef struct {
    char            *busaddr;
    char            *srvname;
    int              server;
    int              log_mask;
    const char      *log_target;
    mrp_mainloop_t  *ml;
    mrp_timer_t     *timer;
    uint32_t         seqno;
    mrp_dbus_t      *dbus;
    const char      *name;
    int32_t          cid;
    int              server_up;
    int              all_pongs;
} context_t;


static mrp_dbus_msg_t *create_pong_signal(mrp_dbus_t *dbus, const char *dest,
                                          uint32_t seq)
{
    const char     *sig = "u";
    mrp_dbus_msg_t *msg;

    msg = mrp_dbus_msg_signal(dbus, dest, SERVER_PATH, SERVER_INTERFACE, PONG);

    if (msg != NULL) {
        if (mrp_dbus_msg_open_container(msg, MRP_DBUS_TYPE_ARRAY, sig) &&
            mrp_dbus_msg_append_basic(msg, MRP_DBUS_TYPE_UINT32, &seq) &&
            mrp_dbus_msg_close_container(msg))
            return msg;
        else
            mrp_dbus_msg_unref(msg);
    }

    return NULL;
}


static uint32_t parse_pong_signal(mrp_dbus_msg_t *msg)
{
    const char *sig = "u";
    uint32_t    seq;

    if (mrp_dbus_msg_enter_container(msg, MRP_DBUS_TYPE_ARRAY, sig) &&
        mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_UINT32, &seq) &&
        mrp_dbus_msg_exit_container(msg))
        return seq;
    else
        return (uint32_t)-1;
}


static int ping_handler(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data)
{
    context_t      *c = (context_t *)user_data;
    mrp_dbus_msg_t *pong;
    uint32_t        seq;
    const char     *dest;

    MRP_UNUSED(c);

    if (mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_UINT32, &seq))
        mrp_log_info("-> ping request #%u", seq);
    else
        mrp_log_error("-> malformed ping request");

    if (mrp_dbus_reply(dbus, msg, MRP_DBUS_TYPE_UINT32, &seq,
                        MRP_DBUS_TYPE_INVALID))
        mrp_log_info("<- ping reply #%u", seq);
    else
        mrp_log_error("Failed to send ping reply #%u.", seq);

    if (seq & 0x1)
        dest = mrp_dbus_msg_sender(msg);
    else
        dest = NULL;

    if ((pong = create_pong_signal(dbus, dest, seq)) != NULL) {
        if (mrp_dbus_send_msg(dbus, pong))
            mrp_log_info("<- pong %s #%u", dest ? "signal" : "broadcast", seq);
        else
            mrp_log_error("Failed to send pong signal #%u.", seq);

        mrp_dbus_msg_unref(pong);
    }
    else
        mrp_log_error("Failed to create pong signal #%u.", seq);

    return TRUE;
}


static int name_owner_changed(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg,
                              void *user_data)
{
    context_t  *c = (context_t *)user_data;
    const char *name, *prev, *next;

    MRP_UNUSED(c);
    MRP_UNUSED(dbus);

    if (mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_STRING, &name) &&
        mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_STRING, &prev) &&
        mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_STRING, &next))
        mrp_log_info("Name %s was reassigned from %s to %s...", name,
                     prev, next);
    else
        mrp_log_error("Failed to parse NameOwnerChanged signal.");

    return TRUE;
}


static void server_setup(context_t *c)
{
    c->dbus = mrp_dbus_connect(c->ml, c->busaddr, NULL);

    if (c->dbus == NULL) {
        mrp_log_error("Failed to create D-BUS connection to '%s' bus.",
                      c->busaddr);
        exit(1);
    }

    c->name = mrp_dbus_get_unique_name(c->dbus);
    mrp_log_info("Our address is %s on the bus...",
                 c->name ? c->name : "unknown");

    if (c->srvname && *c->srvname) {
        if (!mrp_dbus_acquire_name(c->dbus, c->srvname, NULL)) {
            mrp_log_error("Failed to acquire D-BUS name '%s' on bus '%s'.",
                          c->srvname, c->busaddr);
            exit(1);
        }
    }

    if (!mrp_dbus_export_method(c->dbus, SERVER_PATH, SERVER_INTERFACE,
                                PING, ping_handler, c)) {
        mrp_log_error("Failed to export D-BUS method '%s'.", PING);
        exit(1);
    }

    if (!mrp_dbus_subscribe_signal(c->dbus, name_owner_changed, c,
                                   "org.freedesktop.DBus",
                                   "/org/freedesktop/DBus",
                                   "org.freedesktop.DBus",
                                   "NameOwnerChanged",
                                   NULL)) {
        mrp_log_error("Failed to subscribe to NameOwnerChanged signals.");
        exit(1);
    }
}


void server_cleanup(context_t *c)
{
    if (c->srvname && *c->srvname)
        mrp_dbus_release_name(c->dbus, c->srvname, NULL);

    mrp_dbus_remove_method(c->dbus, SERVER_PATH, SERVER_INTERFACE,
                           PING, ping_handler, c);
    mrp_dbus_unref(c->dbus);
}


static void ping_reply(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data)
{
    context_t *c = (context_t *)user_data;
    uint32_t   seq;

    MRP_UNUSED(dbus);
    MRP_UNUSED(user_data);

    if (mrp_dbus_msg_type(msg) == MRP_DBUS_MESSAGE_TYPE_ERROR) {
        mrp_log_error("Received errorping reply.");

        return;
    }

    if (mrp_dbus_msg_read_basic(msg, MRP_DBUS_TYPE_UINT32, &seq))
        mrp_log_info("-> ping reply #%u", seq);
    else
        mrp_log_error("Received malformedping reply.");

    c->cid = 0;
}


static void ping_request(context_t *c)
{
    uint32_t seq;

    if (c->cid != 0) {
        mrp_log_warning("Previous ping request still unanswered...");
        return;
    }

    seq    = c->seqno++;
    c->cid = mrp_dbus_call(c->dbus,
                           c->srvname, SERVER_PATH, SERVER_INTERFACE,
                           PING, 500, ping_reply, c,
                           MRP_DBUS_TYPE_UINT32, &seq,
                           MRP_DBUS_TYPE_INVALID);

    if (c->cid > 0)
        mrp_log_info("<- ping request #%u", seq);
    else
        mrp_log_warning("Failed to send ping request #%u.", seq);
}


static void send_cb(mrp_timer_t *t, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);

    ping_request(c);
}


static int pong_handler(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *user_data)
{
    context_t *c = (context_t *)user_data;
    uint32_t   seq;

    MRP_UNUSED(c);
    MRP_UNUSED(dbus);

    if ((seq = parse_pong_signal(msg)) != (uint32_t)-1)
        mrp_log_info("-> pong signal #%u", seq);
    else
        mrp_log_error("-> malformed pong signal");

    return TRUE;
}


static void server_status_cb(mrp_dbus_t *dbus, const char *name, int up,
                             const char *owner, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(dbus);
    MRP_UNUSED(name);

    if (up) {
        mrp_log_info("%s came up (as %s)", name, owner);

        if (c->timer == NULL) {
            c->timer = mrp_add_timer(c->ml, 1000, send_cb, c);

            if (c->timer == NULL) {
                mrp_log_error("Failed to create D-BUS sending timer.");
                exit(1);
            }
        }
    }
    else {
        mrp_log_info("%s went down", name);

        if (c->timer != NULL) {
            mrp_del_timer(c->timer);
            c->timer = NULL;
        }
    }
}


static void client_setup(context_t *c)
{
    const char *dest;

    c->dbus = mrp_dbus_connect(c->ml, c->busaddr, NULL);

    if (c->dbus == NULL) {
        mrp_log_error("Failed to create D-BUS connection to '%s' bus.",
                      c->busaddr);
        exit(1);
    }

    c->name = mrp_dbus_get_unique_name(c->dbus);
    mrp_log_info("Our address is %s on the bus...",
                 c->name ? c->name : "unknown");

    mrp_dbus_follow_name(c->dbus, c->srvname, server_status_cb, c);

    if (c->all_pongs) {
        mrp_log_info("Subscribing for all pong signals...");
        dest = NULL;
    }
    else {
        mrp_log_info("Subscribing only for pong signals to us...");
        dest = c->name;
    }

    if (!mrp_dbus_subscribe_signal(c->dbus, pong_handler, c,
                                   dest, SERVER_PATH, SERVER_INTERFACE,
                                   PONG, NULL)) {
        mrp_log_error("Failed to subscribe for signal '%s/%s.%s'.", SERVER_PATH,
                      SERVER_INTERFACE, PONG);
        exit(1);
    }

    c->timer = mrp_add_timer(c->ml, 1000, send_cb, c);

    if (c->timer == NULL) {
        mrp_log_error("Failed to create D-BUS sending timer.");
        exit(1);
    }
}


static void client_cleanup(context_t *c)
{
    mrp_dbus_forget_name(c->dbus, c->srvname, server_status_cb, c);
    mrp_del_timer(c->timer);
    mrp_dbus_unsubscribe_signal(c->dbus, pong_handler, c,
                                c->name, SERVER_PATH, SERVER_INTERFACE,
                                PONG, NULL);
    mrp_dbus_unref(c->dbus);
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list ap;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    printf("usage: %s [options]\n\n"
           "The possible options are:\n"
           "  -s, --server                   run as test server (default)\n"
           "  -b, --bus                      connect the given D-BUS\n"
           "      If omitted, defaults to the session bus.\n"
           "  -a, --all-pongs                subscribe for all pong signals\n"
           "      If omitted, only pong with the client address are handled.\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug site               enable debug message for <site>\n"
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
    ctx->busaddr    = "session";
    ctx->srvname    = SERVER_NAME;
    ctx->server     = FALSE;
    ctx->log_mask   = MRP_LOG_UPTO(MRP_LOG_DEBUG);
    ctx->log_target = MRP_LOG_TO_STDERR;
}


int parse_cmdline(context_t *ctx, int argc, char **argv)
{
#   define OPTIONS "sab:n:l:t:vd:h"
    struct option options[] = {
        { "server"    , no_argument      , NULL, 's' },
        { "bus"       , required_argument, NULL, 'b' },
        { "name"      , required_argument, NULL, 'n' },
        { "all-pongs" , no_argument      , NULL, 'a' },
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
        case 's':
            ctx->server = TRUE;
            break;

        case 'b':
            ctx->busaddr = optarg;
            break;

        case 'n':
            ctx->srvname = optarg;
            break;

        case 'a':
            ctx->all_pongs = TRUE;
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
            ctx->log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_debug_set_config(optarg);
            mrp_debug_enable(TRUE);
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


static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    mrp_mainloop_t *ml = mrp_get_sighandler_mainloop(h);
    context_t      *c = (context_t *)user_data;

    MRP_UNUSED(c);

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
        if (ml != NULL)
            mrp_mainloop_quit(ml, 0);
        else
            exit(0);
        break;
    }
}


int main(int argc, char *argv[])
{
    context_t c;

    mrp_clear(&c);

    if (!parse_cmdline(&c, argc, argv))
        exit(1);

    mrp_log_set_mask(c.log_mask);
    mrp_log_set_target(c.log_target);

    if (c.server)
        mrp_log_info("Running as server, using D-BUS '%s'...", c.busaddr);
    else
        mrp_log_info("Running as client, using D-BUS '%s'...", c.busaddr);

    c.ml = mrp_mainloop_create();

    if (c.ml == NULL) {
        mrp_log_error("Failed to create mainloop.");
        exit(1);
    }

    mrp_add_sighandler(c.ml, SIGINT , signal_handler, &c);

    if (c.server)
        server_setup(&c);
    else
        client_setup(&c);

    mrp_mainloop_run(c.ml);

    if (c.server)
        server_cleanup(&c);
    else
        client_cleanup(&c);

    return 0;
}
