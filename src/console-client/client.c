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
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/plugins/console-protocol.h>

#include <breedline/breedline-murphy.h>

#define client_info  mrp_log_info
#define client_warn  mrp_log_warning
#define client_error mrp_log_error

#define DEFAULT_PROMPT  "murphy"
#define DEFAULT_ADDRESS "tcp4:127.0.0.1:3000"


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
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
    mrp_transport_t *t;                  /* transport to server */
    int              seqno;              /* sequence number */
    recvbuf_t        buf;                /* receive buffer */
    brl_t           *brl;                /* breedline for terminal input */
} client_t;


void input_cb(brl_t *brl, const char *input, void *user_data)
{
    client_t  *c = (client_t *)user_data;
    mrp_msg_t *msg;
    uint16_t   tag, type;
    uint32_t   len;

    len = input ? strlen(input) + 1: 0;

    if (len > 1) {
        brl_add_history(brl, input);
        brl_hide_prompt(brl);

        tag  = MRP_CONSOLE_INPUT;
        type = MRP_MSG_FIELD_BLOB;
        msg  = mrp_msg_create(tag, type, len, input, NULL);

        if (msg != NULL) {
            mrp_transport_send(c->t, msg);
            mrp_msg_unref(msg);
        }

        brl_show_prompt(brl);
        return;
    }

    client_error("failed to send request to server.");
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

    brl_hide_prompt(c->brl);

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_OUTPUT)) != NULL) {
        output = f->str;
        size   = f->size[0];
        printf("%.*s", (int)size, output);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_PROMPT)) != NULL) {
        prompt = f->str;
        brl_set_prompt(c->brl, prompt);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_BYE)) != NULL) {
        mrp_mainloop_quit(c->ml, 0);
        return;
    }

    brl_show_prompt(c->brl);
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


int client_setup(client_t *c, const char *addrstr)
{
    static mrp_transport_evt_t evt = {
        .closed      = closed_evt,
        .recvmsg     = recv_evt,
        .recvmsgfrom = recvfrom_evt,
    };

    mrp_sockaddr_t  addr;
    socklen_t       addrlen;
    const char     *type;

    addrlen = mrp_transport_resolve(NULL, addrstr, &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        c->t = mrp_transport_create(c->ml, type, &evt, c, 0);

        if (c->t == NULL) {
            mrp_log_error("Failed to create new transport.");
            return FALSE;
        }

        if (!mrp_transport_connect(c->t, &addr, addrlen)) {
            mrp_log_error("Failed to connect to %s.", addrstr);
            mrp_transport_destroy(c->t);
            c->t = NULL;
            return FALSE;
        }

        return TRUE;
    }
    else
        mrp_log_error("Failed to resolve address '%s'.", addrstr);

    return FALSE;
}


static void client_cleanup(client_t *c)
{
    mrp_transport_destroy(c->t);
    c->t = NULL;
}


static void signal_handler(mrp_mainloop_t *ml, mrp_sighandler_t *h,
                           int signum, void *user_data)
{
    MRP_UNUSED(h);
    MRP_UNUSED(user_data);

    switch (signum) {
    case SIGINT:
        mrp_log_info("Got SIGINT, stopping...");
        mrp_mainloop_quit(ml, 0);
        break;
    }
}


int main(int argc, char *argv[])
{
    client_t    c;
    const char *addr;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_clear(&c);

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_INFO));
    mrp_log_set_target(MRP_LOG_TO_STDOUT);

    c.seqno = 1;

    if ((c.ml = mrp_mainloop_create()) == NULL) {
        mrp_log_error("Failed to create mainloop.");
        exit(1);
    }

    mrp_add_sighandler(c.ml, SIGINT, signal_handler, &c);

    if (argc == 2)
        addr = argv[1];
    else
        addr = DEFAULT_ADDRESS;

    if (!input_setup(&c) || !client_setup(&c, addr))
        goto fail;

    mrp_mainloop_run(c.ml);

    client_cleanup(&c);
    input_cleanup(&c);

    return 0;

 fail:
    client_cleanup(&c);
    input_cleanup(&c);
    exit(1);
}

