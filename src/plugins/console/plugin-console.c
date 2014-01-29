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

#include "config.h"

#ifdef WEBSOCKETS_ENABLED
#    include <murphy/common/wsck-transport.h>
#    include <murphy/common/json.h>
#endif

#include <murphy/plugins/console-protocol.h>

#define DEFAULT_ADDRESS "unxs:@murphy-console"    /* default console address */

#ifdef MURPHY_DATADIR                             /* default content dir */
#    define DEFAULT_HTTPDIR MURPHY_DATADIR"/webconsole"
#else
#    define DEFAULT_HTTPDIR "/usr/share/murphy/webconsole"
#endif

enum {
    DEBUG_NONE    = 0x0,
    DEBUG_FUNC    = 0x1,
    DEBUG_FILE    = 0x2,
    DEBUG_LINE    = 0x4,
    DEBUG_DEFAULT = DEBUG_FUNC
};


/*
 * an active console instance
 */

typedef struct {
    mrp_console_t   *mc;                 /* associated murphy console */
    mrp_transport_t *t;                  /* associated transport */
    mrp_sockaddr_t   addr;               /* for temp. datagram 'connection' */
    socklen_t        alen;               /* address length if any */
    int              id;                 /* console ID for log redirection */
    int              dbgmeta;            /* debug metadata to show */
} console_t;


/*
 * console plugin data
 */

typedef struct {
    const char      *address;            /* console address */
    mrp_transport_t *t;                  /* transport we're listening on */
    mrp_context_t   *ctx;                /* murphy context */
    mrp_list_hook_t  clients;            /* active console clients */
    mrp_sockaddr_t   addr;               /* resolved transport address */
    socklen_t        alen;               /* address length */
    console_t       *c;                  /* datagram console being served */
    const char      *httpdir;            /* WRT console agent directory */
    const char      *sslcert;            /* path to SSL certificate */
    const char      *sslpkey;            /* path to SSL private key */
    const char      *sslca;              /* path to SSL CA */
} data_t;



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
    char        buf[256], lnstr[64];

    MRP_UNUSED(file);
    MRP_UNUSED(line);
    MRP_UNUSED(func);

    switch (level) {
    case MRP_LOG_ERROR:   prefix = "[log] E: "; break;
    case MRP_LOG_WARNING: prefix = "[log] W: "; break;
    case MRP_LOG_INFO:    prefix = "[log] I: "; break;
    case MRP_LOG_DEBUG:
        if (c->dbgmeta & DEBUG_LINE)
            snprintf(lnstr, sizeof(lnstr), ":%d", line);
        else
            lnstr[0] = '\0';
        snprintf(buf, sizeof(buf), "[log] D: %s%s%s%s%s%s%s",
                 c->dbgmeta ? "[" : "",
                 c->dbgmeta & DEBUG_FUNC ? func  : "",
                 c->dbgmeta & DEBUG_FILE ? "@"   : "",
                 c->dbgmeta & DEBUG_FILE ? file  : "",
                 c->dbgmeta & DEBUG_LINE ? lnstr : "",
                 c->dbgmeta ? "]" : "",
                 c->dbgmeta ? " " : "");
        prefix = buf;
        break;
    default:              prefix = "[log] ?: ";
    }

    va_copy(cp, ap);
    mrp_console_printf(c->mc, "%s", prefix);
    mrp_console_vprintf(c->mc, format, cp);
    mrp_console_printf(c->mc, "\n");
    va_end(cp);
}


static void debug_cb(mrp_console_t *mc, void *user_data, int argc, char **argv)
{
    console_t  *c = (console_t *)mc->backend_data;
    int         debug;
    const char *p, *n;
    int         i, l;

    MRP_UNUSED(user_data);

    debug = 0;
    for (i = 2; i < argc; i++) {
        p = argv[i];
        while (p && *p) {
            if ((n = strchr(p, ',')) != NULL)
                l = n - p;
            else
                l = strlen(p);

            if      (!strncmp(p, "function", l) ||
                     !strncmp(p, "func"    , l)) debug |= DEBUG_FUNC;
            else if (!strncmp(p, "file"    , l)) debug |= DEBUG_FILE;
            else if (!strncmp(p, "line"    , l)) debug |= DEBUG_LINE;
            else
                mrp_log_warning("Unknown console debug flag '%*.*s'.", l, l, p);

            if ((p = n) != NULL)
                p++;
        }
    }

    c->dbgmeta = debug & ((debug & ~DEBUG_LINE) ? -1 : ~DEBUG_LINE);

    if (c->dbgmeta != debug)
        mrp_log_warning("Orphan console debug flag 'line' forced off.");
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


static void recv_cb(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
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

            if (size > 0) {
                MRP_CONSOLE_BUSY(c->mc, {
                        c->mc->evt.input(c->mc, input, size);
                    });

                c->mc->check_destroy(c->mc);
            }

            return;
        }
    }

    mrp_log_warning("Ignoring malformed message from console/%d...", c->id);
}


