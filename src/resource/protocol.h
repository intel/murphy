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

#ifndef __MURPHY_RESOURCE_PROTOCOL_H__
#define __MURPHY_RESOURCE_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <murphy/common/msg.h>

#define RESPROTO_DEFAULT_ADDRESS      "unxs:@murphy-resource-native"
#define RESPROTO_DEFAULT_ADDRVAR      "MURPHY_RESOURCE_ADDRESS"


#define RESPROTO_BIT(n)               ((uint32_t)1 << (n))

#define RESPROTO_RSETFLAG_AUTORELEASE RESPROTO_BIT(0)
#define RESPROTO_RSETFLAG_AUTOACQUIRE RESPROTO_BIT(1)
#define RESPROTO_RSETFLAG_NOEVENTS    RESPROTO_BIT(2)
#define RESPROTO_RSETFLAG_DONTWAIT    RESPROTO_BIT(3)

#define RESPROTO_RESFLAG_MANDATORY    RESPROTO_BIT(0)
#define RESPROTO_RESFLAG_SHARED       RESPROTO_BIT(1)

#define RESPROTO_TAG(x)               ((uint16_t)(x))

#define RESPROTO_MESSAGE_END          MRP_MSG_FIELD_END
#define RESPROTO_SECTION_END          RESPROTO_TAG(1)
#define RESPROTO_ARRAY_DIMENSION      RESPROTO_TAG(2)
#define RESPROTO_SEQUENCE_NO          RESPROTO_TAG(3)
#define RESPROTO_REQUEST_TYPE         RESPROTO_TAG(4)
#define RESPROTO_REQUEST_STATUS       RESPROTO_TAG(5)
#define RESPROTO_RESOURCE_SET_ID      RESPROTO_TAG(6)
#define RESPROTO_RESOURCE_STATE       RESPROTO_TAG(7)
#define RESPROTO_RESOURCE_GRANT       RESPROTO_TAG(8)
#define RESPROTO_RESOURCE_ADVICE      RESPROTO_TAG(9)
#define RESPROTO_RESOURCE_ID          RESPROTO_TAG(10)
#define RESPROTO_RESOURCE_NAME        RESPROTO_TAG(11)
#define RESPROTO_RESOURCE_FLAGS       RESPROTO_TAG(12)
#define RESPROTO_RESOURCE_PRIORITY    RESPROTO_TAG(13)
#define RESPROTO_CLASS_NAME           RESPROTO_TAG(14)
#define RESPROTO_ZONE_NAME            RESPROTO_TAG(15)
#define RESPROTO_ATTRIBUTE_INDEX      RESPROTO_TAG(16)
#define RESPROTO_ATTRIBUTE_NAME       RESPROTO_TAG(17)
#define RESPROTO_ATTRIBUTE_VALUE      RESPROTO_TAG(18)

typedef enum {
    RESPROTO_QUERY_RESOURCES,
    RESPROTO_QUERY_CLASSES,
    RESPROTO_QUERY_ZONES,
    RESPROTO_CREATE_RESOURCE_SET,
    RESPROTO_DESTROY_RESOURCE_SET,
    RESPROTO_ACQUIRE_RESOURCE_SET,
    RESPROTO_RELEASE_RESOURCE_SET,
    RESPROTO_RESOURCES_EVENT,
} mrp_resproto_request_t;

typedef enum {
    RESPROTO_RELEASE,
    RESPROTO_ACQUIRE,
} mrp_resproto_state_t;


static inline const char *mrp_resource_get_default_address(void)
{
    const char *addr;

    if ((addr = getenv(RESPROTO_DEFAULT_ADDRVAR)) == NULL)
        return RESPROTO_DEFAULT_ADDRESS;
    else
        return addr;
}

#endif  /* __MURPHY_RESOURCE_PROTOCOL_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
