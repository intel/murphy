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

#define DEFAULT_SIZE 1024                   /* default input buffer size */

typedef struct {
    MRP_TRANSPORT_PUBLIC_FIELDS;         /* common transport fields */
    int             sock;                /* UDP socket */
    int             family;              /* socket family */
    mrp_io_watch_t *iow;                 /* socket I/O watch */
    void           *ibuf;                /* input buffer */
    size_t          isize;               /* input buffer size */
    size_t          idata;               /* amount of input data */
} udp_t;


static void udp_recv_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			mrp_io_event_t events, void *user_data);
static int udp_disconnect(mrp_transport_t *mu);
static int open_socket(udp_t *u, int family);


static socklen_t udp_resolve(char *str, void *addr, socklen_t size)
{
    struct addrinfo *ai, hints;
    char             node[512], *port;
    
    mrp_clear(&hints);    
    hints.ai_family = AF_UNSPEC;
    ai              = NULL;

    if      (!strncmp(str, "udp:" , 4)) str += 4;
    else if (!strncmp(str, "udp4:", 5)) str += 5, hints.ai_family = AF_INET;
    else if (!strncmp(str, "udp6:", 5)) str += 5, hints.ai_family = AF_INET6;
    
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


static int udp_open(mrp_transport_t *mu)
{
    udp_t *u = (udp_t *)mu;
    
    u->sock   = -1;
    u->family = -1;

    return TRUE;
}


static int udp_bind(mrp_transport_t *mu, void *addr, socklen_t addrlen)
{
    udp_t *u = (udp_t *)mu;
    
    if (u->sock != -1 || !u->connected) {
	if (open_socket(u, ((struct sockaddr *)addr)->sa_family))
	    if (bind(u->sock, (struct sockaddr *)addr, addrlen) == 0)
		return TRUE;
    }

    return FALSE;
}


static void udp_close(mrp_transport_t *mu)
{
    udp_t *u = (udp_t *)mu;

    mrp_del_io_watch(u->iow);
    u->iow = NULL;

    mrp_free(u->ibuf);
    u->ibuf  = NULL;
    u->isize = 0;
    u->idata = 0;
    
    if (u->sock >= 0){
	close(u->sock);
	u->sock = -1;
    }
}


static void udp_recv_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			 mrp_io_event_t events, void *user_data)
{
    udp_t           *u  = (udp_t *)user_data;
    mrp_transport_t *mu = (mrp_transport_t *)u;
    struct sockaddr  addr;
    socklen_t        addrlen;
    uint32_t         size;
    ssize_t          n;
    void            *data;
    int              old, error;
    mrp_msg_t       *msg;

    MRP_UNUSED(ml);
    MRP_UNUSED(w);

    if (events & MRP_IO_EVENT_IN) {
	if (u->idata == u->isize) {
	    if (u->isize != 0) {
		old      = u->isize;
		u->isize *= 2;
	    }
	    else {
		old      = 0;
		u->isize = DEFAULT_SIZE;
	    }
	    if (!mrp_reallocz(u->ibuf, old, u->isize)) {
		error = ENOMEM;
	    fatal_error:
	    closed:
		udp_disconnect(mu);
		
		if (u->evt.closed != NULL)
		    MRP_TRANSPORT_BUSY(mu, {
			    mu->evt.closed(mu, error, mu->user_data);
			});
		
		u->check_destroy(mu);
		return;
	    }
	}

	if (recv(fd, &size, sizeof(size), MSG_PEEK) != sizeof(size)) {
	    error = EIO;
	    goto fatal_error;
	}
	
	size = ntohl(size);

	if (u->isize < size + sizeof(size)) {
	    old      = u->isize;
	    u->isize = size + sizeof(size);
	    
	    if (!mrp_reallocz(u->ibuf, old, u->isize)) {
		error = ENOMEM;
		goto fatal_error;
	    }
	}

	addrlen = sizeof(addr);
	n = recvfrom(fd, u->ibuf, size + sizeof(size), 0, &addr, &addrlen);
	
	if (n != (ssize_t)(size + sizeof(size))) {
	    error = n < 0 ? EIO : EPROTO;
	    goto fatal_error;
	}
	
	data = u->ibuf + sizeof(size);
	msg  = mrp_msg_default_decode(data, size);
	
	if (msg != NULL) {
	    if (mu->connected) {
		MRP_TRANSPORT_BUSY(mu, {
			mu->evt.recv(mu, msg, mu->user_data);
		    });
	    }
	    else {
		MRP_TRANSPORT_BUSY(mu, {
			mu->evt.recvfrom(mu, msg, &addr, addrlen,
					 mu->user_data);
		    });
	    }

	    mrp_msg_unref(msg);

	    if (u->check_destroy(mu))
		return;
	}
	else {
	    error = EPROTO;
	    goto fatal_error;
	}
    }

    if (events & MRP_IO_EVENT_HUP) {
	error = 0;
	goto closed;
    }
}