static void recvfrom_cb(mrp_transport_t *t, mrp_msg_t *msg,
                        mrp_sockaddr_t *addr, socklen_t alen, void *user_data)
{
    console_t       *c = (console_t *)user_data;
    mrp_sockaddr_t   obuf;
    socklen_t        olen;
    mrp_msg_field_t *f;
    char            *input;
    size_t           size;

    MRP_UNUSED(t);

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_INPUT)) != NULL) {
        if (f->type == MRP_MSG_FIELD_BLOB) {
            input = f->str;
            size  = f->size[0];

            if (size > 0) {
                mrp_sockaddr_cpy(&obuf, &c->addr, olen = c->alen);
                mrp_sockaddr_cpy(&c->addr, addr, c->alen = alen);
                mrp_transport_connect(t, addr, alen);

                MRP_CONSOLE_BUSY(c->mc, {
                        c->mc->evt.input(c->mc, input, size);
                    });

                c->mc->check_destroy(c->mc);


                mrp_transport_disconnect(t);

                if (olen) {
                    mrp_transport_connect(t, &obuf, olen);
                    mrp_sockaddr_cpy(&c->addr, &obuf, c->alen = olen);
                }

                return;
            }
        }
    }

    mrp_log_warning("Ignoring malformed message from console/%d...", c->id);
}


/*
 * generic stream transport
 */

#define stream_write_req      write_req
#define stream_set_prompt_req set_prompt_req
#define stream_free_req       free_req
#define stream_recv_cb        recv_cb

static void stream_close_req(mrp_console_t *mc)
{
    console_t *c = (console_t *)mc->backend_data;

    if (c->t != NULL) {
        mrp_transport_disconnect(c->t);
        mrp_transport_destroy(c->t);
        unregister_logger(c);

        c->t = NULL;
    }
}


static void stream_connection_cb(mrp_transport_t *lt, void *user_data)
{
    static mrp_console_req_t req;

    data_t    *data = (data_t *)user_data;
    console_t *c;
    int        flags;

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        flags = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_NONBLOCK;
        c->t  = mrp_transport_accept(lt, c, flags);

        if (c->t != NULL) {
            req.write      = stream_write_req;
            req.close      = stream_close_req;
            req.free       = stream_free_req;
            req.set_prompt = stream_set_prompt_req;

            c->mc = mrp_create_console(data->ctx, &req, c);

            if (c->mc != NULL) {
                c->id      = next_id++;
                c->dbgmeta = DEBUG_DEFAULT;
                register_logger(c);

                return;
            }
            else {
                mrp_transport_destroy(c->t);
                c->t = NULL;
            }
        }
    }
}


static void stream_closed_cb(mrp_transport_t *t, int error, void *user_data)
{
    console_t *c = (console_t *)user_data;

    if (error)
        mrp_log_error("Connection to console/%d closed with error %d (%s).",
                      c->id, error, strerror(error));
    else {
        mrp_log_info("console/%d has closed the connection.", c->id);

        mrp_transport_disconnect(t);
        mrp_transport_destroy(t);
        unregister_logger(c);
        c->t = NULL;
    }
}


static int stream_setup(data_t *data)
{
    static mrp_transport_evt_t evt;

    mrp_mainloop_t  *ml = data->ctx->ml;
    mrp_transport_t *t;
    const char      *type;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    int              flags;

    t    = NULL;
    alen = sizeof(addr);
    alen = mrp_transport_resolve(NULL, data->address, &addr, alen, &type);

    if (alen <= 0) {
        mrp_log_error("Failed to resolve console transport address '%s'.",
                      data->address);

        return FALSE;
    }

    evt.connection  = stream_connection_cb;
    evt.closed      = stream_closed_cb;
    evt.recvmsg     = stream_recv_cb;
    evt.recvmsgfrom = NULL;

    flags = MRP_TRANSPORT_REUSEADDR;
    t     = mrp_transport_create(ml, type, &evt, data, flags);

    if (t != NULL) {
        if (mrp_transport_bind(t, &addr, alen) && mrp_transport_listen(t, 1)) {
            data->t = t;

            return TRUE;
        }
        else {
            mrp_log_error("Failed to bind console to '%s'.", data->address);
            mrp_transport_destroy(t);
        }
    }
    else
        mrp_log_error("Failed to create console transport.");

    return FALSE;
}


/*
 * datagram transports
 */

#define dgram_write_req       write_req
#define dgram_free_req        free_req
#define dgram_set_prompt_req  set_prompt_req

#define dgram_recv_cb         recv_cb
#define dgram_recvfrom_cb     recvfrom_cb


