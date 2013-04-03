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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/plugins/console-protocol.h>

#include <breedline/breedline-murphy.h>

#define client_info  mrp_log_info
#define client_warn  mrp_log_warning
#define client_error mrp_log_error

#define DEFAULT_PROMPT  "murphy"
#define DEFAULT_ADDRESS "unxs:@murphy-console"


/*
 * message types
 */

typedef enum {
    MSG_UNKNOWN,                         /* unknown message */
    MSG_PROMPT,                          /* set new prompt */
    MSG_COMMAND,                         /* client command */
    MSG_ECHO,                            /* output from server */
    MSG_COMPLETIONS,                     /* get/set completion results */
} msg_type_t;


/*
 * client receive buffer
 */

#define RECVBUF_MAXSIZE                  /* maximum buffer size */

typedef struct {
    char *buf;                           /* incoming data buffer */
    int   size;                          /* size of buffer */
    char *in;                            /* write pointer */
    char *out;                           /* read pointer */
} recvbuf_t;


/*
 * client context
 */

typedef struct {
    const char      *server;             /* server address */
    int              log_mask;           /* log mask */
    const char      *log_target;         /* log target */
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
    mrp_transport_t *t;                  /* transport to server */
    int              seqno;              /* sequence number */
    recvbuf_t        buf;                /* receive buffer */
    brl_t           *brl;                /* breedline for terminal input */
    char           **cmds;               /* commands to run */
    int              ncmd;               /* number of commands */
    int              ccmd;               /* current command */
} client_t;


int send_cmd(client_t *c, const char *cmd)
{
    mrp_msg_t *msg;
    uint16_t   tag, type;
    uint32_t   len;
    int        success;

    len = cmd ? strlen(cmd) + 1 : 0;

    if (len > 1) {
        tag  = MRP_CONSOLE_INPUT;
        type = MRP_MSG_FIELD_BLOB;
        msg  = mrp_msg_create(tag, type, len, cmd, NULL);

        if (msg != NULL) {
            success = mrp_transport_send(c->t, msg);
            mrp_msg_unref(msg);
            return success;
        }

        return FALSE;
    }
    else
        return TRUE;
}


void input_cb(brl_t *brl, const char *input, void *user_data)
{
    client_t *c   = (client_t *)user_data;
    int       len = input ? strlen(input) + 1 : 0;

    if (len > 1) {
        brl_add_history(brl, input);
        brl_hide_prompt(brl);

        send_cmd(c, input);

        brl_show_prompt(brl);
    }
}


static int input_setup(client_t *c)
{
    int         fd;
    const char *prompt;

    fd     = fileno(stdin);
    prompt = DEFAULT_PROMPT;
    c->brl = brl_create_with_murphy(fd, prompt, c->ml, input_cb, c);

    if (c->brl != NULL) {
        brl_show_prompt(c->brl);
        return TRUE;
    }
    else {
        mrp_log_error("Failed to breedline for console input.");
        return FALSE;
    }
}


static void input_cleanup(client_t *c)
{
    if (c->brl != NULL) {
        brl_destroy(c->brl);
        c->brl = NULL;
    }
}


static void hide_prompt(client_t *c)
{
    if (c->brl)
        brl_hide_prompt(c->brl);
}


static void set_prompt(client_t *c, const char *prompt)
{
    if (c->brl)
        brl_set_prompt(c->brl, prompt);
}


static void show_prompt(client_t *c)
{
    if (c->brl)
        brl_show_prompt(c->brl);
}


void recvfrom_evt(mrp_transport_t *t, mrp_msg_t *msg,
                  mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    client_t        *c = (client_t *)user_data;
    mrp_msg_field_t *f;
    char           *prompt, *output;
    size_t          size;

    MRP_UNUSED(t);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    hide_prompt(c);

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_OUTPUT)) != NULL) {
        output = f->str;
        size   = f->size[0];
        printf("%.*s", (int)size, output);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_PROMPT)) != NULL) {
        prompt = f->str;
        set_prompt(c, prompt);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_BYE)) != NULL) {
        mrp_mainloop_quit(c->ml, 0);
        return;
    }

    if (c->cmds != NULL) {
        if (c->ccmd < c->ncmd)
            send_cmd(c, c->cmds[c->ccmd++]);
        else
            mrp_mainloop_quit(c->ml, 0);
    }

    show_prompt(c);
}



void recv_evt(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    recvfrom_evt(t, msg, NULL, 0, user_data);
}


void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    client_t *c = (client_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(c);

    if (error) {
        mrp_log_error("Connection closed with error %d (%s).", error,
                      strerror(error));
        exit(1);
    }
    else {
        mrp_log_info("Peer has closed the connection.");
        mrp_mainloop_quit(c->ml, 0);
    }
}


