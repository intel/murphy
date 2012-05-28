#ifndef __MURPHY_TRANSPORT_H__
#define __MURPHY_TRANSPORT_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/msg.h>

typedef struct mrp_transport_s mrp_transport_t;


/*
 * Notes:
 *
 *    Transports can get destructed in two slightly different ways.
 *
 *    1)
 *      Someone calls mrp_transport_destroy while the transport is
 *      idle, ie. with no callbacks or operations being active. This
 *      is simple and straightforward:
 *         - mrp_transport_destroy calls req.disconnect
 *         - mrp_transport_destroy calls req.close
 *         - mrp_transport_destroy check and sees the transport is idle
 *           so it frees the transport
 *
 *    2)
 *      Someone calls mrp_tansport_destroy while the transport is
 *      busy, ie. it has an unfinished callback or operation running.
 *      This typically happens when an operation or callback function,
 *      or a user function called from either of those calls
 *      mrp_transport_destroy as a result of a received message, or a
 *      (communication) error. In this case destroying the transport
 *      is less straightforward and needs to get delayed to avoid
 *      shooting out the transport underneath the active operation or
 *      callback.
 *
 *    To handle the latter case, the generic (ie. top-level) transport 
 *    layer has a member function check_destroy. This function checks
 *    for pending destroy requests and destroys the transport if it
 *    is not busy. All transport backends MUST CALL this function and
 *    CHECK ITS RETURN VALUE, whenever a user callback or a transport
 *    callback (ie. bottom-up event propagation) function invoked by
 *    the backend returns.
 *
 *    If the transport has been left intact, check_destroy returns
 *    FALSE and processing can continue normally, taking into account
 *    that any transport state stored locally in the stack frame of the
 *    backend function might have changed during the callback. However,
 *    if check_destroy returns TRUE, it has nuked the transport and the
 *    backend MUST NOT touch or try to dereference the transport any more
 *    as its resources have already been released.
 */


/*
 * transport socket address
 */

typedef union {
    struct sockaddr     any;
    struct sockaddr_in  ipv4;
    struct sockaddr_in6 ipv6;
    struct sockaddr_un  unx;
} mrp_sockaddr_t;


static inline mrp_sockaddr_t *mrp_sockaddr_cpy(mrp_sockaddr_t *d,
					       mrp_sockaddr_t *s, socklen_t n)
{
    memcpy(d, s, n);
    return d;
}


/*
 * various transport flags
 */

typedef enum {
    MRP_TRANSPORT_REUSEADDR = 0x1,
    MRP_TRANSPORT_NONBLOCK  = 0x2,
    MRP_TRANSPORT_CLOEXEC   = 0x4,

    MRP_TRANSPORT_MODE_MSG    = 0x00000000, /* in generic mode */
    MRP_TRANSPORT_MODE_RAW    = 0x10000000, /* in bitpipe mode */
    MRP_TRANSPORT_MODE_CUSTOM = 0x20000000, /* in custom type mode */
    MRP_TRANSPORT_MODE_MASK   = 0x30000000, /* mask for  transport mode */

    MRP_TRANSPORT_INHERIT     = 0x30000000, /* mask of inherited flags */
} mrp_transport_flag_t;

#define MRP_TRANSPORT_MODE(t) ((t)->flags & MRP_TRANSPORT_MODE_MASK)

/*
 * transport requests
 *
 * Transport requests correspond to top-down event propagation in the
 * communication stack. These requests are made by the core tansport
 * abstraction layer to the underlying actual transport implementation
 * to carry out the implementation-specific details of some transport
 * operation.
 */

