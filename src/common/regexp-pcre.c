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

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/regexp.h>


mrp_regexp_t *mrp_regexp_compile(const char *pattern, int flags)
{
    mrp_regexp_t *re;
    const char   *errmsg;
    int           errofs;

    re = pcre_compile(pattern, flags, &errmsg, &errofs, NULL);

    if (re == NULL)
        errno = EINVAL;

    return re;
}


void mrp_regexp_free(mrp_regexp_t *re)
{
    if (re == NULL)
        return;

    pcre_free(re);
}


bool mrp_regexp_matches(mrp_regexp_t *re, const char *input, int flags)
{
    int len = (int)strlen(input);

    if (pcre_exec(re, NULL, input, len, 0, flags, NULL, 0) < 0)
        return false;
    else
        return true;
}


int mrp_regexp_exec(mrp_regexp_t *re, const char *input, mrp_regmatch_t *matches,
                    size_t nmatch, int flags)
{
    int len = (int)strlen(input);
    int n;

    if (nmatch % 3) {                /* PCRE requires to be a multiple of 3 */
        errno = EINVAL;
        return -1;
    }

    n = pcre_exec(re, NULL, input, len, 0, flags, matches, (int)nmatch);

    if (n < 0)
        return -1;
    else
        return n;
}


bool mrp_regexp_match(mrp_regmatch_t *matches, int idx, int *beg, int *end)
{
    if (beg != NULL)
        *beg = (int)matches[2 * idx];
    if (end != NULL)
        *end = (int)matches[2 * idx + 1];

    return true;  /* Hmm... not sure if we can check if this is valid */
}
