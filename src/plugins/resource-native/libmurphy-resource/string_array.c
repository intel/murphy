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

#include "string_array.h"
#include <errno.h>


mrp_res_string_array_t *mrp_str_array_dup(uint32_t dim, const char **arr)
{
    uint32_t i;
    mrp_res_string_array_t *dup;

    if (dim >= ARRAY_MAX || !arr)
        return NULL;

    if (!dim && arr) {
        for (dim = 0;  arr[dim];  dim++)
            ;
    }

    if (!(dup = mrp_allocz(sizeof(mrp_res_string_array_t)))) {
        errno = ENOMEM;
        return NULL;
    }

    dup->num_strings = dim;
    dup->strings = mrp_allocz_array(const char *, dim);

    if (!dup->strings) {
        mrp_free(dup);
        errno = ENOMEM;
        return NULL;
    }

    for (i = 0;   i < dim;   i++) {
        if (arr[i]) {
            if (!(dup->strings[i] = mrp_strdup(arr[i]))) {
                for (; i > 0; i--) {
                    mrp_free((void *)dup->strings[i-1]);
                }
                mrp_free(dup->strings);
                mrp_free(dup);
                errno = ENOMEM;
                return NULL;
            }
        }
    }

    return dup;
}

/* public API */

void mrp_res_free_string_array(mrp_res_string_array_t *arr)
{
    int i;

    if (!arr)
        return;

    for (i = 0; i < arr->num_strings; i++)
        mrp_free((void *) arr->strings[i]);

    mrp_free(arr->strings);
    mrp_free(arr);
}
