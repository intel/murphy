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

#define console_info(fmt, args...)  mrp_log_info("console: "fmt , ## args)
#define console_warn(fmt, args...)  mrp_log_warning("console: "fmt , ## args)
#define console_error(fmt, args...) mrp_log_error("console: "fmt , ## args)

#define MRP_CFG_MAXLINE 4096             /* input line length limit */

/*
 * console plugin data
 */

typedef struct {
    const char      *address;            /* console address */
    int              sock;               /* main socket for new connections */
    mrp_io_watch_t  *iow;                /* main socket I/O watch */
    mrp_context_t   *ctx;                /* murphy context */
    mrp_list_hook_t  consoles;           /* active consoles */
} data_t;


/*
 * a console instance
 */

typedef struct {
    mrp_console_t   *mc;                 /* associated murphy console */
    mrp_transport_t *t;                  /* associated transport */
} console_t;


static int console_listen(const char *address)
{
    struct sockaddr addr;
    socklen_t       addrlen;
    int             sock, on;

    addrlen = mrp_transport_resolve(NULL, address, &addr, sizeof(addr));

    if (!addrlen) {
	console_error("invalid console address '%s'.", address);
	return FALSE;
    }
    
    if ((sock = socket(addr.sa_family, SOCK_STREAM, 0)) < 0) {
	console_error("failed to create console socket (%d: %s).",
		      errno, strerror(errno));
	goto fail;
    }
    
    on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(sock, &addr, addrlen) != 0) {
	console_error("failed to bind to address '%s' (%d: %s).",
		      address, errno, strerror(errno));
	goto fail;
    }

    if (listen(sock, 4) < 0) {
	console_error("failed to listen for connections (%d: %s).",
		      errno, strerror(errno));
	goto fail;
    }

    return sock;

 fail:
    if (sock >= 0)
	close(sock);
    
    return FALSE;
}


static ssize_t write_req(mrp_console_t *mc, void *buf, size_t size)
{
    console_t *c = (console_t *)mc->backend_data;
    mrp_msg_t *msg;

    if ((msg = mrp_msg_create("output", buf, size, NULL)) != NULL) {
	mrp_transport_send(c->t, msg);
	mrp_msg_unref(msg);
	
	return size;
    }
    else
	return -1;
}


static void close_req(mrp_console_t *mc)
{
    console_t *c = (console_t *)mc->backend_data;
    
    mrp_transport_disconnect(c->t);
    mrp_transport_destroy(c->t);
    c->t = NULL;
}


static void set_prompt_req(mrp_console_t *mc, const char *prompt)
{
    console_t *c = (console_t *)mc->backend_data;
    mrp_msg_t *msg;

    msg = mrp_msg_create("prompt", prompt, strlen(prompt) + 1, NULL);
    
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
    console_t *c = (console_t *)user_data;
    char      *input;
    size_t     size;

    MRP_UNUSED(t);
    
    input = mrp_msg_find(msg, "input", &size);

    if (input != NULL) {
	MRP_CONSOLE_BUSY(c->mc, {
		c->mc->evt.input(c->mc, input, size);
	    });
	
	c->mc->check_destroy(c->mc);
    }
    else
	mrp_log_error("Received malformed console message.");
    
#if 0 /* done by the transport layer... */
    mrp_msg_unref(msg);         /* XXX TODO change to refcounting */
#endif
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
	c->t = NULL;
    }
}


static void accept_cb(mrp_mainloop_t *ml, mrp_io_watch_t *iow, int fd,
		      mrp_io_event_t events, void *user_data)
{
    static mrp_transport_evt_t evt = {
	.recv     = recv_evt,
	.recvfrom = NULL,
	.closed   = closed_evt,
    };
    
    static mrp_console_req_t req = {
	.write      = write_req,
	.close      = close_req,
	.free       = free_req,
	.set_prompt = set_prompt_req,
    };
    
    data_t    *data = (data_t *)user_data;
    console_t *c;

    MRP_UNUSED(iow);

    c = NULL;

    if (events & MRP_IO_EVENT_IN) {
	if ((c = mrp_allocz(sizeof(*c))) != NULL) {
	    c->t = mrp_transport_accept(ml, "tcp", &fd, &evt, c);

	    if (c->t != NULL) {
		c->mc = mrp_create_console(data->ctx, &req, c);
		
		if (c->mc != NULL) {
		    console_info("accepted new console connection.");
		    return;
		}
		else
		    console_error("failed to create new console.");
	    }
	    else
		console_error("failed to accept console connection.");
	}
	else
	    console_error("failed to allocate new console.");

	if (c != NULL) {
	    if (c->t != NULL)
		mrp_transport_destroy(c->t);
	    
	    mrp_free(c);
	}
    }
}


enum {
    ARG_ADDRESS                          /* console address, 'address:port' */
};


static int console_init(mrp_plugin_t *plugin)
{
    data_t *data;
    mrp_mainloop_t *ml;
    mrp_io_event_t  events;

    if ((data = mrp_allocz(sizeof(*data))) != NULL) {
	mrp_list_init(&data->consoles);

	data->ctx     = plugin->ctx;
	data->address = plugin->args[ARG_ADDRESS].str;
	data->sock    = console_listen(data->address);

	if (data->sock < 0)
	    goto fail;

	ml        = data->ctx->ml;
	events    = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP | MRP_IO_EVENT_ERR;
	data->iow = mrp_add_io_watch(ml, data->sock, events, accept_cb, data);
	
	if (data->iow == NULL) {
	    console_error("failed to set up console I/O watch.");
	    goto fail;
	}

	plugin->data = data;
	console_info("set up at address '%s'.", data->address);

	return TRUE;
    }
    
 fail:
    if (data != NULL) {
	if (data->sock >= 0)
	    close(data->sock);
	
	mrp_free(data);
    }

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
    "The debug console provides a telnet-like remote session and a\n"	  \
    "simple shell-like command interpreter with commands to help\n"	  \
    "development, debugging, and trouble-shooting. The set of commands\n" \
    "can be dynamically extended by registering new commands from\n"	  \
    "other plugins."

#define CONSOLE_VERSION MRP_VERSION_INT(0, 0, 1)
#define CONSOLE_AUTHORS "Krisztian Litkey <kli@iki.fi>"


static mrp_plugin_arg_t console_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS, STRING, "address", "tcp:127.0.0.1:3000"),
};

MURPHY_REGISTER_CORE_PLUGIN("console",
			    CONSOLE_VERSION, CONSOLE_DESCRIPTION,
			    CONSOLE_AUTHORS, CONSOLE_HELP, MRP_SINGLETON,
			    console_init, console_exit, console_args, NULL);


