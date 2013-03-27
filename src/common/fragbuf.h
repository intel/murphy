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

#ifndef __MURPHY_FRAGBUF_H__
#define __MURPHY_FRAGBUF_H__

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

/*
 * Fragment collector buffers.
 *
 * As the name implies, a fragment collector buffer can be used
 * to collect message fragments and reassemble messages that were
 * transmitted in arbitrary pieces.
 *
 * Messages are expected to be transmitted in frames where each
 * frame simply consist of a 32-bit message size followed by
 * the actual message data. On the sending side you can simply
 * send each message prefixed with its size. On the receiving side
 * you keep feeding the received chunks of data to a fragment
 * collector buffer (using mrp_fragbuf_push). After each chunk you
 * can iterate through the fully reassembled messages (by calling
 * mrp_fragbuf_pull until it returns FALSE). Messages are removed
 * automatically from the collector buffer as you iterate through
 * them.
 *
 * You can also create a collector buffer in frameless mode. Such a
 * buffer will always return immediately all available data as you
 * iterate through it.
 */

/** Buffer for collecting fragments of (framed or unframed) message data. */
typedef struct mrp_fragbuf_s mrp_fragbuf_t;

/** Initialize the given fragment collector buffer. */
mrp_fragbuf_t *mrp_fragbuf_create(int framed, size_t pre_alloc);

/** Initialize the given data collector buffer. */
int mrp_fragbuf_init(mrp_fragbuf_t *buf, int framed, size_t pre_alloc);

/** Reset the given data collector buffer. */
void mrp_fragbuf_reset(mrp_fragbuf_t *buf);

/** Destroy the given data collector buffer, freeing all associated memory. */
void mrp_fragbuf_destroy(mrp_fragbuf_t *buf);

/** Return the amount of buffer space currently in used in th buffer. */
size_t mrp_fragbuf_used(mrp_fragbuf_t *buf);

/** Return the amount of bytes missing from the last message. */
size_t mrp_fragbuf_missing(mrp_fragbuf_t *buf);

/** Allocate a buffer of the given size from the buffer. */
void *mrp_fragbuf_alloc(mrp_fragbuf_t *buf, size_t size);

/** Trim the last allocation to nsize bytes. */
int mrp_fragbuf_trim(mrp_fragbuf_t *buf, void *ptr, size_t osize, size_t nsize);

/** Append the given data to the buffer. */
int mrp_fragbuf_push(mrp_fragbuf_t *buf, void *data, size_t size);

/** Iterate through the given buffer, pulling and freeing assembled messages. */
int mrp_fragbuf_pull(mrp_fragbuf_t *buf, void **data, size_t *size);

MRP_CDECL_END

#endif /* __MURPHY_FRAGBUF_H__ */
