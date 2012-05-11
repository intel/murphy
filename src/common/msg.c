#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/msg.h>


static inline mrp_msg_field_t *create_field(uint16_t tag, va_list *ap)
{
    mrp_msg_field_t *f;
    uint16_t         type;
    uint32_t         size;
    void            *blb;

    if ((f = mrp_allocz(sizeof(*f))) != NULL) {
	mrp_list_init(&f->hook);
	type = va_arg(*ap, uint32_t);
	
#define CREATE(_f, _tag, _type, _fldtype, _fld, _last, _errlbl) do {	\
	    (_f) = mrp_allocz(MRP_OFFSET(typeof(*_f), _last) +		\
			      sizeof(_f->_last));			\
	    								\
	    if ((_f) != NULL) {						\
		(_f)->tag  = _tag;					\
		(_f)->type = _type;					\
		(_f)->_fld = va_arg(*ap, _fldtype);			\
	    }								\
	    else {							\
		goto _errlbl;						\
	    }								\
	} while (0)

	switch (type) {
	case MRP_MSG_FIELD_STRING:
	    CREATE(f, tag, type, char *, str, str, nomem);
	    f->str = mrp_strdup(f->str);
	    if (f->str == NULL)
		goto nomem;
	    break;
	case MRP_MSG_FIELD_BOOL:
	    CREATE(f, tag, type, int, bln, bln, nomem);
	    break;
	case MRP_MSG_FIELD_UINT8:
	    CREATE(f, tag, type, unsigned int, u8, u8, nomem);
	    break;
	case MRP_MSG_FIELD_SINT8:
	    CREATE(f, tag, type, signed int, s8, s8, nomem);
	    break;
	case MRP_MSG_FIELD_UINT16:
	    CREATE(f, tag, type, unsigned int, u16, u16, nomem);
	    break;
	case MRP_MSG_FIELD_SINT16:
	    CREATE(f, tag, type, signed int, s16, s16, nomem);
	    break;
	case MRP_MSG_FIELD_UINT32:
	    CREATE(f, tag, type, unsigned int, u32, u32, nomem);
	    break;
	case MRP_MSG_FIELD_SINT32:
	    CREATE(f, tag, type, signed int, s32, s32, nomem);
	    break;
	case MRP_MSG_FIELD_UINT64:
	    CREATE(f, tag, type, uint64_t, u64, u64, nomem);
	    break;
	case MRP_MSG_FIELD_SINT64:
	    CREATE(f, tag, type, int64_t, s64, s64, nomem);
	    break;
	case MRP_MSG_FIELD_DOUBLE:
	    CREATE(f, tag, type, double, dbl, dbl, nomem);
	    break;

	case MRP_MSG_FIELD_BLOB:
	    size = va_arg(ap, uint32_t);
	    CREATE(f, tag, type, void *, blb, size[0], nomem);

	    blb        = f->blb;
	    f->size[0] = size;
	    f->blb     = mrp_allocz(size);

	    if (f->blb != NULL) {
		memcpy(f->blb, blb, size);
		f->size[0] = size;
	    }
	    else
		goto nomem;
	    break;
	    
	default:
	    if (f->type & MRP_MSG_FIELD_ARRAY) {
		errno = EOPNOTSUPP;
		mrp_log_error("XXX TODO: MRP_MSG_FIELD_ARRAY not implemented");
	    }
	    else
		errno = EINVAL;
	    
	    mrp_free(f);
	    f = NULL;
	}
#undef CREATE
    }
    
    return f;

 nomem:
    errno = ENOMEM;
    return NULL;
}


static void msg_destroy(mrp_msg_t *msg)
{
    mrp_list_hook_t *p, *n;
    mrp_msg_field_t *f;

    if (msg != NULL) {
	mrp_list_foreach(&msg->fields, p, n) {
	    f = mrp_list_entry(p, typeof(*f), hook);
	    mrp_list_delete(&f->hook);
	    
	    switch (f->type) {
	    case MRP_MSG_FIELD_STRING:
		mrp_free(f->str);
		break;
	    case MRP_MSG_FIELD_BLOB:
		mrp_free(f->blb);
		break;
	    }

	    mrp_free(f);
	}
    }
}


