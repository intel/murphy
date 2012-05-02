#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/msg.h>

#define AVG_SPACE_PER_FIELD 32           /* guesstimate for tag + data */
#define len_t               uint32_t
#define MSG_ALIGN           sizeof(len_t)

static inline int msg_add(mrp_msg_t *msg, const char *tag, void *data,
			  size_t size, int prepend);
static void msg_destroy(mrp_msg_t *msg);

mrp_msg_t *mrp_msg_create(const char *tag, ...)
{
    mrp_msg_t *msg;
    va_list    ap;
    void      *data;
    size_t     size;
    
    va_start(ap, tag);
    if ((msg = mrp_allocz(sizeof(*msg))) != NULL) {
	mrp_list_init(&msg->fields);
	
	while (tag != NULL) {
	    data = va_arg(ap, typeof(data));
	    size = va_arg(ap, typeof(size));
	    
	    if (!msg_add(msg, tag, data, size, FALSE)) {
		msg_destroy(msg);
		msg = NULL;

		goto out;
	    }

	    tag = va_arg(ap, typeof(tag));
	}
    }

    msg->refcnt = 1;

 out:
    va_end(ap);
    
    return msg;
}


static void msg_destroy(mrp_msg_t *msg)
{
    mrp_list_hook_t *p, *n;
    mrp_msg_field_t *f;

    if (msg != NULL) {
#ifdef __MSG_EXTRA_CHECKS__
	if (MRP_UNLIKELY(msg->refcnt) != 0) {
	    mrp_log_error("%s() called for message (%p) with refcnt %d...",
			  __FUNCTION__, msg, msg->refcnt);
	}
#endif

	mrp_debug("destroying message %p...", msg);

	mrp_list_foreach(&msg->fields, p, n) {
	    f = mrp_list_entry(p, typeof(*f), hook);
	    mrp_list_delete(p);

	    mrp_free(f->tag);
	    mrp_free(f->data);
	    mrp_free(f);
	}
	
	mrp_free(msg);
    }
    
}


mrp_msg_t *mrp_msg_ref(mrp_msg_t *msg)
{
    if (msg != NULL)
	msg->refcnt++;
    
    return msg;
}


void mrp_msg_unref(mrp_msg_t *msg)
{
    if (msg != NULL) {
	msg->refcnt--;

	if (msg->refcnt <= 0)
	    msg_destroy(msg);
    }
}


static inline int msg_add(mrp_msg_t *msg, const char *tag, void *data,
			  size_t size, int prepend)
{
    mrp_msg_field_t *f;

    if ((f = mrp_allocz(sizeof(*f))) != NULL) {
	f->tag  = mrp_strdup(tag);
	f->data = mrp_datadup(data, size);
	f->size = size;

	if (f->tag != NULL && f->data != NULL) {
	    mrp_list_init(&f->hook);
	    if (!prepend)
		mrp_list_append(&msg->fields, &f->hook);
	    else
		mrp_list_prepend(&msg->fields, &f->hook);
	    msg->nfield++;
	    
	    return TRUE;
	}
	else {
	    mrp_free(f->tag);
	    mrp_free(f->data);
	    mrp_free(f);
	}
    }

    return FALSE;
}


int mrp_msg_append(mrp_msg_t *msg, char *tag, void *data, size_t size)
{
    return msg_add(msg, tag, data, size, FALSE);
}


int mrp_msg_prepend(mrp_msg_t *msg, char *tag, void *data, size_t size)
{
    return msg_add(msg, tag, data, size, TRUE);
}


void *mrp_msg_find(mrp_msg_t *msg, char *tag, size_t *size)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&msg->fields, p, n) {
	f = mrp_list_entry(p, typeof(*f), hook);

	if (!strcmp(f->tag, tag)) {
	    *size = f->size;
	    return f->data;
	}
    }

    *size = 0;
    return NULL;
}

