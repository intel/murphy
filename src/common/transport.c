#include <string.h>

#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/transport.h>
#include <murphy/common/log.h>

static int check_destroy(mrp_transport_t *t);

static MRP_LIST_HOOK(transports);


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
				      mrp_transport_evt_t *evt, void *user_data)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if ((d = find_transport(type)) != NULL) {
	if ((t = mrp_allocz(d->size)) != NULL) {
	    t->ml        = ml;
	    t->req       = d->req;
	    t->evt       = *evt;
	    t->user_data = user_data;
	    
	    t->check_destroy = check_destroy;

	    if (!t->req.open(t)) {
		mrp_free(t);
		t = NULL;
	    }
	}
    }
    else
	t = NULL;
       
    return t;
}


socklen_t mrp_transport_resolve(mrp_transport_t *t, char *str, void *addr,
				socklen_t size)
{
    mrp_transport_descr_t *d;
    char                  *p, type[32];
    int                    n;

    if (t != NULL) {
	mrp_log_warning("%s@%s:%d: t != NULL but transport-relative resolving",
			__FUNCTION__, __FILE__, __LINE__);
	mrp_log_warning("is not implemented! Ignoring t (%p)...", t);
    }

    if ((p = strchr(str, ':')) != NULL && (n = p - str) < (int)sizeof(type)) {
	strncpy(type, str, n);
	type[n] = '\0';

	if ((d = find_transport(type)) != NULL)
	    return d->resolve(p + 1, addr, size);
    }

    return 0;
}


int mrp_transport_bind(mrp_transport_t *t, void *addr, socklen_t addrlen)
{
    if (t != NULL) {
	if (t->req.bind != NULL)
	    return t->req.bind(t, addr, addrlen);
	else
	    return TRUE;                  /* assume no binding is needed */
    }
    else
	return FALSE;
}


mrp_transport_t *mrp_transport_accept(mrp_mainloop_t *ml, const char *type,
				      void *conn, mrp_transport_evt_t *evt,
				      void *user_data)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if ((d = find_transport(type)) != NULL) {
	if ((t = mrp_allocz(d->size)) != NULL) {
	    t->ml        = ml;
	    t->req       = d->req;
	    t->evt       = *evt;
	    t->user_data = user_data;
	    
	    t->check_destroy = check_destroy;

	    if (!t->req.accept(t, conn)) {
		mrp_free(t);
		t = NULL;
	    }
	}
    }
    else
	t = NULL;
       
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
		t->req.disconnect(t);
		t->req.close(t);
	    });

	purge_destroyed(t);
    }
}


static int check_destroy(mrp_transport_t *t)
{
    return purge_destroyed(t);
}


int mrp_transport_connect(mrp_transport_t *t, void *addr)
{
    int result;
    
    if (!t->connected) {
	MRP_TRANSPORT_BUSY(t, {
		if (t->req.connect(t, addr))  {
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
		if (t->req.disconnect(t)) {
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
    
    if (t->connected && t->req.send) {
	MRP_TRANSPORT_BUSY(t, {
		result = t->req.send(t, msg);
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;

    return result;
}


int mrp_transport_sendto(mrp_transport_t *t, mrp_msg_t *msg, void *addr,
			 socklen_t addrlen)
{
    int result;
    
    if (!t->connected && t->req.sendto) {
	MRP_TRANSPORT_BUSY(t, {
		result = t->req.sendto(t, msg, addr, addrlen);
	    });

	purge_destroyed(t);
    }
    else
	result = FALSE;
	
    return result;
}


