#include <string.h>

#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/transport.h>
#include <murphy/common/log.h>

static int check_destroy(mrp_transport_t *t);

static MRP_LIST_HOOK(transports);

static inline int purge_destroyed(mrp_transport_t *t);


int mrp_transport_register(mrp_transport_descr_t *d)
{
    if (d->size >= sizeof(mrp_transport_t)) {
	mrp_list_init(&d->hook);
	mrp_list_append(&transports, &d->hook);
    
	return TRUE;
    }
    else
	return FALSE;
}


void mrp_transport_unregister(mrp_transport_descr_t *d)
{
    mrp_list_delete(&d->hook);
}


static mrp_transport_descr_t *find_transport(const char *type)
{
    mrp_transport_descr_t *d;
    mrp_list_hook_t       *p, *n;

    mrp_list_foreach(&transports, p, n) {
	d = mrp_list_entry(p, typeof(*d), hook);
	if (!strcmp(d->type, type))
	    return d;
    }

    return NULL;
}


mrp_transport_t *mrp_transport_create(mrp_mainloop_t *ml, const char *type,
				      mrp_transport_evt_t *evt, void *user_data,
				      int flags)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if ((d = find_transport(type)) != NULL) {
	if ((t = mrp_allocz(d->size)) != NULL) {
	    t->descr     = d;
	    t->ml        = ml;
	    t->evt       = *evt;
	    t->user_data = user_data;
	    
	    t->check_destroy = check_destroy;

	    if (!t->descr->req.open(t, flags)) {
		mrp_free(t);
		t = NULL;
	    }
	}
    }
    else
	t = NULL;
       
    return t;
}


mrp_transport_t *mrp_transport_create_from(mrp_mainloop_t *ml, const char *type,
					   void *conn, mrp_transport_evt_t *evt,
					   void *user_data, int flags,
					   int connected)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if ((d = find_transport(type)) != NULL) {
	if ((t = mrp_allocz(d->size)) != NULL) {
	    t->ml        = ml;
	    t->evt       = *evt;
	    t->user_data = user_data;
	    t->connected = connected;
	    
	    t->check_destroy = check_destroy;

	    if (!t->descr->req.create(t, conn, flags)) {
		mrp_free(t);
		t = NULL;
	    }
	}
    }
    else
	t = NULL;
       
    return t;
}


static inline int type_matches(const char *type, const char *addr)
{
    while (*type == *addr)
	type++, addr++;
    
    return (*type == '\0' && *addr == ':');
}


socklen_t mrp_transport_resolve(mrp_transport_t *t, const char *str,
				mrp_sockaddr_t *addr, socklen_t size,
				const char **type)
{
#if 1
    mrp_transport_descr_t *d;
    mrp_list_hook_t       *p, *n;
    socklen_t              l;
    
    if (t != NULL)
	return t->descr->resolve(str, addr, size);
    else {
	mrp_list_foreach(&transports, p, n) {
	    d = mrp_list_entry(p, typeof(*d), hook);
	    l = d->resolve(str, addr, size);
	    
	    if (l > 0) {
		if (type != NULL)
		    *type = d->type;
		return l;
	    }
	}
    }
    
    return 0;
#else
    mrp_transport_descr_t *d;
    char                  *p, type[32];
    int                    n;
    
    if ((p = strchr(str, ':')) != NULL && (n = p - str) < (int)sizeof(type)) {
	strncpy(type, str, n);
	type[n] = '\0';

	if (t != NULL)
	    return t->descr->resolve(p + 1, addr, size);
	else {
	    if ((d = find_transport(type)) != NULL)
		return d->resolve(p + 1, addr, size);
	}
    }

    return 0;
#endif
}


int mrp_transport_bind(mrp_transport_t *t, mrp_sockaddr_t *addr,
		       socklen_t addrlen)
{
    if (t != NULL) {
	if (t->descr->req.bind != NULL)
	    return t->descr->req.bind(t, addr, addrlen);
	else
	    return TRUE;                  /* assume no binding is needed */
    }
    else
	return FALSE;
}


int mrp_transport_listen(mrp_transport_t *t, int backlog)
{
    int result;
    
    if (t != NULL) {
	if (t->descr->req.listen != NULL) {
	    MRP_TRANSPORT_BUSY(t, {
		    result = t->descr->req.listen(t, backlog);
		});

	    purge_destroyed(t);
	    
	    return result;
	}
    }

    return FALSE;
}


mrp_transport_t *mrp_transport_accept(mrp_transport_t *lt,
				      mrp_transport_evt_t *evt,
				      void *user_data, int flags)
{
    mrp_transport_t *t;

    if ((t = mrp_allocz(lt->descr->size)) != NULL) {
	t->descr     = lt->descr;
	t->ml        = lt->ml;
	t->evt       = *evt;
	t->user_data = user_data;

	t->check_destroy = check_destroy;

	MRP_TRANSPORT_BUSY(t, {
		if (!t->descr->req.accept(t, lt, flags)) {
		    mrp_free(t);
		    t = NULL;
		}
	    });
    }

    return t;
}


static inline int purge_destroyed(mrp_transport_t *t)
{
    if (t->destroyed && !t->busy) {
	mrp_debug("destroying transport %p...", t);
	mrp_free(t);
	return TRUE;
    }
    else
	return FALSE;
}


void mrp_transport_destroy(mrp_transport_t *t)
{
    if (t != NULL) {
	t->destroyed = TRUE;
	
	MRP_TRANSPORT_BUSY(t, {
		t->descr->req.disconnect(t);
		t->descr->req.close(t);
	    });

	purge_destroyed(t);
    }
}


static int check_destroy(mrp_transport_t *t)
{
    return purge_destroyed(t);
}


int mrp_transport_connect(mrp_transport_t *t, mrp_sockaddr_t *addr,
			  socklen_t addrlen)
{
    int result;
    
    if (!t->connected) {
	MRP_TRANSPORT_BUSY(t, {
		if (t->descr->req.connect(t, addr, addrlen))  {
		    t->connected = TRUE;
		    result       = TRUE;
		}
		else
		    result = FALSE;
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;

    return result;
}


int mrp_transport_disconnect(mrp_transport_t *t)
{
    int result;
    
    if (t->connected) {
	MRP_TRANSPORT_BUSY(t, {
		if (t->descr->req.disconnect(t)) {
		    t->connected = FALSE;
		    result       = TRUE;
		}
		else
		    result = TRUE;
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;

    return result;
}


int mrp_transport_send(mrp_transport_t *t, mrp_msg_t *msg)
{
    int result;
    
    if (t->connected && t->descr->req.send) {
	MRP_TRANSPORT_BUSY(t, {
		result = t->descr->req.send(t, msg);
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;

    return result;
}


int mrp_transport_sendto(mrp_transport_t *t, mrp_msg_t *msg,
			 mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;
    
    if (/*!t->connected && */t->descr->req.sendto) {
	MRP_TRANSPORT_BUSY(t, {
		result = t->descr->req.sendto(t, msg, addr, addrlen);
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;
	
    return result;
}


