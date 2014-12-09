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

#include <murphy/common/mainloop.h>

#include "resolver-types.h"
#include "resolver.h"
#include "events.h"

MRP_REGISTER_EVENTS(events,
             MRP_EVENT(MRP_RESOLVER_EVENT_STARTED, RESOLVER_UPDATE_STARTED),
             MRP_EVENT(MRP_RESOLVER_EVENT_FAILED , RESOLVER_UPDATE_FAILED ),
             MRP_EVENT(MRP_RESOLVER_EVENT_DONE   , RESOLVER_UPDATE_DONE   ));


int emit_resolver_event(mrp_resolver_t *r, int event, const char *target,
                        int level)
{
    uint16_t ttarget = MRP_RESOLVER_TAG_TARGET;
    uint16_t tlevel  = MRP_RESOLVER_TAG_LEVEL;
    int      flags   = MRP_EVENT_SYNCHRONOUS;

    return mrp_event_emit_msg(r->bus, events[event].id, flags,
                              MRP_MSG_TAG_STRING(ttarget, target),
                              MRP_MSG_TAG_UINT32(tlevel , level));
}
