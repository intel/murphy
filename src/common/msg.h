#ifndef __MURPHY_MSG_H__
#define __MURPHY_MSG_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#include <murphy/common/list.h>
#include <murphy/common/refcnt.h>


/*
 * message field types
 */

#define A(t) MRP_MSG_FIELD_##t
typedef enum {
    MRP_MSG_FIELD_INVALID = 0x00,        /* defined invalid type */
    MRP_MSG_FIELD_STRING  = 0x01,        /* string */
    MRP_MSG_FIELD_BOOL    = 0x02,        /* boolean */ 
    MRP_MSG_FIELD_UINT8   = 0x03,        /* unsigned 8-bit integer */
    MRP_MSG_FIELD_SINT8   = 0x04,        /* signed 8-bit integer */
    MRP_MSG_FIELD_INT8    = A(SINT8),    /* alias for SINT8 */
    MRP_MSG_FIELD_UINT16  = 0x05,        /* unsigned 16-bit integer */
    MRP_MSG_FIELD_SINT16  = 0x06,        /* signed 16-bit integer */
    MRP_MSG_FIELD_INT16   = A(SINT16),   /* alias for SINT16 */
    MRP_MSG_FIELD_UINT32  = 0x07,        /* unsigned 32-bit integer */
    MRP_MSG_FIELD_SINT32  = 0x08,        /* signed 32-bit integer */
    MRP_MSG_FIELD_INT32   = A(SINT32),   /* alias for SINT32 */
    MRP_MSG_FIELD_UINT64  = 0x09,        /* unsigned 64-bit integer */
    MRP_MSG_FIELD_SINT64  = 0x0a,        /* signed 64-bit integer */
    MRP_MSG_FIELD_INT64   = A(SINT64),   /* alias for SINT64 */
    MRP_MSG_FIELD_DOUBLE  = 0x0b,        /* double-prec. floating point */
    MRP_MSG_FIELD_BLOB    = 0x0c,        /* a blob (not allowed in arrays) */
    MRP_MSG_FIELD_ARRAY   = 0x80,        /* bit-mask to mark arrays */
} mrp_msg_field_type_t;
#undef A


/** Tag to terminate a */
#define MRP_MSG_INVALID_TAG MRP_MSG_FIELD_INVALID



/*
 * a message field
 */

#define MRP_MSG_VALUE_UNION union {		\
	char      *str;				\
	bool       bln;				\
	uint8_t    u8;				\
	int8_t     s8;				\
	uint16_t   u16;				\
	int16_t    s16;				\
	uint32_t   u32;				\
	int32_t    s32;				\
	uint64_t   u64;				\
	int64_t    s64;				\
	double     dbl;				\
	void      *blb;				\
	char     **astr;			\
	bool      *abln;			\
	uint8_t   *au8;				\
	int8_t    *as8;				\
	uint16_t  *au16;			\
	int16_t   *as16;			\
	uint32_t  *au32;			\
	int32_t   *as32;			\
	uint64_t  *au64;			\
	int64_t   *as64;			\
    }

typedef MRP_MSG_VALUE_UNION mrp_msg_value_t;

typedef struct {
    mrp_list_hook_t hook;                /* to message */
    uint16_t        tag;                 /* message field tag */
    uint16_t        type;                /* message field type */
    MRP_MSG_VALUE_UNION;                 /* message field value */
    uint32_t        size[0];             /* size, if an array or a blob */
} mrp_msg_field_t;


/*
 * a message
 */

typedef struct {
    mrp_list_hook_t fields;              /* list of message fields */
    size_t          nfield;              /* number of fields */
    mrp_refcnt_t    refcnt;              /* reference count */
} mrp_msg_t;


/*
 * a message buffer to help encoding / decoding
 */

typedef struct {
    void   *buf;                         /* message buffer */
    size_t  size;                        /* allocated size */
    void   *p;                           /* fill pointer */
    size_t  l;                           /* space left in buffer */
} mrp_msgbuf_t;

/** Create a new message. */
mrp_msg_t *mrp_msg_create(uint16_t tag, ...);

/** Add a new reference to a message (ie. increase refcount). */
mrp_msg_t *mrp_msg_ref(mrp_msg_t *msg);

/** Delete a reference from a message, freeing it if refcount drops to zero. */
void mrp_msg_unref(mrp_msg_t *msg);

/** Append a field to a message. */
int mrp_msg_append(mrp_msg_t *msg, uint16_t tag, ...);

/** Prepend a field to a message. */
int mrp_msg_prepend(mrp_msg_t *msg, uint16_t tag, ...);

/** Find a field in a message. */
mrp_msg_field_t *mrp_msg_find(mrp_msg_t *msg, uint16_t tag);

/** Dump a message. */
int mrp_msg_dump(mrp_msg_t *msg, FILE *fp);

/** Default message encoding. */
ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp);

/** Default message decoding. */
mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size);

/** Initialize the given message buffer for writing. */
void *mrp_msgbuf_write(mrp_msgbuf_t *mb, size_t size);

/** Initialize the given message buffer for reading. */
void mrp_msgbuf_read(mrp_msgbuf_t *mb, void *buf, size_t size);

/** Deinitialize the given message buffer, usually due to some error. */
void mrp_msgbuf_cancel(mrp_msgbuf_t *mb);

/** Reallocate the buffer if needed to accomodate size bytes of data. */
void *mrp_msgbuf_ensure(mrp_msgbuf_t *mb, size_t size);

/** Reserve the given amount of space from the buffer. */
void *mrp_msgbuf_reserve(mrp_msgbuf_t *mb, size_t size, size_t align);

/** Pull the given amount of data from the buffer. */
void *mrp_msgbuf_pull(mrp_msgbuf_t *mb, size_t size, size_t align);


#define MRP_MSGBUF_PUSH(mb, data, align, errlbl) do {		\
	size_t        _size = sizeof(data);			\
	typeof(data) *_ptr;					\
		 						\
	_ptr  = mrp_msgbuf_reserve((mb), _size, (align));	\
								\
	if (_ptr != NULL)					\
	    *_ptr = data;					\
	else							\
	    goto errlbl;					\
    } while (0)

#define MRP_MSGBUF_PUSH_DATA(mb, data, size, align, errlbl) do {	\
	size_t _size = (size);						\
	void   *_ptr;							\
									\
	_ptr  = mrp_msgbuf_reserve((mb), _size, (align));		\
		     							\
	if (_ptr != NULL)						\
	    memcpy(_ptr, data, _size);					\
	else								\
	    goto errlbl;						\
    } while (0)

#define MRP_MSGBUF_PULL(mb, type, align, errlbl) ({			\
	    size_t  _size = sizeof(type);				\
	    type   *_ptr;						\
									\
	    _ptr = mrp_msgbuf_pull((mb), _size, (align));		\
									\
	    if (_ptr == NULL) 						\
		goto errlbl;						\
									\
	    *_ptr;							\
	})


#define MRP_MSGBUF_PULL_DATA(mb, size, align, errlbl) ({	\
	    size_t  _size = size;				\
	    void   *_ptr;					\
								\
	    _ptr = mrp_msgbuf_pull((mb), _size, (align));	\
								\
	    if (_ptr == NULL)					\
		goto errlbl;					\
								\
	    _ptr;						\
	})

#endif /* __MURPHY_MSG_H__ */
