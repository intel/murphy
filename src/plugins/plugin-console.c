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
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/plugins/console-protocol.h>

#define console_info(fmt, args...)  mrp_log_info("console: "fmt , ## args)
#define console_warn(fmt, args...)  mrp_log_warning("console: "fmt , ## args)
#define console_error(fmt, args...) mrp_log_error("console: "fmt , ## args)

#define MRP_CFG_MAXLINE 4096             /* input line length limit */

typedef struct console_s console_t;

/*
 * console plugin data
 */

typedef struct {
    const char      *address;            /* console address */
    mrp_transport_t *t;                  /* transport we're listening on */
    int              sock;               /* main socket for new connections */
    mrp_io_watch_t  *iow;                /* main socket I/O watch */
    mrp_context_t   *ctx;                /* murphy context */
    mrp_list_hook_t  consoles;           /* active consoles */
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    console_t       *c;
} data_t;


/*
 * a console instance
 */

struct console_s {
    mrp_console_t   *mc;                 /* associated murphy console */
    mrp_transport_t *t;                  /* associated transport */
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    int              id;
};


static int next_id = 1;

static ssize_t write_req(mrp_console_t *mc, void *buf, size_t size)
{
    console_t *c = (console_t *)mc->backend_data;
    mrp_msg_t *msg;
    uint16_t   tag, type;
    uint32_t   len;

    tag  = MRP_CONSOLE_OUTPUT;
    type = MRP_MSG_FIELD_BLOB;
    len  = size;
    msg  = mrp_msg_create(tag, type, len, buf, NULL);

    if (msg != NULL) {
        mrp_transport_send(c->t, msg);
        mrp_msg_unref(msg);

        return size;
    }
    else
        return -1;
}


static void logger(void *data, mrp_log_level_t level, const char *file,
                   int line, const char *func, const char *format, va_list ap)
{
    console_t  *c = (console_t *)data;
    va_list     cp;
    const char *prefix;

    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    switch (level) {
    case MRP_LOG_ERROR:   prefix = "[log] E: "; break;
    case MRP_LOG_WARNING: prefix = "[log] W: "; break;
    case MRP_LOG_INFO:    prefix = "[log] I: "; break;
    case MRP_LOG_DEBUG:   prefix = "[log] D: "; break;
    default:              prefix = "[log] ?: ";
    }

    va_copy(cp, ap);
    mrp_console_printf(c->mc, "%s", prefix);
    mrp_console_vprintf(c->mc, format, cp);
    mrp_console_printf(c->mc, "\n");
    va_end(cp);
}


static void register_logger(console_t *c)
{
    char name[32];

    if (!c->id)
        return;

    snprintf(name, sizeof(name), "console/%d", c->id);
    mrp_log_register_target(name, logger, c);
}


static void unregister_logger(console_t *c)
{
    char name[32];

    if (!c->id)
        return;

    snprintf(name, sizeof(name), "console/%d", c->id);
    mrp_log_unregister_target(name);
}


static void tcp_close_req(mrp_console_t *mc)
{
    console_t *c = (console_t *)mc->backend_data;

    if (c->t != NULL) {
        mrp_transport_disconnect(c->t);
        mrp_transport_destroy(c->t);
        unregister_logger(c);
        c->t = NULL;
    }
}


static void udp_close_req(mrp_console_t *mc)
{
    console_t *c = (console_t *)mc->backend_data;
    mrp_msg_t *msg;
    uint16_t   tag, type;

    tag  = MRP_CONSOLE_BYE;
    type = MRP_MSG_FIELD_BOOL;
    msg  = mrp_msg_create(tag, type, TRUE, NULL);

    if (msg != NULL) {
        mrp_transport_send(c->t, msg);
        mrp_msg_unref(msg);
    }

    mrp_transport_disconnect(c->t);
}


