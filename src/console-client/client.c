#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <murphy/common.h>
#include <murphy/plugins/console-protocol.h>

#define client_info  mrp_log_info
#define client_warn  mrp_log_warning
#define client_error mrp_log_error

#define DEFAULT_PROMPT  "murphy> "
#define DEFAULT_ADDRESS "tcp4:127.0.0.1:3000"

static void input_process_cb(char *input);

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
    int              in;                 /* terminal input */
    mrp_io_watch_t  *inw;                /* input I/O watch */
    mrp_transport_t *t;                  /* transport to server */
    int             seqno;               /* sequence number */
    recvbuf_t       buf;                 /* receive buffer */
    char            prompt_str[256];
    int             prompt_len;
} client_t;



/*
 * prompt handling and readline glue
 */

/*
 * Although readline has an interface for mainloop integration, it does
 * not support opaque 'user_data' in the callbacks so one cannot pass in
 * any application-specific data. We need to store the client context in
 * this global just for that...
 */
static client_t *rl_client;


void prompt_set(client_t *c, const char *prompt)
{
    strncpy(c->prompt_str, prompt, sizeof(c->prompt_str) - 1);
    c->prompt_len = strlen(c->prompt_str);
}


void prompt_erase(client_t *c)
{
    int n = c->prompt_len;

    printf("\r");
    while (n-- > 0)
	printf(" ");
    printf("\r");
}


void prompt_display(client_t *c)
{
    rl_callback_handler_remove();
    rl_callback_handler_install(c->prompt_str, input_process_cb);
}


void prompt_process_input(client_t *c, char *input)
{
    mrp_msg_t *msg;
    uint16_t   tag, type;
    uint32_t   len;

    len = input ? strlen(input) + 1: 0;

    if (len > 1) {
	add_history(input);
	prompt_erase(c);
	
	tag  = MRP_CONSOLE_INPUT;
	type = MRP_MSG_FIELD_BLOB;
	msg  = mrp_msg_create(tag, type, len, input, NULL);

	if (msg != NULL) {
	    mrp_transport_send(c->t, msg);
	    mrp_msg_unref(msg);
	}
	
	prompt_display(c);
	return;
    }

    client_error("failed to send request to server.");
}


static void input_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
		     mrp_io_event_t events, void *user_data)
{
    MRP_UNUSED(w);
    MRP_UNUSED(fd);
    MRP_UNUSED(user_data);

    if (events & MRP_IO_EVENT_IN)
	rl_callback_read_char();
    
    if (events & MRP_IO_EVENT_HUP)
	mrp_mainloop_quit(ml, 0);
}


static void input_process_cb(char *input)
{
    prompt_process_input(rl_client, input);
    free(input);
}


static int input_setup(client_t *c)
{
    mrp_io_event_t events;

    c->in  = fileno(stdin);
    events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
    c->inw = mrp_add_io_watch(c->ml, c->in,events, input_cb, c);

    if (c->inw != NULL) {
	prompt_set(c, DEFAULT_PROMPT);
	prompt_display(c);
	return TRUE;
    }
    else {
	mrp_log_error("Failed to create I/O watch for console input.");
	return FALSE;
    }
}


static void input_cleanup(client_t *c)
{
    mrp_del_io_watch(c->inw);
    c->inw = NULL;
    rl_callback_handler_remove();
}


void recvfrom_evt(mrp_transport_t *t, mrp_msg_t *msg,
		  mrp_sockaddr_t *addr, socklen_t addrlen, void *user_data)
{
    client_t        *c = (client_t *)user_data;
    mrp_msg_field_t *f;
    char           *prompt, *output, buf[128];
    size_t          size;
    
    MRP_UNUSED(t);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);
    
    prompt_erase(c);

    if ((f = mrp_msg_find(msg, MRP_CONSOLE_OUTPUT)) != NULL) {
	output = f->str;
	size   = f->size[0];
	printf("%.*s", (int)size, output);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_PROMPT)) != NULL) {
	prompt = f->str;
	snprintf(buf, sizeof(buf), "%s> ", prompt);
	prompt_set(c, buf);
    }
    else if ((f = mrp_msg_find(msg, MRP_CONSOLE_BYE)) != NULL) {
	mrp_mainloop_quit(c->ml, 0);
	return;
    }
    
    prompt_display(c);
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
        
    rl_client = &c;                      /* readline has not user_data */

    mrp_mainloop_run(c.ml);

    client_cleanup(&c);
    input_cleanup(&c);
    
    return 0;

 fail:
    client_cleanup(&c);
    input_cleanup(&c);
    exit(1);
}