static void dgram_close_req(mrp_console_t *mc)
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


static int dgram_setup(data_t *data)
{
    static mrp_transport_evt_t evt;
    static mrp_console_req_t   req;

    mrp_mainloop_t  *ml = data->ctx->ml;
    mrp_transport_t *t;
    const char      *type;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    int              flags;
    console_t       *c;

    t    = NULL;
    alen = sizeof(addr);
    alen = mrp_transport_resolve(NULL, data->address, &addr, alen, &type);

    if (alen <= 0) {
        mrp_log_error("Failed to resolve console transport address '%s'.",
                      data->address);

        return FALSE;
    }

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        evt.recvmsg     = dgram_recv_cb;
        evt.recvmsgfrom = dgram_recvfrom_cb;
        evt.connection  = NULL;
        evt.closed      = NULL;

        flags = MRP_TRANSPORT_REUSEADDR;
        t     = mrp_transport_create(ml, type, &evt, c, flags);

        if (t != NULL) {
            if (mrp_transport_bind(t, &addr, alen)) {
                req.write      = dgram_write_req;
                req.close      = dgram_close_req;
                req.free       = dgram_free_req;
                req.set_prompt = dgram_set_prompt_req;

                c->t  = t;
                c->mc = mrp_create_console(data->ctx, &req, c);

                if (c->mc != NULL) {
                    data->c         = c;
                    c->mc->preserve = TRUE;

                    return TRUE;
                }
                else
                    mrp_log_error("Failed to create console.");
            }
            else
                mrp_log_error("Failed to bind console to '%s'.", data->address);

            c->t = NULL;
            mrp_transport_destroy(t);
        }
        else
            mrp_log_error("Failed to create console transport.");

        mrp_free(c);
    }

    return FALSE;
}


#ifdef WEBSOCKETS_ENABLED

/*
 * websocket transport
 */

#define wsock_close_req stream_close_req
#define wsock_free_req  free_req
#define wsock_closed_cb stream_closed_cb

static ssize_t wsock_write_req(mrp_console_t *mc, void *buf, size_t size)
{
    console_t  *c = (console_t *)mc->backend_data;
    mrp_json_t *msg;

    msg = mrp_json_create(MRP_JSON_OBJECT);

    if (msg != NULL) {
        if (mrp_json_add_string_slice(msg, "output", buf, size))
            mrp_transport_sendcustom(c->t, msg);

        mrp_json_unref(msg);

        return size;
    }
    else
        return -1;
}


static void wsock_set_prompt_req(mrp_console_t *mc, const char *prompt)
{
    console_t  *c = (console_t *)mc->backend_data;
    mrp_json_t *msg;

    msg = mrp_json_create(MRP_JSON_OBJECT);

    if (msg != NULL) {
        if (mrp_json_add_string(msg, "prompt", prompt))
            mrp_transport_sendcustom(c->t, msg);

        mrp_json_unref(msg);
    }
}


static void wsock_recv_cb(mrp_transport_t *t, void *data, void *user_data)
{
    console_t  *c   = (console_t *)user_data;
    mrp_json_t *msg = (mrp_json_t *)data;
    const char *s;
    char       *input;
    size_t      size;

    MRP_UNUSED(t);

    s = mrp_json_object_to_string((mrp_json_t *)data);

    mrp_debug("recived WRT console message:");
    mrp_debug("  %s", s);

    if (mrp_json_get_string(msg, "input", &input)) {
        size = strlen(input);

        if (size > 0) {
            MRP_CONSOLE_BUSY(c->mc, {
                    c->mc->evt.input(c->mc, input, size);
                });

            c->mc->check_destroy(c->mc);
        }
    }
}


static void wsock_connection_cb(mrp_transport_t *lt, void *user_data)
{
    static mrp_console_req_t req;
    data_t    *data = (data_t *)user_data;
    console_t *c;

    mrp_debug("incoming web console connection...");

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        c->t = mrp_transport_accept(lt, c, 0);

        if (c->t != NULL) {
            req.write      = wsock_write_req;
            req.close      = wsock_close_req;
            req.free       = wsock_free_req;
            req.set_prompt = wsock_set_prompt_req;

            c->mc = mrp_create_console(data->ctx, &req, c);

            if (c->mc != NULL) {
                c->id = next_id++;
                register_logger(c);

                return;
            }
            else {
                mrp_transport_destroy(c->t);
                c->t = NULL;
            }
        }
    }
}


