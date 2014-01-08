/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#ifndef __MRP_COMMON_TLV_H__
#define __MRP_COMMON_TLV_H__

#include <stdint.h>
#include <stdbool.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

#define MRP_TLV_UNTAGGED 0

/**
 * a tagged-value-list encoding/decoding buffer
 */

typedef struct {
    void   *buf;                         /* actual data buffer */
    size_t  size;                        /* allocated buffer size */
    void   *p;                           /* encoding/decoding pointer */
    int     write : 1;                   /* whether set up for writing */
} mrp_tlv_t;

/** Set up the given TLV buffer for encoding. */
int mrp_tlv_setup_write(mrp_tlv_t *tlv, size_t prealloc);

/** Set up the given TLV buffer for decoding. */
int mrp_tlv_setup_read(mrp_tlv_t *tlv, void *buf, size_t size);

/** Clean up the given TLV buffer. */
void mrp_tlv_cleanup(mrp_tlv_t *tlv);

/** Ensure the given amount of space is available in the TLV buffer. */
int mrp_tlv_ensure(mrp_tlv_t *tlv, size_t size);

/** Reserve the given amount of buffer space from the TLV buffer. */
void *mrp_tlv_reserve(mrp_tlv_t *tlv, size_t size, int align);

/** Take ownership of the data buffer from the TLV buffer. */
void mrp_tlv_steal(mrp_tlv_t *tlv, void **bufp, size_t *sizep);

/** Trim the data buffer of the TLV buffer to current amount of data. */
void mrp_tlv_trim(mrp_tlv_t *tlv);

/** Get the current read/write offset from the TLV buffer. */
size_t mrp_tlv_offset(mrp_tlv_t *tlv);

/** Add an int8_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_int8(mrp_tlv_t *tlv, uint32_t tag, int8_t v);

/** Add an uint8_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_uint8(mrp_tlv_t *tlv, uint32_t tag, uint8_t v);

/** Add an int16_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_int16(mrp_tlv_t *tlv, uint32_t tag, int16_t v);

/** Add an uint16_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_uint16(mrp_tlv_t *tlv, uint32_t tag, uint16_t v);

/** Add an int32_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_int32(mrp_tlv_t *tlv, uint32_t tag, int32_t v);

/** Add an uint32_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_uint32(mrp_tlv_t *tlv, uint32_t tag, uint32_t v);

/** Add an int64_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_int64(mrp_tlv_t *tlv, uint32_t tag, int64_t v);

/** Add an uint64_t with an optional tag to the TLV buffer. */
int mrp_tlv_push_uint64(mrp_tlv_t *tlv, uint32_t tag, uint64_t v);

/** Add an float with an optional tag to the TLV buffer. */
int mrp_tlv_push_float(mrp_tlv_t *tlv, uint32_t tag, float v);

/** Add an double with an optional tag to the TLV buffer. */
int mrp_tlv_push_double(mrp_tlv_t *tlv, uint32_t tag, double v);

/** Add a boolean with an optional tag to the TLV buffer. */
int mrp_tlv_push_bool(mrp_tlv_t *tlv, uint32_t tag, bool v);

/** Add a string with an optional tag to the TLV buffer. */
int mrp_tlv_push_string(mrp_tlv_t *tlv, uint32_t tag, const char *str);


int mrp_tlv_pull_int8(mrp_tlv_t *tlv, uint32_t tag, int8_t *v);
int mrp_tlv_pull_uint8(mrp_tlv_t *tlv, uint32_t tag, uint8_t *v);
int mrp_tlv_pull_int16(mrp_tlv_t *tlv, uint32_t tag, int16_t *v);
int mrp_tlv_pull_uint16(mrp_tlv_t *tlv, uint32_t tag, uint16_t *v);
int mrp_tlv_pull_int32(mrp_tlv_t *tlv, uint32_t tag, int32_t *v);
int mrp_tlv_pull_uint32(mrp_tlv_t *tlv, uint32_t tag, uint32_t *v);
int mrp_tlv_pull_int64(mrp_tlv_t *tlv, uint32_t tag, int64_t *v);
int mrp_tlv_pull_uint64(mrp_tlv_t *tlv, uint32_t tag, uint64_t *v);
int mrp_tlv_pull_float(mrp_tlv_t *tlv, uint32_t tag, float *v);
int mrp_tlv_pull_double(mrp_tlv_t *tlv, uint32_t tag, double *v);
int mrp_tlv_pull_bool(mrp_tlv_t *tlv, uint32_t tag, bool *v);
int mrp_tlv_pull_string(mrp_tlv_t *tlv, uint32_t tag, char **v, size_t max,
                        void *(alloc)(size_t, void *), void *alloc_data);

MRP_CDECL_END

#endif /* __MRP_COMMON_TLV_H__ */
