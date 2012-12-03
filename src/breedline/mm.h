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

#ifndef __BREEDLINE_MM_H__
#define __BREEDLINE_MM_H__

#include <stdlib.h>
#include <string.h>

/* Macro that can be used to pass the location of its usage further. */
#define BRL_LOC __FILE__, __LINE__, __func__

/* Memory allocation macros used by breedline. */
#define brl_allocz(size) ({                                        \
            void   *_ptr;                                          \
            size_t  _size = size;                                  \
                                                                   \
            if ((_ptr = __brl_mm.allocfn(_size, BRL_LOC)) != NULL) \
                memset(_ptr, 0, _size);                            \
                                                                   \
            __brl_mm_busy = TRUE;                                  \
                                                                   \
            _ptr; })

#define brl_reallocz(ptr, o, n) ({                               \
            typeof(ptr) _ptr;                                    \
            size_t      _size = sizeof(*_ptr) * (n);             \
            typeof(n)   _n    = (n);                             \
            typeof(o)   _o;                                      \
                                                                 \
            if ((ptr) != NULL)                                   \
                _o = o;                                          \
            else                                                 \
                _o = 0;                                          \
                                                                 \
            _ptr = __brl_mm.reallocfn(ptr, _size, BRL_LOC);      \
            if (_ptr != NULL || _n == 0) {                       \
                if ((unsigned)(_n) > (unsigned)(_o))             \
                    memset(_ptr + (_o), 0,                       \
                           ((_n) - (_o)) * sizeof(*_ptr));       \
                ptr = _ptr;                                      \
            }                                                    \
                                                                 \
            __brl_mm_busy = TRUE;                                \
                                                                 \
            _ptr; })

#define brl_strdup(s) ({                                         \
            __brl_mm_busy = TRUE;                                \
                                                                 \
            __brl_mm.strdupfn((s), BRL_LOC);                     \
        })

#define brl_free(ptr) __brl_mm.freefn((ptr), BRL_LOC)

extern brl_allocator_t __brl_mm;
extern int __brl_mm_busy;

#endif /* __BREEDLINE_MM_H__ */