static int open_socket(udp_t *u, int family)
{
    mrp_io_event_t events;

    u->sock = socket(family, SOCK_DGRAM, 0);
    
    if (u->sock != -1) {
	events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
	u->iow = mrp_add_io_watch(u->ml, u->sock, events, udp_recv_cb, u);
    
	if (u->iow != NULL)
	    return TRUE;
	else {
	    close(u->sock);
	    u->sock = -1;
	}
    }

    return FALSE;
}


static int udp_connect(mrp_transport_t *mu, void *addrstr)
{
    udp_t           *u = (udp_t *)mu;
    struct sockaddr  addr;
    int              addrlen;
    int              reuse;
    long             nonblk;

    addrlen = mrp_transport_resolve(mu, addrstr, &addr, sizeof(addr));
    
    if (addrlen > 0) {
	if (MRP_UNLIKELY(u->family != -1 && u->family != addr.sa_family))
	    return FALSE;

	if (MRP_UNLIKELY(u->sock == -1)) {
	    if (!open_socket(u, addr.sa_family))
		return FALSE;
	}

	if (connect(u->sock, &addr, addrlen) == 0) {
	    reuse = 1;
	    setsockopt(u->sock, SOL_SOCKET, SO_REUSEADDR,
		       &reuse, sizeof(reuse));
	    nonblk = 1;
	    fcntl(u->sock, F_SETFL, O_NONBLOCK, nonblk);

	    u->connected = TRUE;

	    return TRUE;
	}
    }
    
    return FALSE;
}


static int udp_disconnect(mrp_transport_t *mu)
{
    udp_t *u = (udp_t *)mu;

    if (u->connected) {
	mrp_del_io_watch(u->iow);
	u->iow = NULL;

	shutdown(u->sock, SHUT_RDWR);
	u->connected = FALSE;

	return TRUE;
    }
    else
	return FALSE;
}


static int udp_send(mrp_transport_t *mu, mrp_msg_t *msg)
{
    udp_t        *u = (udp_t *)mu;
    struct iovec  iov[2];
    void         *buf;
    ssize_t       size, n;
    uint32_t      len;

    if (u->connected) {
	size = mrp_msg_default_encode(msg, &buf);
    
	if (size >= 0) {
	    len = htonl(size);
	    iov[0].iov_base = &len;
	    iov[0].iov_len  = sizeof(len);
	    iov[1].iov_base = buf;
	    iov[1].iov_len  = size;
	
	    n = writev(u->sock, iov, 2);
	    mrp_free(buf);

	    if (n == (ssize_t)(size + sizeof(len)))
		return TRUE;
	    else {
		if (n == -1 && errno == EAGAIN) {
		    mrp_log_error("%s(): XXX TODO: this sucks, need to add "
				  "output queuing for udp-transport.",
				  __FUNCTION__);
		}
	    }
	}
    }

    return FALSE;
}


static int udp_sendto(mrp_transport_t *mu, mrp_msg_t *msg, void *addr,
		      socklen_t addrlen)
{
    udp_t           *u = (udp_t *)mu;
    struct iovec     iov[2];
    void            *buf;
    ssize_t          size, n;
    uint32_t         len;
    struct msghdr    hdr;

    if (MRP_UNLIKELY(u->sock == -1)) {
	if (!open_socket(u, ((struct sockaddr *)addr)->sa_family))
	    return FALSE;
    }
	
    size = mrp_msg_default_encode(msg, &buf);
    
    if (size >= 0) {
	len = htonl(size);
	iov[0].iov_base = &len;
	iov[0].iov_len  = sizeof(len);
	iov[1].iov_base = buf;
	iov[1].iov_len  = size;
	
	hdr.msg_name    = addr;
	hdr.msg_namelen = addrlen;
	hdr.msg_iov     = iov;
	hdr.msg_iovlen  = MRP_ARRAY_SIZE(iov);
	
	hdr.msg_control    = NULL;
	hdr.msg_controllen = 0;
	hdr.msg_flags      = 0;
	    
	n = sendmsg(u->sock, &hdr, 0);
	mrp_free(buf);
	
	if (n == (ssize_t)(size + sizeof(len)))
	    return TRUE;
	else {
	    if (n == -1 && errno == EAGAIN) {
		mrp_log_error("%s(): XXX TODO: udp-transport send failed",
			      __FUNCTION__);
	    }
	}
    }
    
    return FALSE;
}


MRP_REGISTER_TRANSPORT("udp", udp_t, udp_resolve,
		       udp_open, udp_bind, NULL, udp_close,
		       udp_connect, udp_disconnect,
		       udp_send, udp_sendto);