typedef struct {
    /** Open a new transport. */
    int  (*open)(mrp_transport_t *t);
    /** Create a new transport from an existing backend object. */
    int  (*createfrom)(mrp_transport_t *t, void *obj);
    /** Bind a transport to a given transport-specific address. */
    int  (*bind)(mrp_transport_t *t, mrp_sockaddr_t *addr, socklen_t addrlen);
    /** Listen on a transport for incoming connections. */
    int  (*listen)(mrp_transport_t *t, int backlog);
    /** Accept a new transport connection over an existing transport. */
    int  (*accept)(mrp_transport_t *t, mrp_transport_t *lt);
    /** Connect a transport to an endpoint. */
    int  (*connect)(mrp_transport_t *t, mrp_sockaddr_t *addr,
		    socklen_t addrlen);
    /** Disconnect a transport, if it is connection-oriented. */
    int  (*disconnect)(mrp_transport_t *t);
    /** Close a transport, free all resources from open/accept/connect. */
    void (*close)(mrp_transport_t *t);
    /** Send a message over a (connected) transport. */
    int (*sendmsg)(mrp_transport_t *t, mrp_msg_t *msg);
    /** Send raw data over a (connected) transport. */
    int (*sendraw)(mrp_transport_t *t, void *buf, size_t size);
    /** Send custom data over a (connected) transport. */
    int (*senddata)(mrp_transport_t *t, void *data, uint16_t tag);

    /** Send a message over a(n unconnected) transport. */
    int (*sendmsgto)(mrp_transport_t *t, mrp_msg_t *msg, mrp_sockaddr_t *addr,
		     socklen_t addrlen);
    /** Send raw data over a(n unconnected) transport. */
    int (*sendrawto)(mrp_transport_t *t, void *buf, size_t size,
		     mrp_sockaddr_t *addr, socklen_t addrlen);
    /** Send custom data over a(n unconnected) transport. */
    int (*senddatato)(mrp_transport_t *t, void *data, uint16_t tag,
		      mrp_sockaddr_t *addr, socklen_t addrlen);
} mrp_transport_req_t;


/*
 * transport events
 *
 * Transport events correspond to bottom-up event propagation in the
 * communication stack. These callbacks are made by the actual transport
 * implementation to the generic transport abstraction to inform it
 * about relevant transport events, such as the reception of data, or
 * transport disconnection by the peer.
 */

typedef struct {
    /** Message received on a connected transport. */
    union {
	/** Generic message callback for connected transports. */
	void (*recvmsg)(mrp_transport_t *t, mrp_msg_t *msg, void *user_data);
	/** Raw data callback for connected transports. */
	void (*recvraw)(mrp_transport_t *t, void *data, size_t size,
			void *user_data);
	/** Custom data callback for connected transports. */
	void (*recvdata)(mrp_transport_t *t, void *data, uint16_t tag,
			 void *user_data);
    };
    
    /** Message received on an unconnected transport. */
    union {
	/** Generic message callback for unconnected transports. */
	void (*recvmsgfrom)(mrp_transport_t *t, mrp_msg_t *msg,
			    mrp_sockaddr_t *addr, socklen_t addrlen,
			    void *user_data);
	/** Raw data callback for unconnected transports. */
	void (*recvrawfrom)(mrp_transport_t *t, void *data, size_t size,
			    mrp_sockaddr_t *addr, socklen_t addrlen,
			    void *user_data);
	/** Custom data callback for unconnected transports. */
	void (*recvdatafrom)(mrp_transport_t *t, void *data, uint16_t tag,
			     mrp_sockaddr_t *addr, socklen_t addrlen,
			     void *user_data);
    };
    /** Connection closed by peer. */
    void (*closed)(mrp_transport_t *t, int error, void *user_data);
    /** Connection attempt on a socket being listened on. */
    void (*connection)(mrp_transport_t *t, void *user_data);
} mrp_transport_evt_t;


/*
 * transport descriptor
 */

typedef struct {
    const char          *type;           /* transport type name */
    size_t               size;           /* full transport struct size */
    mrp_transport_req_t  req;            /* transport requests */
    socklen_t          (*resolve)(const char *str, mrp_sockaddr_t *addr,
				  socklen_t addrlen, const char **typep);
    mrp_list_hook_t      hook;           /* to list of registered transports */
} mrp_transport_descr_t;


/*
 * transport
 */

