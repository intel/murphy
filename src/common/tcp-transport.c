#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/msg.h>
#include <murphy/common/transport.h>

#define DEFAULT_SIZE 128                 /* default input buffer size */

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    int             sock;                /* TCP socket */
    mrp_io_watch_t *iow;                 /* socket I/O watch */
    void           *ibuf;                /* input buffer */
    size_t          isize;               /* input buffer size */
    size_t          idata;               /* amount of input data */
    void           *obuf;                /* output buffer */
    size_t          osize;               /* output buffer size */
    size_t          odata;               /* amount of output data */
} tcp_t;


static void tcp_recv_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			mrp_io_event_t events, void *user_data);
static int tcp_disconnect(mrp_transport_t *mt);
static int open_socket(tcp_t *t, int family);

static socklen_t tcp_resolve(char *str, void *addr, socklen_t size)
{
    struct addrinfo *ai, hints;
    char             node[512], *port;
    
    mrp_clear(&hints);    
    hints.ai_family = AF_UNSPEC;
    ai              = NULL;

    if      (!strncmp(str, "tcp:" , 4)) str += 4;
    else if (!strncmp(str, "tcp4:", 5)) str += 5, hints.ai_family = AF_INET;
    else if (!strncmp(str, "tcp6:", 5)) str += 5, hints.ai_family = AF_INET6;
    
    strncpy(node, str, sizeof(node) - 1);
    node[sizeof(node) - 1] = '\0';
    if ((port = strrchr(node, ':')) == NULL)
	return FALSE;
    *port++ = '\0';

    if (getaddrinfo(node, port, &hints, &ai) == 0) {
	if (size >= ai->ai_addrlen) {
	    memcpy(addr, ai->ai_addr, ai->ai_addrlen);
	    size = ai->ai_addrlen;
	}
	else
	    size = 0;
	freeaddrinfo(ai);

	return size;
    }
    else
	return 0;
}


static int tcp_open(mrp_transport_t *mt)
{
    tcp_t *t = (tcp_t *)mt;
    
    t->sock = -1;

    return TRUE;
}


static int tcp_bind(mrp_transport_t *mt, void *addr, socklen_t addrlen)
{
    tcp_t *t = (tcp_t *)mt;
    
    if (t->sock != -1 || open_socket(t, ((struct sockaddr *)addr)->sa_family)) {
	if (bind(t->sock, (struct sockaddr *)addr, addrlen) == 0)
	    return TRUE;
    }
    
    return FALSE;
}


static int tcp_accept(mrp_transport_t *mt, void *conn)
{
    tcp_t           *t = (tcp_t *)mt;
    struct sockaddr  addr;
    socklen_t        addrlen;
    int              reuse;
    long             nonblk;
    mrp_io_event_t   events;    

    addrlen = sizeof(addr);
    t->sock = accept(*(int *)conn, &addr, &addrlen);

    if (t->sock >= 0) {
	reuse = 1;
	setsockopt(t->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	nonblk = 1;
	fcntl(t->sock, F_SETFL, O_NONBLOCK, nonblk);

	events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
	t->iow = mrp_add_io_watch(t->ml, t->sock, events, tcp_recv_cb, t);
	    
	if (t->iow != NULL) {
	    t->connected = TRUE;

	    return TRUE;
	}
    }
    
    if (t->sock != -1) {
	close(t->sock);
	t->sock = -1;
    }

    return FALSE;
}


static void tcp_close(mrp_transport_t *mt)
{
    tcp_t *t = (tcp_t *)mt;

    mrp_del_io_watch(t->iow);
    t->iow = NULL;

    mrp_free(t->ibuf);
    t->ibuf  = NULL;
    t->isize = 0;
    t->idata = 0;
    
    if (t->sock >= 0){
	close(t->sock);
	t->sock = -1;
    }
}


static void tcp_recv_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			 mrp_io_event_t events, void *user_data)
{
    tcp_t           *t  = (tcp_t *)user_data;
    mrp_transport_t *mt = (mrp_transport_t *)t;
    uint32_t        *sizep, size;
    ssize_t          n, space, left;
    void            *data;
    int              old, error;
    mrp_msg_t       *msg;

    MRP_UNUSED(ml);
    MRP_UNUSED(w);

    if (events & MRP_IO_EVENT_IN) {
	/*
	 * enlarge the buffer buddy-style if we're out of space
	 */
    realloc:
	if (t->idata == t->isize) {
	    if (t->isize > sizeof(size)) {
		old       = t->isize;
		sizep     = t->ibuf;
		size      = sizeof(size) + ntohl(*sizep);
		t->isize  = size;
	    }
	    else {
		old      = 0;
		t->isize = DEFAULT_SIZE;
	    }
	    if (!mrp_reallocz(t->ibuf, old, t->isize)) {
		error = ENOMEM;
	    fatal_error:
	    closed:
		tcp_disconnect(mt);
		
		if (t->evt.closed != NULL)
		    MRP_TRANSPORT_BUSY(mt, {
			    mt->evt.closed(mt, error, mt->user_data);
			});
		
		t->check_destroy(mt);
		return;
	    }
	}

	
	space = t->isize - t->idata;
	while ((n = read(fd, t->ibuf + t->idata, space)) > 0) {
	    t->idata += n;

	    if (t->idata >= sizeof(size)) {
		sizep = t->ibuf;
		size  = ntohl(*sizep);
		
		while (t->idata >= sizeof(size) + size) {
		    data = t->ibuf + sizeof(size);
		    msg  = mrp_msg_default_decode(data, size);

		    if (msg != NULL) {
			MRP_TRANSPORT_BUSY(mt, {
				mt->evt.recv(mt, msg, mt->user_data);
			    });

			mrp_msg_unref(msg);

			if (t->check_destroy(mt))
			    return;
		    }
		    else {
			error = EPROTO;
			goto fatal_error;
		    }

		    left = t->idata - (sizeof(size) + size);
		    memmove(t->ibuf, t->ibuf + sizeof(size) + size, left);
		    t->idata = left;
		    
		    if (t->idata >= sizeof(size)) {
			sizep = t->ibuf;
			size = ntohl(*sizep);
		    }
		    else
			size = (uint32_t)-1;
		}
	    }
	    space = t->isize - t->idata;
	    if (space == 0)
		goto realloc;
	}
	
	if (n < 0 && errno != EAGAIN) {
	    error = EIO;
	    goto fatal_error;
	}
    }

    if (events & MRP_IO_EVENT_HUP) {
	error = 0;
	goto closed;
    }
}