mrp_msg_t *mrp_msg_create(uint16_t tag, ...)
{
    mrp_msg_t       *msg;
    mrp_msg_field_t *f;
    va_list          ap;
    
    va_start(ap, tag);
    if ((msg = mrp_allocz(sizeof(*msg))) != NULL) {
	mrp_list_init(&msg->fields);
	msg->refcnt = 1;
	
	while (tag != MRP_MSG_FIELD_INVALID) {
	    f = create_field(tag, &ap);
	    
	    if (f != NULL) {
		mrp_list_append(&msg->fields, &f->hook);
		msg->nfield++;
	    }
	    else {
		msg_destroy(msg);
		msg = NULL;
		goto out;
	    }
	    tag = va_arg(ap, uint32_t);
	}
    }
 out:
    va_end(ap);
    
    return msg;
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


int mrp_msg_append(mrp_msg_t *msg, uint16_t tag, ...)
{
    mrp_msg_field_t *f;
    va_list          ap;

    va_start(ap, tag);
    f = create_field(tag, &ap);
    va_end(ap);

    if (f != NULL) {
	mrp_list_append(&msg->fields, &f->hook);
	msg->nfield++;
	return TRUE;
    }
    else
	return FALSE;
}


int mrp_msg_prepend(mrp_msg_t *msg, uint16_t tag, ...)
{
    mrp_msg_field_t *f;
    va_list          ap;

    va_start(ap, tag);
    f = create_field(tag, &ap);
    va_end(ap);

    if (f != NULL) {
	mrp_list_prepend(&msg->fields, &f->hook);
	msg->nfield++;
	return TRUE;
    }
    else
	return FALSE;    
}


mrp_msg_field_t *mrp_msg_find(mrp_msg_t *msg, uint16_t tag)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&msg->fields, p, n) {
	f = mrp_list_entry(p, typeof(*f), hook);
	if (f->tag == tag)
	    return f;
    }

    return NULL;
}


int mrp_msg_dump(mrp_msg_t *msg, FILE *fp)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;
    int              l;
    
    l = fprintf(fp, "{\n");
    mrp_list_foreach(&msg->fields, p, n) {
	f = mrp_list_entry(p, typeof(*f), hook);

	l += fprintf(fp, "    0x%x ", f->tag);

#define DUMP(_fmt, _type, _val)					\
	l += fprintf(fp, "= <%s> "_fmt"\n", _type, _val)
	
	switch (f->type) {
	case MRP_MSG_FIELD_STRING:
	    DUMP("'%s'", "string", f->str);
	    break;
	case MRP_MSG_FIELD_BOOL:
	    DUMP("%s", "boolean", f->bln ? "true" : "false");
	    break;
	case MRP_MSG_FIELD_UINT8:
	    DUMP("%u", "uint8", f->u8);
	    break;
	case MRP_MSG_FIELD_SINT8:
	    DUMP("%d", "sint8", f->s8);
	    break;
	case MRP_MSG_FIELD_UINT16:
	    DUMP("%u", "uint16", f->u16);
	    break;
	case MRP_MSG_FIELD_SINT16:
	    DUMP("%d", "sint16", f->s16);
	    break;
	case MRP_MSG_FIELD_UINT32:
	    DUMP("%u", "uint32", f->u32);
	    break;
	case MRP_MSG_FIELD_SINT32:
	    DUMP("%d", "sint32", f->s32);
	    break;
	case MRP_MSG_FIELD_UINT64:
	    DUMP("%Lu", "uint64", (long long unsigned)f->u64);
	    break;
	case MRP_MSG_FIELD_SINT64:
	    DUMP("%Ld", "sint64", (long long signed)f->s64);
	    break;
	case MRP_MSG_FIELD_DOUBLE:
	    DUMP("%f", "double", f->dbl);
	    break;
	case MRP_MSG_FIELD_BLOB: {
	    char     *p;
	    uint32_t  i;
	    
	    fprintf(fp, "= <%s> <%u bytes, ", "blob", f->size[0]);
	    
	    for (i = 0, p = f->blb; i < f->size[0]; i++, p++) {
		if (isprint(*p) && *p != '\n' && *p != '\t' && *p != '\r')
		    fprintf(fp, "%c", *p);
		else
		    fprintf(fp, ".");
	    }
	    fprintf(fp, ">\n");
	}
	    break;
	    
	default:
	    fprintf(fp, "= <%s> {%u items, XXX TODO}\n", "array", f->size[0]);
	}
    }
    l += fprintf(fp, "}\n");

    return l;
}


#define MSG_MIN_CHUNK 32

ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp)
{
#define RESERVE(type) ({						\
	    void *_ptr;							\
									\
	    _ptr = mrp_msgbuf_reserve(&mb, sizeof(type), 1);		\
									\
	    if (_ptr == NULL) {						\
		*bufp = NULL;						\
		return -1;						\
	    }								\
	    								\
	    _ptr;							\
	})

