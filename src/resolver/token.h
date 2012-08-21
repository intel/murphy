#ifndef __MURPHY_RESOLVER_TOKEN_H__
#define __MURPHY_RESOLVER_TOKEN_H__

#include <stdint.h>
#include <stdlib.h>

/*
 * common token fields
 */

#define RESOLVER_TOKEN_FIELDS                                             \
    const char *token;                   /* token string */               \
    const char *source;                  /* encountered in this source */ \
    int         line;                    /* and on this line */           \
    size_t      size                     /* token size */

/*
 * a generic token
 */

typedef struct {
    RESOLVER_TOKEN_FIELDS;
} tkn_any_t;


/*
 * a string token
 */

typedef struct {
    RESOLVER_TOKEN_FIELDS;
    char *value;
} tkn_string_t;


/*
 * signed and unsigned 16-bit integer tokens
 */

typedef struct {
    RESOLVER_TOKEN_FIELDS;
    int16_t value;
} tkn_s16_t;


typedef struct {
    RESOLVER_TOKEN_FIELDS;
    uint16_t value;
} tkn_u16_t;


/*
 * signed and unsigned 32-bit integer tokens
 */

typedef struct {
    RESOLVER_TOKEN_FIELDS;
    int32_t value;
} tkn_s32_t;


typedef struct {
    RESOLVER_TOKEN_FIELDS;
    uint32_t value;
} tkn_u32_t;


typedef struct {
    RESOLVER_TOKEN_FIELDS;
    int    nstr;
    char **strs;
} tkn_strarr_t;

#ifdef __MURPHY_RESOLVER_CHECK_RINGBUF__
#    define RESOLVER_TOKEN_DONE(t)         memset((t).token, 0, (t).size)
#else
#    define RESOLVER_TOKEN_DONE(t)         do {} while (0)
#endif

#define RESOLVER_TOKEN_SAVE(str, size) save_token((str), (size))


#endif /* __MURPHY_RESOLVER_TOKEN_H__ */