static void set_prompt_req(mrp_console_t *mc, const char *prompt)
{
    console_t *c = (console_t *)mc->backend_data;
    mrp_msg_t *msg;
    uint16_t   tag, type;

    tag  = MRP_CONSOLE_PROMPT;
    type = MRP_MSG_FIELD_STRING;
    msg  = mrp_msg_create(tag, type, prompt, NULL);

    if (msg != NULL) {
        mrp_transport_send(c->t, msg);
        mrp_msg_unref(msg);
    }
}


static void free_req(void *backend_data)
{
    mrp_free(backend_data);
}


static void recv_evt(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    console_t       *c = (console_t *)user_data;
    mrp_msg_field_t *f;
    char            *input;
    size_t           size;

    MRP_UNUSED(t);

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_INPUT)) != NULL) {
        if (f->type == MRP_MSG_FIELD_BLOB) {
            input = f->str;
            size  = f->size[0];
            MRP_CONSOLE_BUSY(c->mc, {
                    c->mc->evt.input(c->mc, input, size);
                });

            c->mc->check_destroy(c->mc);
            return;
        }
    }

    mrp_log_error("Received malformed console message.");
}


static void recvfrom_evt(mrp_transport_t *t, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    console_t       *c = (console_t *)user_data;
    mrp_msg_field_t *f;
    char            *input;
    size_t           size;

    MRP_UNUSED(t);

    mrp_debug("got new message...");

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_INPUT)) != NULL) {
        if (f->type == MRP_MSG_FIELD_BLOB) {
            input = f->str;
            size  = f->size[0];

            if (input != NULL) {
                mrp_sockaddr_t   a;
                socklen_t        l;

                mrp_sockaddr_cpy(&a, &c->addr, l=c->addrlen);
                mrp_sockaddr_cpy(&c->addr, addr, c->addrlen=addrlen);

                mrp_transport_connect(t, addr, addrlen);
                MRP_CONSOLE_BUSY(c->mc, {
                        c->mc->evt.input(c->mc, input, size);
                    });

                c->mc->check_destroy(c->mc);

                mrp_transport_disconnect(t);

                mrp_sockaddr_cpy(&c->addr, &a, c->addrlen=l);


                if (l)
                    mrp_transport_connect(t, &a, l);

                return;
            }
        }
    }

    mrp_log_error("Received malformed console message.");
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    console_t *c = (console_t *)user_data;

    if (error)
        mrp_log_error("Connection closed with error %d (%s).", error,
                      strerror(error));
    else {
        mrp_log_info("Peer has closed the console connection.");

        mrp_transport_disconnect(t);
        mrp_transport_destroy(t);
        unregister_logger(c);
        c->t = NULL;
        unregister_logger(c);
    }
}


static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    static mrp_console_req_t req;

    data_t    *data  = (data_t *)user_data;
    int        flags;
    console_t *c;

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        c->t  = mrp_transport_accept(lt, c, flags);

        if (c->t != NULL) {
            req.write      = write_req;
            req.close      = tcp_close_req;
            req.free       = free_req;
            req.set_prompt = set_prompt_req;

            c->mc = mrp_create_console(data->ctx, &req, c);

            if (c->mc != NULL) {
                c->id = next_id++;
                register_logger(c);
                return;
            }
        }

        mrp_transport_destroy(c->t);
        mrp_free(c);
    }
}


enum {
    ARG_ADDRESS                          /* console address, 'address:port' */
};


static int strm_setup(data_t *data)
{
    static mrp_transport_evt_t evt;

    mrp_transport_t *t;
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    int              flags;
    const char      *type;

    t       = NULL;
    addrlen = mrp_transport_resolve(NULL, data->address,
                                    &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        evt.closed      = closed_evt;
        evt.recvmsg     = recv_evt;
        evt.recvmsgfrom = NULL;
        evt.connection  = connection_evt;

        flags = MRP_TRANSPORT_REUSEADDR;
        t     = mrp_transport_create(data->ctx->ml, type, &evt, data, flags);

        if (t != NULL) {
            if (mrp_transport_bind(t, &addr, addrlen)) {
                if (mrp_transport_listen(t, 4)) {
                    data->t = t;
                    return TRUE;
                }
                else
                    console_error("Failed to listen on server transport.");
            }
            else
                console_error("Failed to bind to address %s.", data->address);
        }
        else
            console_error("Failed to create main console transport.");
    }
    else
        console_error("Invalid console address '%s'.", data->address);

    mrp_transport_destroy(t);

    return FALSE;
}


