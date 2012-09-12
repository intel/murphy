/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