#define RESERVE_SIZE(size) ({				\
	    void *_ptr;					\
							\
	    _ptr = mrp_msgbuf_reserve(&mb, size, 1);	\
							\
	    if (_ptr == NULL) {				\
		*bufp = NULL;				\
		return -1;				\
	    }						\
							\
	    _ptr;					\
	})
    

    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;
    mrp_msgbuf_t     mb;
    uint32_t         len;
    size_t           size;
    
    size = msg->nfield * (2 * sizeof(uint16_t) + sizeof(uint64_t));
    
    if (mrp_msgbuf_write(&mb, size)) {
	MRP_MSGBUF_PUSH(&mb, htobe16(msg->nfield), 1, nomem);

	mrp_list_foreach(&msg->fields, p, n) {
	    f = mrp_list_entry(p, typeof(*f), hook);
	    
	    MRP_MSGBUF_PUSH(&mb, htobe16(f->tag) , 1, nomem);
	    MRP_MSGBUF_PUSH(&mb, htobe16(f->type), 1, nomem);

	    switch (f->type) {
	    case MRP_MSG_FIELD_STRING:
		len = strlen(f->str) + 1;
		MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
		MRP_MSGBUF_PUSH_DATA(&mb, f->str, len, 1, nomem);
		break;
		
	    case MRP_MSG_FIELD_BOOL:
		MRP_MSGBUF_PUSH(&mb, htobe32(f->bln ? TRUE : FALSE), 1, nomem);
		break;

	    case MRP_MSG_FIELD_UINT8:
		MRP_MSGBUF_PUSH(&mb, f->u8, 1, nomem);
		break;

	    case MRP_MSG_FIELD_SINT8:
		MRP_MSGBUF_PUSH(&mb, f->s8, 1, nomem);
		break;

	    case MRP_MSG_FIELD_UINT16:
		MRP_MSGBUF_PUSH(&mb, htobe16(f->u16), 1, nomem);
		break;

	    case MRP_MSG_FIELD_SINT16:
		MRP_MSGBUF_PUSH(&mb, htobe16(f->s16), 1, nomem);
		break;

	    case MRP_MSG_FIELD_UINT32:
		MRP_MSGBUF_PUSH(&mb, htobe32(f->u32), 1, nomem);
		break;

	    case MRP_MSG_FIELD_SINT32:
		MRP_MSGBUF_PUSH(&mb, htobe32(f->s32), 1, nomem);
		break;

	    case MRP_MSG_FIELD_UINT64:
		MRP_MSGBUF_PUSH(&mb, htobe64(f->u64), 1, nomem);
		break;

	    case MRP_MSG_FIELD_SINT64:
		MRP_MSGBUF_PUSH(&mb, htobe64(f->s64), 1, nomem);
		break;

	    case MRP_MSG_FIELD_DOUBLE:
		MRP_MSGBUF_PUSH(&mb, f->dbl, 1, nomem);
		break;
		
	    case MRP_MSG_FIELD_BLOB:
		len   = f->size[0];
		MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
		MRP_MSGBUF_PUSH_DATA(&mb, f->blb, len, 1, nomem);
		break;

	    default:
		if (f->type & MRP_MSG_FIELD_ARRAY) {
		    errno = EOPNOTSUPP;
		    mrp_log_error("XXX TODO: MRP_MSG_FIELD_ARRAY "
				  "not implemented");
		}
		else
		    errno = EINVAL;	

		mrp_msgbuf_cancel(&mb);
	    nomem:
		*bufp = NULL;
		return -1;
	    }
	}
    }
    
    *bufp = mb.buf;
    return mb.p - mb.buf;
}


mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size)
{
#define PULL(type) ({						\
	    void *_ptr;						\
	    							\
	    _ptr = mrp_msgbuf_pull(&mb, sizeof(type), 1);	\
	    							\
	    if (_ptr == NULL)					\
		return NULL;					\
	    							\
	    _ptr;						\
	})