static int dgrm_setup(data_t *data)
{
    static mrp_transport_evt_t evt;
    static mrp_console_req_t   req;

    console_t       *c;
    mrp_transport_t *t;
    mrp_sockaddr_t   addr;
    socklen_t        addrlen;
    int              f;
    const char      *type;

    t       = NULL;
    addrlen = mrp_transport_resolve(NULL, data->address,
                                    &addr, sizeof(addr), &type);

    if (addrlen > 0) {
        if ((c = mrp_allocz(sizeof(*c))) != NULL) {
            evt.recvmsg     = recv_evt;
            evt.recvmsgfrom = recvfrom_evt;
            evt.closed      = NULL;

            f = MRP_TRANSPORT_REUSEADDR;
            t = mrp_transport_create(data->ctx->ml, type, &evt, c, f);

            if (t != NULL) {
                if (mrp_transport_bind(t, &addr, addrlen)) {
                    req.write      = write_req;
                    req.close      = udp_close_req;
                    req.free       = free_req;
                    req.set_prompt = set_prompt_req;

                    c->t  = t;
                    c->mc = mrp_create_console(data->ctx, &req, c);

                    if (c->mc != NULL){
                        data->c         = c;
                        c->mc->preserve = TRUE;
                        return TRUE;
                    }
                }

                mrp_transport_destroy(t);
            }

            mrp_free(c);
        }
    }
    else
        console_error("Invalid console address '%s'.", data->address);

    return FALSE;
}


static int console_init(mrp_plugin_t *plugin)
{
    data_t *data;
    int     ok;

    if ((data = mrp_allocz(sizeof(*data))) != NULL) {
        mrp_list_init(&data->consoles);

        data->ctx     = plugin->ctx;
        data->address = plugin->args[ARG_ADDRESS].str;

        if (!strncmp(data->address, "tcp4:", 5) ||
            !strncmp(data->address, "tcp6:", 5) ||
            !strncmp(data->address, "unxs:", 5))
            ok = strm_setup(data);
        else
            ok = dgrm_setup(data);

        if (ok) {
            plugin->data = data;
            console_info("set up at address '%s'.", data->address);

            return TRUE;
        }
    }

    mrp_free(data);

    console_error("failed to set up console at address '%s'.",
                  plugin->args[ARG_ADDRESS].str);

    return FALSE;
}


static void console_exit(mrp_plugin_t *plugin)
{
    console_info("cleaning up instance '%s'...", plugin->instance);
}


#define CONSOLE_DESCRIPTION "A debug console for Murphy."
#define CONSOLE_HELP \
    "The debug console provides a telnet-like remote session and a\n"     \
    "simple shell-like command interpreter with commands to help\n"       \
    "development, debugging, and trouble-shooting. The set of commands\n" \
    "can be dynamically extended by registering new commands from\n"      \
    "other plugins."

#define CONSOLE_VERSION MRP_VERSION_INT(0, 0, 1)
#define CONSOLE_AUTHORS "Krisztian Litkey <kli@iki.fi>"


static mrp_plugin_arg_t console_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS, STRING, "address", "unxs:@murphy-console"),
};

MURPHY_REGISTER_CORE_PLUGIN("console",
                            CONSOLE_VERSION, CONSOLE_DESCRIPTION,
                            CONSOLE_AUTHORS, CONSOLE_HELP, MRP_SINGLETON,
                            console_init, console_exit,
                            console_args, MRP_ARRAY_SIZE(console_args),
                            NULL, 0, NULL, 0, NULL);


