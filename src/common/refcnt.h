/*
 * Copyright (c) 2012, Intel Corporation
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

#ifndef __MURPHY_REFCNT_H__
#define __MURPHY_REFCNT_H__

/*
 * A place/typeholder, so we can switch easily to atomic type
 * if/when necessary.
 */

#include <murphy/common/macros.h>
#include <murphy/common/log.h>

#define __MURPHY_REFCNT_CHECK__

MRP_CDECL_BEGIN

typedef int mrp_refcnt_t;

static inline void *_mrp_ref_obj(void *obj, off_t offs)
{
    mrp_refcnt_t *refcnt;

    if (obj != NULL) {
        refcnt = (mrp_refcnt_t *) ((char *) obj + offs);
        (*refcnt)++;
    }

    return obj;
}

static inline int _mrp_unref_obj(void *obj, off_t offs
#ifdef __MURPHY_REFCNT_CHECK__
                                 , const char *file
                                 , int line
                                 , const char *func
#endif
                                 )
{
    mrp_refcnt_t *refcnt;

    if (obj != NULL) {
        refcnt = (mrp_refcnt_t *) ((char *) obj + offs);
        --(*refcnt);

        if (*refcnt == 0)
            return TRUE;

#ifdef __MURPHY_REFCNT_CHECK__
#  define W mrp_log_error

        if (*refcnt < 0) {
            W("****************** REFCOUNTING BUG WARNING ******************");
            W("* Reference-counting bug detected. The reference count of");
            W("* object %p (@offs %d) has dropped to %d.", obj, (int)offs,
              (int)*refcnt);
            W("* The offending unref call was made at:");
            W("*     %s@%s:%d", func ? func : "<unkown>",
              file ? file : "<unknown>", line);
            W("*************************************************************");
        }

#undef W
#endif
    }

    return FALSE;
}


static inline void mrp_refcnt_init(mrp_refcnt_t *refcnt)
{
    *refcnt = 1;
}

#define mrp_ref_obj(obj, member)                                          \
    (typeof(obj))_mrp_ref_obj(obj, MRP_OFFSET(typeof(*(obj)), member))

#ifndef __MURPHY_REFCNT_CHECK__
#  define mrp_unref_obj(obj, member)                                      \
    _mrp_unref_obj(obj, MRP_OFFSET(typeof(*(obj)), member))
#else
#  define mrp_unref_obj(obj, member)                                      \
    _mrp_unref_obj(obj, MRP_OFFSET(typeof(*(obj)), member), __LOC__)
#endif

MRP_CDECL_END

#endif /* __MURPHY_REFCNT_H__ */