#define MRP_TRANSPORT_PUBLIC_FIELDS					\
    mrp_mainloop_t          *ml;					\
    mrp_transport_descr_t   *descr;					\
    mrp_transport_evt_t      evt;					\
    int                    (*check_destroy)(mrp_transport_t *t);	\
    int                    (*recv_data)(mrp_transport_t *t, void *data,	\
					size_t size,			\
					mrp_sockaddr_t *addr,		\
					socklen_t addrlen);		\
    void                    *user_data;					\
    int                      flags;					\
    int                      busy;					\
    int                      connected : 1;				\
    int                      listened : 1;				\
    int                      destroyed : 1				\
    

struct mrp_transport_s {
    MRP_TRANSPORT_PUBLIC_FIELDS;
};



/*
 * convenience macros
 */

/**
 * Macro to mark a transport busy while running a block of code.
 *
 * The backend needs to make sure the transport is not freed while a
 * transport request or event callback function is active. Similarly,
 * the backend needs to check if the transport has been marked for
 * destruction whenever an event callback returns and trigger the
 * destruction if it is necessary and possible (ie. the above criterium
 * of not being active is fullfilled).
 *
 * These are the easiest to accomplish using the provided MRP_TRANSPORT_BUSY
 * macro and the check_destroy callback member provided by mrp_transport_t.
 * 
 *     1) Use the provided MRP_TRANSPORT_BUSY macro to enclose al blocks of
 *        code that invoke event callbacks. Do not do a return directly
 *        from within the enclosed call blocks, rather just set a flag
 *        within the block, check it after the block and do the return
 *        from there if necessary.
 *
 *     2) Call mrp_transport_t->check_destroy after any call to an event
 *        callback. check_destroy will check for any pending destroy
 *        request and perform the actual destruction if it is necessary
 *        and possible. If the transport has been left intact, check_destroy
 *        returns FALSE. However, if the transport has been destroyed and
 *        freed it returns TRUE, in which case the caller must not attempt
 *        to use or dereference the transport data structures any more.
 */


#ifndef __MRP_TRANSPORT_DISABLE_CODE_CHECK__
#  define __TRANSPORT_CHK_BLOCK(...) do {				\
	static int __warned = 0;					\
									\
    if (MRP_UNLIKELY(__warned == 0 &&					\
		     strstr(#__VA_ARGS__, "return") != NULL)) {		\
	mrp_log_error("********************* WARNING *********************"); \
	mrp_log_error("* You seem to directly do a return from a block   *"); \
	mrp_log_error("* of code protected by MRP_TRANSPORT_BUSY. Are    *"); \
	mrp_log_error("* you absolutely sure you know what you are doing *"); \
	mrp_log_error("* and that you are also doing it correctly ?      *"); \
	mrp_log_error("***************************************************"); \
	mrp_log_error("The suspicious code block is located at: ");	     \
	mrp_log_error("  %s@%s:%d", __FUNCTION__, __FILE__, __LINE__);	     \
	mrp_log_error("and it looks like this:");			     \
	mrp_log_error("---------------------------------------------");	     \
	mrp_log_error("%s", #__VA_ARGS__);				     \
	mrp_log_error("---------------------------------------------");	     \
	mrp_log_error("If you understand what MRP_TRANSPORT_BUSY does and"); \
	mrp_log_error("how, and you are sure about the corretness of your"); \
	mrp_log_error("code you can disable this error message by");	     \
	mrp_log_error("#defining __MRP_TRANSPORT_DISABLE_CODE_CHECK__");     \
	mrp_log_error("when compiling %s.", __FILE__);			     \
	__warned = 1;							     \
    }									     \
 } while (0)
#else
#  define __TRANSPORT_CHK_BLOCK(...) do { } while (0)
#endif

#define MRP_TRANSPORT_BUSY(t, ...) do {		\
	__TRANSPORT_CHK_BLOCK(__VA_ARGS__);	\
	(t)->busy++;				\
	__VA_ARGS__				\
        (t)->busy--;			        \
    } while (0)



/** Automatically register a transport on startup. */
#define MRP_REGISTER_TRANSPORT(_prfx, _typename, _structtype, _resolve,	\
			       _open, _createfrom, _close,		\
			       _bind, _listen, _accept,			\
			       _connect, _disconnect,			\
			       _sendmsg, _sendmsgto,			\
			       _sendraw, _sendrawto,			\
			       _senddata, _senddatato)			\
    static void _prfx##_register_transport(void)			\
	 __attribute__((constructor));					\
    									\
    static void _prfx##_register_transport(void) {			\
	static mrp_transport_descr_t descriptor = {			\
	    .type    = _typename,					\
	    .size    = sizeof(_structtype),				\
	    .resolve = _resolve,					\
	    .req     = {						\
		.open       = _open,					\
	        .createfrom = _createfrom,				\
		.bind       = _bind,					\
		.listen     = _listen,					\
		.accept     = _accept,					\
		.close      = _close,					\
		.connect    = _connect,					\
		.disconnect = _disconnect,				\
		.sendmsg    = _sendmsg,					\
		.sendmsgto  = _sendmsgto,				\
		.sendraw    = _sendraw,					\
		.sendrawto  = _sendrawto,				\
		.senddata   = _senddata,				\
		.senddatato = _senddatato,				\
	    },								\
	};								\
									\
	if (!mrp_transport_register(&descriptor))			\
	    mrp_log_error("Failed to register transport '%s'.",		\
			  _typename);					\
	else								\
	    mrp_log_info("Registered transport '%s'.", _typename);	\
    }									\
    struct mrp_allow_trailing_semicolon