static int wsock_setup(data_t *data)
{
    static mrp_transport_evt_t evt;

    mrp_mainloop_t  *ml   = data->ctx->ml;
    const char      *cert = data->sslcert;
    const char      *pkey = data->sslpkey;
    const char      *ca   = data->sslca;
    mrp_transport_t *t;
    const char      *type;
    mrp_sockaddr_t   addr;
    socklen_t        alen;
    int              flags;

    t    = NULL;
    alen = sizeof(addr);
    alen = mrp_transport_resolve(NULL, data->address, &addr, alen, &type);

    if (alen <= 0) {
        mrp_log_error("Failed to resolve console transport address '%s'.",
                      data->address);

        return FALSE;
    }

    evt.connection  = wsock_connection_cb;
    evt.closed      = wsock_closed_cb;
    evt.recvcustom  = wsock_recv_cb;
    evt.recvmsgfrom = NULL;

    flags = MRP_TRANSPORT_MODE_CUSTOM;
    t     = mrp_transport_create(ml, type, &evt, data, flags);

    if (t != NULL) {
        if (cert || pkey || ca) {
            mrp_transport_setopt(t, MRP_WSCK_OPT_SSL_CERT, cert);
            mrp_transport_setopt(t, MRP_WSCK_OPT_SSL_PKEY, pkey);
            mrp_transport_setopt(t, MRP_WSCK_OPT_SSL_CA  , ca);
        }

        if (mrp_transport_bind(t, &addr, alen) && mrp_transport_listen(t, 1)) {
            mrp_transport_setopt(t, MRP_WSCK_OPT_HTTPDIR, data->httpdir);
            data->t = t;

            return TRUE;
        }
        else {
            mrp_log_error("Failed to bind console to '%s'.", data->address);
            mrp_transport_destroy(t);
        }
    }
    else
        mrp_log_error("Failed to create console transport.");

    return FALSE;
}

#endif /* WEBSOCKETS_ENABLED */


enum {
    ARG_ADDRESS,                         /* console transport address */
    ARG_HTTPDIR,                         /* content directory for HTTP */
    ARG_SSLCERT,                         /* path to SSL certificate */
    ARG_SSLPKEY,                         /* path to SSL private key */
    ARG_SSLCA                            /* path to SSL CA */
};



static int console_init(mrp_plugin_t *plugin)
{
    data_t *data;
    int     ok;

    if ((data = mrp_allocz(sizeof(*data))) != NULL) {
        mrp_list_init(&data->clients);

        data->ctx     = plugin->ctx;
        data->address = plugin->args[ARG_ADDRESS].str;
        data->httpdir = plugin->args[ARG_HTTPDIR].str;
        data->sslcert = plugin->args[ARG_SSLCERT].str;
        data->sslpkey = plugin->args[ARG_SSLPKEY].str;
        data->sslca   = plugin->args[ARG_SSLCA].str;

        mrp_log_info("Using console address '%s'...", data->address);

        if (!strncmp(data->address, "wsck:", 5)) {
            if (data->httpdir != NULL)
                mrp_log_info("Using '%s' for serving console Web agent...",
                             data->httpdir);
            else
                mrp_log_info("Not serving console Web agent...");
        }

        if (!strncmp(data->address, "tcp4:", 5) ||
            !strncmp(data->address, "tcp6:", 5) ||
            !strncmp(data->address, "unxs:", 5))
            ok = stream_setup(data);
#ifdef WEBSOCKETS_ENABLED
        else if (!strncmp(data->address, "wsck:", 5))
            ok = wsock_setup(data);
#endif
        else
            ok = dgram_setup(data);

        if (ok) {
            plugin->data = data;

            return TRUE;
        }
    }

    mrp_free(data);

    return FALSE;
}


static void console_exit(mrp_plugin_t *plugin)
{
    mrp_log_info("Cleaning up %s...", plugin->instance);
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
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS , STRING, "address", DEFAULT_ADDRESS),
    MRP_PLUGIN_ARGIDX(ARG_HTTPDIR , STRING, "httpdir", DEFAULT_HTTPDIR),
    MRP_PLUGIN_ARGIDX(ARG_SSLCERT , STRING, "sslcert", NULL),
    MRP_PLUGIN_ARGIDX(ARG_SSLPKEY , STRING, "sslpkey", NULL),
    MRP_PLUGIN_ARGIDX(ARG_SSLCA   , STRING, "sslca"  , NULL)
};


MRP_CONSOLE_GROUP(console_commands, "console", NULL, NULL, {
        MRP_TOKENIZED_CMD("debug", debug_cb, FALSE,
                          "debug [function] [file] [line]",
                          "set debug metadata to show",
                          "Set what metadata to show for debug messages."),
});

MURPHY_REGISTER_CORE_PLUGIN("console",
                            CONSOLE_VERSION, CONSOLE_DESCRIPTION,
                            CONSOLE_AUTHORS, CONSOLE_HELP, MRP_MULTIPLE,
                            console_init, console_exit,
                            console_args, MRP_ARRAY_SIZE(console_args),
                            NULL, 0, NULL, 0, &console_commands);