#define PULL_SIZE(size) ({					\
	    void *_ptr;						\
	    							\
	    _ptr = mrp_msgbuf_pull(&mb, size, 1);		\
	    							\
	    if (_ptr == NULL)					\
		return NULL;					\
	    							\
	    _ptr;						\
	})

    mrp_msg_t       *msg;
    mrp_msgbuf_t     mb;
    mrp_msg_value_t  v;
    void            *value;
    uint16_t         nfield, tag, type;
    uint32_t         len;
    int              i;

    msg = mrp_msg_create(MRP_MSG_FIELD_INVALID);

    if (msg == NULL)
	return NULL;
    
    mrp_msgbuf_read(&mb, buf, size);
    
    nfield = be16toh(MRP_MSGBUF_PULL(&mb, typeof(nfield), 1, nodata));
    
    for (i = 0; i < nfield; i++) {
	tag  = be16toh(MRP_MSGBUF_PULL(&mb, typeof(tag) , 1, nodata));
	type = be16toh(MRP_MSGBUF_PULL(&mb, typeof(type), 1, nodata));

	switch (type) {
	case MRP_MSG_FIELD_STRING:
	    len   = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
	    value = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
	    if (!mrp_msg_append(msg, tag, type, value, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	case MRP_MSG_FIELD_BOOL:
	    v.bln = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t, 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.bln, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	case MRP_MSG_FIELD_UINT8:
	    v.u8 = MRP_MSGBUF_PULL(&mb, typeof(v.u8), 1, nodata);
	    if (!mrp_msg_append(msg, tag, type, v.u8, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_SINT8:
	    v.s8 = MRP_MSGBUF_PULL(&mb, typeof(v.s8), 1, nodata);
	    if (!mrp_msg_append(msg, tag, type, v.s8, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_UINT16:
	    v.u16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.u16), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.u16, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	case MRP_MSG_FIELD_SINT16:
	    v.s16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.s16), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.s16, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_UINT32:
	    v.u32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.u32), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.u32, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_SINT32:
	    v.s32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.s32), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.s32, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_UINT64:
	    v.u64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.u64), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.u64, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;

	case MRP_MSG_FIELD_SINT64:
	    v.s64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.s64), 1, nodata));
	    if (!mrp_msg_append(msg, tag, type, v.s64, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	case MRP_MSG_FIELD_DOUBLE:
	    v.dbl = MRP_MSGBUF_PULL(&mb, typeof(v.dbl), 1, nodata);
	    if (!mrp_msg_append(msg, tag, type, v.dbl, MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	case MRP_MSG_FIELD_BLOB:
	    len   = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
	    value = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
	    if (!mrp_msg_append(msg, tag, type, len, value,
				MRP_MSG_FIELD_INVALID))
		goto fail;
	    break;
	    
	default:
	    if (type & MRP_MSG_FIELD_ARRAY) {
		errno = EOPNOTSUPP;
		mrp_log_error("XXX TODO: MRP_MSG_FIELD_ARRAY "
			      "not implemented");
	    }
	    else
		errno = EINVAL;	
	    goto fail;
	}
    }
    
    return msg;

    
 fail:
 nodata:
    mrp_msg_unref(msg);
    return NULL;
}


void *mrp_msgbuf_write(mrp_msgbuf_t *mb, size_t size)
{
    mrp_clear(mb);

    mb->buf = mrp_allocz(size);
    
    if (mb->buf != NULL) {
	mb->size = size;
	mb->p    = mb->buf;
	mb->l    = size;

	return mb->p;
    }
    else
	return NULL;
}


void mrp_msgbuf_read(mrp_msgbuf_t *mb, void *buf, size_t size)
{
    mb->buf  = mb->p = buf;
    mb->size = mb->l = size;
}


void mrp_msgbuf_cancel(mrp_msgbuf_t *mb)
{
    mrp_free(mb->buf);
    mb->buf = mb->p = NULL;
}


void *mrp_msgbuf_ensure(mrp_msgbuf_t *mb, size_t size)
{
    int diff;
    
    if (MRP_UNLIKELY(size > mb->l)) {
	diff = size - mb->l;
	
	if (diff < MSG_MIN_CHUNK)
	    diff = MSG_MIN_CHUNK;
	
	mb->p -= (ptrdiff_t)mb->buf;
	
	if (mrp_realloc(mb->buf, mb->size + diff)) {
	    memset(mb->buf + mb->size, 0, diff);
	    mb->size += diff;
	    mb->p    += (ptrdiff_t)mb->buf;
	    mb->l    += diff;
	}
	else
	    mrp_msgbuf_cancel(mb);
    }

    return mb->p;
}


void *mrp_msgbuf_reserve(mrp_msgbuf_t *mb, size_t size, size_t align)
{
    void      *reserved;
    ptrdiff_t  offs, pad;
    size_t     len;

    len  = size;
    offs = mb->p - mb->buf;
	
    if (offs % align != 0) {
	pad  = align - (offs % align);
	len += pad;
    }
    else
	pad = 0;

    if (mrp_msgbuf_ensure(mb, len)) {
	if (pad != 0)
	    memset(mb->p, 0, pad);

	reserved = mb->p + pad;
	
	mb->p += len;
	mb->l -= len;
    }
    else
	reserved = NULL;

    return reserved;
}


void *mrp_msgbuf_pull(mrp_msgbuf_t *mb, size_t size, size_t align)
{
    void      *pulled;
    ptrdiff_t  offs, pad;
    size_t     len;

    len  = size;
    offs = mb->p - mb->buf;
	
    if (offs % align != 0) {
	pad  = align - (offs % align);
	len += pad;
    }
    else
	pad = 0;

    if (mb->l >= len) {
	pulled = mb->p + pad;
	
	mb->p += len;
	mb->l -= len;
    }
    else
	pulled = NULL;

    return pulled;
}
