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
	if (mrp_transport_send(t, msg))
	    mrp_log_info("Reply successfully sent.");
	else
	    mrp_log_error("Failed to send reply.");
    }

#if 0 /* done by the tranport layer... */
    mrp_msg_destroy(msg);
#endif
}


void recvfrom_evt(mrp_transport_t *t, mrp_msg_t *msg, void *addr,
		  socklen_t addrlen, void *user_data)
{
    context_t *c = (context_t *)user_data;

    MRP_UNUSED(t);    
    MRP_UNUSED(c);
    
    mrp_log_info("Received a message.");
    mrp_msg_dump(msg, stdout);
    
    if (c->server) {
	mrp_msg_append(msg, "type", "reply", strlen("reply")+1);
	if (mrp_transport_sendto(t, msg, addr, addrlen))
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


void server_init(context_t *c)
{
    static mrp_transport_evt_t evt = {
	.closed   = closed_evt,
	.recv     = NULL,
	.recvfrom = recvfrom_evt,
    };
    struct sockaddr addr;
    socklen_t       addrlen;

    c->t = mrp_transport_create(c->ml, "udp", &evt, c, 0);
    
    if (c->t == NULL) {
	mrp_log_error("Failed to create new transport.");
	exit(1);
    }

    addrlen = mrp_transport_resolve(c->t, c->addr, &addr, sizeof(addr));
    
    if (!addrlen) {
	mrp_log_error("Failed to resolve address '%s'.", c->addr);
	exit(1);
    }

    if (!mrp_transport_bind(c->t, &addr, addrlen)) {
	mrp_log_error("Failed to bind to %s.", c->addr);
	exit(1);
    }

    mrp_log_info("Waiting for messages on %s...", c->addr);
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
    
    if (!mrp_msg_append(msg, "seq", seq, len + 1) ||
	!mrp_msg_append(msg, "foo", "bar", sizeof("bar")) ||
	!mrp_msg_append(msg, "bar", "foo", sizeof("foo")) ||
	!mrp_msg_append(msg, "foobar", "barfoo", sizeof("barfoo")) ||
	!mrp_msg_append(msg, "barfoo", "foobar", sizeof("foobar"))) {
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

    struct sockaddr addr;
    socklen_t       addrlen;

    addrlen = mrp_transport_resolve(NULL, c->addr, &addr, sizeof(addr));

    if (addrlen <= 0) {
	mrp_log_error("Failed resolve transport address '%s'.", c->addr);
	exit(1);
    }

    c->t = mrp_transport_create(c->ml, "udp", &evt, c, 0);
    
    if (c->t == NULL) {
	mrp_log_error("Failed to create new transport.");
	exit(1);
    }

    if (!mrp_transport_connect(c->t, &addr, addrlen)) {
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
