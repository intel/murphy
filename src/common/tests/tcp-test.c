#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <murphy/common.h>


typedef struct {
    mrp_mainloop_t  *ml;
    mrp_transport_t *t;
    char            *addr;
    int              server;
    int              sock;
    mrp_io_watch_t  *iow;
    mrp_timer_t     *timer;
} context_t;


void recv_evt(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);    
    MRP_UNUSED(c);
    
    mrp_log_info("Received a message.");
    mrp_msg_dump(msg, stdout);
    
    if (c->server) {

#define REPLY_KEY "this_is_a_rather_long_reply_field_name_that_I_hope_will_cause_reallocation_of_the_message_receiving_buffer_on_the_server_side_and_we_will_see_if_it_can_automatically_readjust_its_buffers"
#define REPLY_VAL "and_this_is_the_rather_long_value_of_the_rather_long_field_name_that_we_hope_might_break_something_if_the_allocation_algorithm_has_horrible_easy_to_exploit_holes"

	mrp_msg_append(msg, REPLY_KEY, REPLY_VAL, strlen(REPLY_VAL) + 1);

	if (mrp_transport_send(t, msg))
	    mrp_log_info("Reply successfully sent.");
	else
	    mrp_log_error("Failed to send reply.");
    }

#if 0 /* done by the tranport layer... */
    mrp_msg_destroy(msg);
#endif
}


void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);
    MRP_UNUSED(c);

    if (error) {
	mrp_log_error("Connection closed with error %d (%s).", error,
		      strerror(error));
	exit(1);
    }
    else {
	mrp_log_info("Peer has closed the connection.");
	exit(0);
    }
}


static void accept_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
		      mrp_io_event_t events, void *user_data)
{
    static mrp_transport_evt_t evt = {
	.closed   = closed_evt,
	.recv     = recv_evt,
	.recvfrom = NULL,
    };
    
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(w);

    if (events & MRP_IO_EVENT_IN) {
	c->t = mrp_transport_accept(ml, "tcp", &fd, &evt, c);

	if (c->t == NULL) {
	    mrp_log_error("Failed to accept new transport.");
	    exit(1);
	}
    }
}


void server_init(context_t *c)
{
    struct sockaddr addr;
    socklen_t       addrlen;
    mrp_io_event_t  events;
    int             reuse;
    long            nonblk;

    addrlen = mrp_transport_resolve(NULL, c->addr, &addr, sizeof(addr));

    if (addrlen > 0) {
	c->sock = socket(addr.sa_family, SOCK_STREAM, 0);
	
	if (c->sock < 0) {
	    mrp_log_error("Failed to create socket.");
	    exit(1);
	}

	reuse = 1;
	setsockopt(c->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	nonblk = 1;
	fcntl(c->sock, F_SETFL, O_NONBLOCK, nonblk);

	if (bind(c->sock, &addr, addrlen) < 0) {
	    mrp_log_error("Failed to bind socket to %s (%d: %s).", c->addr,
			  errno, strerror(errno));
	    exit(1);
	}

	if (listen(c->sock, 4) < 0) {
	    mrp_log_error("Failed to listen on socket (%d: %s).", errno,
			  strerror(errno));
	    exit(1);
	}
	
	events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
	c->iow = mrp_add_io_watch(c->ml, c->sock, events, accept_cb, c);

	if (c->iow == NULL) {
	    mrp_log_error("Failed to create I/O watch for socket.");
	    exit(1);
	}
    }
    else {
	mrp_log_error("Failed to resolve address %s.", c->addr);
	exit(1);
    }
}


void send_cb(mrp_mainloop_t *ml, mrp_timer_t *t, void *user_data)
{
    static int seqno = 1;

    context_t *c = (context_t *)user_data;
    mrp_msg_t *msg;
    char       seq[32];
    int        len;

    MRP_UNUSED(ml);
    MRP_UNUSED(t);

    if ((msg = mrp_msg_create(NULL)) == NULL) {
	mrp_log_error("Failed to create new message.");
	exit(1);
    }

    len = snprintf(seq, sizeof(seq), "%d", seqno);
    
#define LONG_KEY "aaaaaaaaaaaallllllllllllloooooooooooonnnnnnnnnnngggggggggffffffffffffiiiiiiiiiiiiieeeeeeeeeeeelllllllllllllddddddddddddnnnnnnnnnnnnnnnaaaaaaaaaaaaaaaammmmmmmmmmmmmmmeeeeeeeeeeeeeeeeeeeeee"
#define LONG_VAL "aaaaaaaaaaallllllllllllllllloooooooooooonnnnnnnnngggggggggggvvvvvvvvvvvvaaaaaaaaaaaaaalllllllllluuuuuuuuuuuuuueeeeee"

    if (!mrp_msg_append(msg, "seq", seq, len + 1) ||
	!mrp_msg_append(msg, "foo", "bar", sizeof("bar")) ||
	!mrp_msg_append(msg, "bar", "foo", sizeof("foo")) ||
	!mrp_msg_append(msg, "foobar", "barfoo", sizeof("barfoo")) ||
	!mrp_msg_append(msg, "barfoo", "foobar", sizeof("foobar")) ||
	!mrp_msg_append(msg, LONG_KEY, LONG_VAL, strlen(LONG_VAL) + 1)) {
	mrp_log_error("Failed to construct message #%d.", seqno);
	exit(1);
    }

    if (!mrp_transport_send(c->t, msg)) {
	mrp_log_error("Failed to send message #%d.", seqno);
	exit(1);
    }
    else {
	mrp_log_info("Message #%d succesfully sent.", seqno++);
    }
}


void client_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
	.closed   = closed_evt,
	.recv     = recv_evt,
	.recvfrom = NULL,
    };

    c->t = mrp_transport_create(c->ml, "tcp", &evt, c);
    
    if (c->t == NULL) {
	mrp_log_error("Failed to create new transport.");
	exit(1);
    }

    if (!mrp_transport_connect(c->t, c->addr)) {
	mrp_log_error("Failed to connect to %s.", c->addr);
	exit(1);
    }

    c->timer = mrp_add_timer(c->ml, 1000, send_cb, c);

    if (c->timer == NULL) {
	mrp_log_error("Failed to create send timer.");
	exit(1);
    }
}


int main(int argc, char *argv[])
{
    context_t c;

    mrp_clear(&c);
    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_DEBUG));
    mrp_log_set_target(MRP_LOG_TO_STDOUT);

    if (argc == 3 && (!strcmp(argv[1], "-s") || !strcmp(argv[1], "--server"))) {
	c.server = TRUE;
	c.addr  = argv[2];
	mrp_log_info("Running as server, using address '%s'...", c.addr);
    }
    else if (argc == 2) {
	c.addr = argv[1];
	mrp_log_info("Running as client, using address '%s'...", c.addr);
    }
    else {
	mrp_log_error("invalid command line arguments");
	mrp_log_error("usage: %s [-s] address:port", argv[0]);
	exit(1);
    }

    c.ml = mrp_mainloop_create();

    if (c.server)
	server_init(&c);
    else
	client_init(&c);
    
    mrp_mainloop_run(c.ml);

    return 0;
}