/** Register a new transport type. */
int mrp_transport_register(mrp_transport_descr_t *d);

/** Unregister a transport. */
void mrp_transport_unregister(mrp_transport_descr_t *d);

/** Create a new transport. */
mrp_transport_t *mrp_transport_create(mrp_mainloop_t *ml, const char *type,
				      mrp_transport_evt_t *evt,
				      void *user_data, int flags);

/** Create a new transport from a backend object. */
mrp_transport_t *mrp_transport_create_from(mrp_mainloop_t *ml, const char *type,
					   void *conn, mrp_transport_evt_t *evt,
					   void *user_data, int flags,
					   int connected);

/** Resolve an address string to a transport-specific address. */
socklen_t mrp_transport_resolve(mrp_transport_t *t, const char *str,
				mrp_sockaddr_t *addr, socklen_t addrlen,
				const char **type);

/** Bind a given transport to a transport-specific address. */
int mrp_transport_bind(mrp_transport_t *t, mrp_sockaddr_t *addr,
		       socklen_t addrlen);

/** Listen for incoming connection on the given transport. */
int  mrp_transport_listen(mrp_transport_t *t, int backlog);

/** Accept and create a new transport connection. */
mrp_transport_t *mrp_transport_accept(mrp_transport_t *t,
				      void *user_data, int flags);

/** Destroy a transport. */
void mrp_transport_destroy(mrp_transport_t *t);

/** Connect a transport to the given address. */
int mrp_transport_connect(mrp_transport_t *t, mrp_sockaddr_t  *addr,
			  socklen_t addrlen);

/** Disconnect a transport. */
int mrp_transport_disconnect(mrp_transport_t *t);

/** Send a message through the given (connected) transport. */
int mrp_transport_send(mrp_transport_t *t, mrp_msg_t *msg);

/** Send a message through the given transport to the remote address. */
int mrp_transport_sendto(mrp_transport_t *t, mrp_msg_t *msg,
			 mrp_sockaddr_t *addr, socklen_t addrlen);

/** Send raw data through the given (connected) transport. */
int mrp_transport_sendraw(mrp_transport_t *t, void *data, size_t size);

/** Send raw data through the given transport to the remote address. */
int mrp_transport_sendrawto(mrp_transport_t *t, void *data, size_t size,
			    mrp_sockaddr_t *addr, socklen_t addrlen);

/** Send custom data through the given (connected) transport. */
int mrp_transport_senddata(mrp_transport_t *t, void *data, uint16_t tag);

/** Send custom data through the given transport to the remote address. */
int mrp_transport_senddatato(mrp_transport_t *t, void *data, uint16_t tag,
			     mrp_sockaddr_t *addr, socklen_t addrlen);

#endif /* __MURPHY_TRANSPORT_H__ */