int client_setup(client_t *c)
{
    static mrp_transport_evt_t evt;

    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;

    addrlen = mrp_transport_resolve(NULL, c->server,
                                    &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        evt.closed      = closed_evt;
        evt.recvmsg     = recv_evt;
        evt.recvmsgfrom = recvfrom_evt;

        c->t = mrp_transport_create(c->ml, type, &evt, c, 0);

        if (c->t == NULL) {
            mrp_log_error("Failed to create new transport.");
            return FALSE;
        }

        if (!mrp_transport_connect(c->t, &addr, addrlen)) {
            mrp_log_error("Failed to connect to %s.", c->server);
            mrp_transport_destroy(c->t);
            c->t = NULL;
            return FALSE;
        }

        return TRUE;
    }
    else
        mrp_log_error("Failed to resolve address '%s'.", c->server);

    return FALSE;
}


static void client_cleanup(client_t *c)
{
    mrp_transport_destroy(c->t);
    c->t = NULL;
}


static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    mrp_mainloop_t *ml = mrp_get_sighandler_mainloop(h);

    MRP_UNUSED(user_data);

    switch (signum) {
    case SIGINT:
        mrp_log_info("Got SIGINT, stopping...");
        if (ml != NULL)
            mrp_mainloop_quit(ml, 0);
        else
            exit(0);
        break;
    }
}


static void client_set_defaults(client_t *c)
{
    mrp_clear(c);
    c->seqno      = 1;
    c->server     = DEFAULT_ADDRESS;
    c->log_mask   = MRP_LOG_UPTO(MRP_LOG_INFO);
    c->log_target = MRP_LOG_TO_STDERR;
}


static void print_usage(const char *argv0, int exit_code, const char *fmt, ...)
{
    va_list     ap;
    const char *exe;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    exe = strrchr(argv0, '/');

    printf("usage: %s [options] [console-commands]\n\n"
           "The possible options are:\n"
           "  -s, --server <address>         server transport to connect to\n"
           "  -t, --log-target=TARGET        log target to use\n"
           "      TARGET is one of stderr,stdout,syslog, or a logfile path\n"
           "  -l, --log-level=LEVELS         logging level to use\n"
           "      LEVELS is a comma separated list of info, error and warning\n"
           "  -v, --verbose                  increase logging verbosity\n"
           "  -d, --debug                    enable debug messages\n"
           "  -h, --help                     show help on usage\n",
           argv0);
    printf("\n");
    printf("If commands are given on the command line, the console will ");
    printf("first execute\nthem then exit after receiving a response to ");
    printf("the last command. If no commands\n");
    printf("are given on the command line, the console will prompt for ");
    printf("commands to execute.\nFor a short summary of commands ");
    printf("try running '%s help'.\n", exe ? exe + 1 : argv0);

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


int parse_cmdline(client_t *c, int argc, char **argv)
{
#   define OPTIONS "s:l:t:v:d:h"
    struct option options[] = {
        { "server"    , required_argument, NULL, 's' },
        { "log-level" , required_argument, NULL, 'l' },
        { "log-target", required_argument, NULL, 't' },
        { "verbose"   , optional_argument, NULL, 'v' },
        { "debug"     , required_argument, NULL, 'd' },
        { "help"      , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 's':
            c->server = optarg;
            break;

        case 'v':
            c->log_mask <<= 1;
            c->log_mask  |= 1;
            break;

        case 'l':
            c->log_mask = mrp_log_parse_levels(optarg);
            if (c->log_mask < 0)
                print_usage(argv[0], EINVAL, "invalid log level '%s'", optarg);
            break;

        case 't':
            c->log_target = mrp_log_parse_target(optarg);
            if (!c->log_target)
                print_usage(argv[0], EINVAL, "invalid log target '%s'", optarg);
            break;

        case 'd':
            c->log_mask |= MRP_LOG_MASK_DEBUG;
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

    return optind;
}


int main(int argc, char *argv[])
{
    client_t c;
    int      next;

    client_set_defaults(&c);
    next = parse_cmdline(&c, argc, argv);

    mrp_log_set_mask(c.log_mask);
    mrp_log_set_target(c.log_target);

    c.seqno = 1;

    if ((c.ml = mrp_mainloop_create()) == NULL) {
        mrp_log_error("Failed to create mainloop.");
        exit(1);
    }

    mrp_add_sighandler(c.ml, SIGINT, signal_handler, &c);

    if (next >= argc) {
        if (!input_setup(&c))
            goto fail;
        c.cmds = NULL;
        c.ncmd = 0;
        c.ccmd = 0;
    }
    else {
        c.cmds = argv + next;
        c.ncmd = argc - next;
        c.ccmd = 0;
    }

    if (!client_setup(&c))
        goto fail;

    mrp_mainloop_run(c.ml);

    client_cleanup(&c);

    if (next >= argc)
        input_cleanup(&c);

    return 0;

 fail:
    client_cleanup(&c);
    input_cleanup(&c);
    exit(1);
}