int mrp_msg_dump(mrp_msg_t *msg, FILE *fp)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;
    int              l;

    l = fprintf(fp, "{\n");
    mrp_list_foreach(&msg->fields, p, n) {
	f  = mrp_list_entry(p, typeof(*f), hook);
	l += fprintf(fp, "    %s='%.*s' (%zd bytes)\n",
		     f->tag, (int)f->size, (char *)f->data, f->size);
    }
    l += fprintf(fp, "}\n");

    return l;
}


ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp)
{
#define ENSURE_SPACE(needed) do {				\
	int _miss = needed - l;					\
	if (MRP_UNLIKELY(_miss > 0)) {				\
	    p -= (ptrdiff_t)buf;				\
	    size += _miss * 2;					\
	    if (mrp_realloc(buf, size) == NULL) {		\
		mrp_free(buf);					\
		*bufp = NULL;					\
		return -1;					\
	    }							\
	    else						\
		p    += (ptrdiff_t)buf;				\
	}							\
    } while (0)

    mrp_msg_field_t *f;
    mrp_list_hook_t *pf, *nf;
    void            *buf, *p;
    len_t            tsize, *sizep;
    size_t           size, nfield, pad, extra, fsize, tpad, dpad;
    int              l;

    nfield = msg->nfield;
    extra  = nfield * 2 * sizeof(uint32_t);
    pad    = nfield * 2 * 3;
    size   = sizeof(len_t) + nfield * AVG_SPACE_PER_FIELD + extra + pad;

    if ((buf = mrp_alloc(size)) != NULL) {
	p = buf;
	l = size;
	
	/* encode number of fields */
	sizep = p;
	*sizep = htonl(msg->nfield);
	p     += sizeof(*sizep);
	l     -= sizeof(*sizep);
	
	mrp_list_foreach(&msg->fields, pf, nf) {
	    f = mrp_list_entry(pf, typeof(*f), hook);

	    /* make space for field if needed */
	    tsize  = strlen(f->tag) + 1;
	    tpad   = (MSG_ALIGN - (tsize   & (MSG_ALIGN-1))) & (MSG_ALIGN-1);
	    dpad   = (MSG_ALIGN - (f->size & (MSG_ALIGN-1))) & (MSG_ALIGN-1);
	    
	    fsize  = sizeof(*sizep) + tsize   + tpad;
	    fsize += sizeof(*sizep) + f->size + dpad;
	    ENSURE_SPACE(fsize);

	    /* tag size and tag */
	    sizep  = p;
	    *sizep = htonl(tsize + tpad);
	    p     += sizeof(*sizep);
	    l     -= sizeof(*sizep);
	    memcpy(p, f->tag, tsize);
	    memset(p + tsize, 0, tpad);
	    p     += tsize + tpad;
	    l     -= tsize + tpad;

	    /* data size and data */
	    sizep  = p;
	    *sizep = htonl(f->size + dpad);
	    p     += sizeof(*sizep);
	    l     -= sizeof(*sizep);
	    memcpy(p, f->data, f->size);
	    memset(p + f->size, 0, dpad);
	    p     += f->size + dpad;
	    l     -= f->size + dpad;
	}
    
	size = p - buf;
	*bufp = buf;
    }
    else {
	*bufp = NULL;
	size = -1;
    }

    return size;

#undef ENSURE_SPACE
}


mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size)
{
#define ENSURE_DATA(n) do {			\
	if (MRP_UNLIKELY((int)(n) > (int)l)) {	\
	    msg_destroy(msg);			\
	    return NULL;			\
	}					\
    } while (0)
    
    mrp_msg_t *msg;
    len_t     *sizep;
    int        nfield, l, i;
    char      *tag;
    void      *p, *data;
    size_t     n;

    if ((msg = mrp_msg_create(NULL)) != NULL) {
	p = buf;
	l = size;
	
	/* get number of fields */
	ENSURE_DATA(sizeof(*sizep));
	sizep   = p;
	nfield  = ntohl(*sizep);
	p      += sizeof(*sizep);
	l      -= sizeof(*sizep);

	/* decode fields */
	for (i = 0; i < nfield; i++) {
	    /* get tag size and tag */
	    ENSURE_DATA(sizeof(*sizep));
	    sizep  = p;
	    p     += sizeof(*sizep);
	    l     -= sizeof(*sizep);
	    n      = ntohl(*sizep);
	    ENSURE_DATA(MRP_ALIGN(n, MSG_ALIGN));
	    tag    = p;
	    /* get data size and data */
	    p     += MRP_ALIGN(n, MSG_ALIGN);
	    l     -= MRP_ALIGN(n, MSG_ALIGN);
	    ENSURE_DATA(sizeof(*sizep));
	    sizep  = p;
	    p     += sizeof(*sizep);
	    l     -= sizeof(*sizep);
	    n      = ntohl(*sizep);
	    ENSURE_DATA(MRP_ALIGN(n, MSG_ALIGN));
	    data   = p;

	    if (!mrp_msg_append(msg, tag, data, n)) {
		msg_destroy(msg);

		return NULL;
	    }
		

	    p += MRP_ALIGN(n, MSG_ALIGN);
	    l -= MRP_ALIGN(n, MSG_ALIGN);
	}

	if (MRP_UNLIKELY(l != 0)) {
	    msg_destroy(msg);
	    msg = NULL;
	}
    }

    return msg;

#undef ENSURE_DATA
}
