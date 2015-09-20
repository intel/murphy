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

#include <murphy/config.h>
#include <murphy/common/regexp.h>


#if !defined(PCRE_ENABLED)
#    include "regexp-libc.c"
#    define BACKEND "posix"
#else
#    include "regexp-pcre.c"
#    define BACKEND "pcre"
#endif

const char *mrp_regexp_backend(void)
{
    return BACKEND;
}


int mrp_regexp_glob(const char *pattern, char *buf, size_t size)
{
    const char *p;
    char       *q;
    int         l;
    int         chidx, stridx;

    if (size < 1)
        goto overflow;

    p = pattern;
    q = buf;
    l = size - 1;

    *q++ = '^';
    l--;

    chidx = stridx = 0;
    while (*p && l > 0) {
        switch (*p) {
        case '*':
            if (l < 2)
                goto overflow;
            *q++ = '.';
            *q++ = '*';
            l -= 2;
            p++;
            break;

        case '?':
            if (l < 2)
                goto overflow;
            *q++ = '\\';
            *q++ = '.';
            l -= 2;
            p++;
            break;

        case '[':
            chidx = 1;
            *q++ = '(';
            l--;
            p++;
            break;

        case ']':
            chidx = 0;
            *q++ = ')';
            l--;
            p++;
            break;

        case '{':
            if (l < 2)
                goto overflow;
            stridx = 1;
            *q++ = '(';
            *q++ = '(';
            l -= 2;
            p++;
            break;

        case '}':
            if (l < 2)
                goto overflow;
            stridx = 0;
            *q++ = ')';
            *q++ = ')';
            l += 2;
            p++;
            break;

        case ',':
            if (stridx) {
                if (stridx > 1) {
                    if (l < 3)
                        goto overflow;
                    *q++ = ')';
                    *q++ = '|';
                    *q++ = '(';
                    l -= 3;
                }
                else {
                    if (l < 3)
                        goto overflow;
                    *q++ = ')';
                    *q++ = '|';
                    *q++ = '(';
                    l -= 3;
                }
                p++;
                stridx++;
            }
            else {
                *q++ = *p++;
                l--;
            }
            break;

        case '.':
            if (l < 2)
                goto overflow;
            *q++ = '\\';
            *q++ = '.';
            l -= 2;
            p++;
            break;

        default:
            if (!chidx) {
                *q++ = *p++;
                l--;
            }
            else {
                if (chidx) {
                    if (chidx > 1) {
                        if (l < 2)
                            goto overflow;
                        *q++ = '|';
                        l--;
                    }
                    chidx++;
                }
                *q++ = *p++;
                l--;
            }
            break;
        }
    }

    if (*p != '\0' || l < 1)
        goto overflow;

    *q++ = '$';
    *q = '\0';

    mrp_debug("glob '%s' translated to regexp '%s'", pattern, buf);

    return 0;

 overflow:
    errno = EOVERFLOW;
    return -1;
}