static int open_socket(tcp_t *t, int family)
{
    mrp_io_event_t events;

    t->sock = socket(family, SOCK_STREAM, 0);
    
    if (t->sock != -1) {
	events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
	t->iow = mrp_add_io_watch(t->ml, t->sock, events, tcp_recv_cb, t);
    
	if (t->iow != NULL)
	    return TRUE;
	else {
	    close(t->sock);
	    t->sock = -1;
	}
    }

    return FALSE;
}


static int tcp_connect(mrp_transport_t *mt, void *addrstr)
{
    tcp_t           *t = (tcp_t *)mt;
    struct sockaddr  addr;
    int              addrlen;
    int              reuse;
    long             nonblk;
    mrp_io_event_t   events;

    addrlen = mrp_transport_resolve(mt, addrstr, &addr, sizeof(addr));
    
    if (addrlen > 0) {
	t->sock = socket(addr.sa_family, SOCK_STREAM, 0);

	if (t->sock < 0)
	    return FALSE;

	if (connect(t->sock, &addr, addrlen) == 0) {
	    events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
	    t->iow = mrp_add_io_watch(t->ml, t->sock, events, tcp_recv_cb, t);
	    
	    if (t->iow != NULL) {
		reuse = 1;
		setsockopt(t->sock, SOL_SOCKET, SO_REUSEADDR,
			   &reuse, sizeof(reuse));
		nonblk = 1;
		fcntl(t->sock, F_SETFL, O_NONBLOCK, nonblk);

		t->connected = TRUE;

		return TRUE;
	    }
	}
    }
    
    if (t->sock != -1) {
	close(t->sock);
	t->sock = -1;
    }

    return FALSE;
}


static int tcp_disconnect(mrp_transport_t *mt)
{
    tcp_t *t = (tcp_t *)mt;

    if (t->connected) {
	mrp_del_io_watch(t->iow);
	t->iow = NULL;

	shutdown(t->sock, SHUT_RDWR);
	t->connected = FALSE;

	return TRUE;
    }
    else
	return FALSE;
}


static int tcp_send(mrp_transport_t *mt, mrp_msg_t *msg)
{
    tcp_t        *t = (tcp_t *)mt;
    struct iovec  iov[2];
    void         *buf;
    ssize_t       size, n;
    uint32_t      len;

    if (t->connected) {
	size = mrp_msg_default_encode(msg, &buf);
    
	if (size >= 0) {
	    len = htonl(size);
	    iov[0].iov_base = &len;
	    iov[0].iov_len  = sizeof(len);
	    iov[1].iov_base = buf;
	    iov[1].iov_len  = size;
	
	    n = writev(t->sock, iov, 2);
	    mrp_free(buf);

	    if (n == (ssize_t)(size + sizeof(len)))
		return TRUE;
	    else {
		if (n == -1 && errno == EAGAIN) {
		    mrp_log_error("%s(): XXX TODO: this sucks, need to add "
				  "output queuing for tcp-transport.",
				  __FUNCTION__);
		}
	    }
	}
    }

    return FALSE;
}


MRP_REGISTER_TRANSPORT("tcp", tcp_t, tcp_resolve,
		       tcp_open, tcp_bind, tcp_accept, tcp_close,
		       tcp_connect, tcp_disconnect,
		       tcp_send, NULL);
