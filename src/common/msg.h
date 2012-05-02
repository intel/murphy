#ifndef __MURPHY_MSG_H__
#define __MURPHY_MSG_H__

#include <stdio.h>
#include <murphy/common/list.h>

/*
 * message field - tagged data
 */

typedef struct {
    char            *tag;                /* tag name */
    void            *data;               /* tag data */
    size_t           size;               /* amount of data */
    mrp_list_hook_t  hook;               /* to more fields */
} mrp_msg_field_t;


/*
 * message - a set of message fields
 */

typedef struct {
    mrp_list_hook_t fields;              /* list of message fields */
    size_t          nfield;              /* unencoded size of tags + data */
    int             refcnt;              /* reference count */
} mrp_msg_t;


/** Create a new message. */
mrp_msg_t *mrp_msg_create(const char *tag, ...);

/** Add a new reference to a message (ie. increase refcount). */
mrp_msg_t *mrp_msg_ref(mrp_msg_t *msg);

/** Delete a reference from a message, freeing it if refcount drops to zero. */
void mrp_msg_unref(mrp_msg_t *msg);

/** Append a field to a message. */
int mrp_msg_append(mrp_msg_t *msg, char *tag, void *data, size_t size);

/** Prepend a field to a message. */
int mrp_msg_prepend(mrp_msg_t *msg, char *tag, void *data, size_t size);

/** Find a field in a message. */
void *mrp_msg_find(mrp_msg_t *msg, char *tag, size_t *size);

/** Dump a message. */
int mrp_msg_dump(mrp_msg_t *msg, FILE *fp);

/** Default message encoding. */
ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp);

/** Default message decoding. */
mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size);

#endif /* __MURPHY_MSG_H__ */
