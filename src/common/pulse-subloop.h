/*
 * Copyright (c) 2014, Intel Corporation
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

#ifndef __MURPHY_PULSE_SUBLOOP_H__
#define __MURPHY_PULSE_SUBLOOP_H__

#include <murphy/common/macros.h>
#include <murphy/common/mainloop.h>

#include <pulse/mainloop-api.h>

MRP_CDECL_BEGIN

/** An opaque Murphy main loop object. */
typedef struct pa_murphy_mainloop pa_murphy_mainloop;

/** Create a new Murphy PA main loop object for the specified main loop. */
pa_murphy_mainloop *pa_murphy_mainloop_new(mrp_mainloop_t *ml);

/** Free the Murphy PA main loop object. */
void pa_murphy_mainloop_free(pa_murphy_mainloop *m);

/** Return the abstract main loop API for the PA Murphy main loop object. */
pa_mainloop_api *pa_murphy_mainloop_get_api(pa_murphy_mainloop *m);

MRP_CDECL_END

#endif /* __PULSE_SUBLOOP_H__ */
