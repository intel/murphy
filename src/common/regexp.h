/*
 * Copyright (c) 2012-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef __MRP_REGEXP_H__
#define __MRP_REGEXP_H__

#include <string.h>
#include <stdbool.h>

#if !defined(PCRE_ENABLED)

#include <regex.h>
#include <sys/types.h>

typedef regex_t    mrp_regexp_t;
typedef regmatch_t mrp_regmatch_t;

#define MRP_REGEXP_EXTENDED REG_EXTENDED
#define MRP_REGEXP_NOSUB    REG_NOSUB

#else /* PCRE_ENABLED */

#include <pcre.h>
#include <sys/types.h>

typedef pcre mrp_regexp_t;
typedef int  mrp_regmatch_t;

#define MRP_REGEXP_EXTENDED PCRE_EXTENDED
#define MRP_REGEXP_NOSUB    0

#endif /* PCRE_ENABLED */


int mrp_regexp_glob(const char *pattern, char *buf, size_t size);
mrp_regexp_t *mrp_regexp_compile(const char *pattern, int flags);
void mrp_regexp_free(mrp_regexp_t *re);
bool mrp_regexp_matches(mrp_regexp_t *re, const char *input, int flags);
int mrp_regexp_exec(mrp_regexp_t *re, const char *input, mrp_regmatch_t *matches,
                    size_t nmatch, int flags);
bool mrp_regexp_match(mrp_regmatch_t *matches, int idx, int *beg, int *end);
const char *mrp_regexp_backend(void);

#endif /* __MRP_REGEXP_H__ */
