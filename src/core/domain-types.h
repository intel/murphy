/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#ifndef __MURPHY_CORE_DOMAIN_TYPES_H__
#define __MURPHY_CORE_DOMAIN_TYPES_H__

#include <murphy/common/macros.h>
#include <murphy/common/msg.h>

MRP_CDECL_BEGIN


/*
 * passable data types to/from domain controllers
 */
typedef enum {
    MRP_DOMCTL_END      = MRP_MSG_FIELD_INVALID,
    MRP_DOMCTL_STRING   = MRP_MSG_FIELD_STRING,
    MRP_DOMCTL_INTEGER  = MRP_MSG_FIELD_INT32,
    MRP_DOMCTL_UNSIGNED = MRP_MSG_FIELD_UINT32,
    MRP_DOMCTL_DOUBLE   = MRP_MSG_FIELD_DOUBLE,
    MRP_DOMCTL_BOOL     = MRP_MSG_FIELD_BOOL,
    MRP_DOMCTL_UINT8    = MRP_MSG_FIELD_UINT8,
    MRP_DOMCTL_INT8     = MRP_MSG_FIELD_INT8,
    MRP_DOMCTL_UINT16   = MRP_MSG_FIELD_UINT16,
    MRP_DOMCTL_INT16    = MRP_MSG_FIELD_INT16,
    MRP_DOMCTL_UINT32   = MRP_MSG_FIELD_UINT32,
    MRP_DOMCTL_INT32    = MRP_MSG_FIELD_INT32,
    MRP_DOMCTL_UINT64   = MRP_MSG_FIELD_UINT64,
    MRP_DOMCTL_INT64    = MRP_MSG_FIELD_INT64,

#define MRP_DOMCTL_ARRAY(_type)      MRP_MSG_FIELD_ARRAY_OF(_type)
#define MRP_DOMCTL_IS_ARRAY(_type)   MRP_MSG_FIELD_IS_ARRAY(_type)
#define MRP_DOMCTL_ARRAY_TYPE(_type) MRP_MSG_FIELD_ARRAY_TYPE(_type)
} mrp_domctl_type_t;


/*
 * a single data value passed to/from a domain controller
 */

typedef struct {
    mrp_domctl_type_t  type;             /* data type */
    union {
        /* these are usable both in DB operations and proxied invocations */
        const char *str;                 /* MRP_DOMCTL_STRING */
        uint32_t    u32;                 /* MRP_DOMCTL_{UNSIGNED,UINT32} */
        int32_t     s32;                 /* MRP_DOMCTL_{INTEGER,INT32} */
        double      dbl;                 /* MRP_DOMCTL_DOUBLE */
        /* these are only usable in proxied invocations */
        int         bln;                 /* MRP_DOMCTL_BOOL */
        uint8_t     u8;                  /* MRP_DOMCTL_UINT8 */
        int8_t      s8;                  /* MRP_DOMCTL_INT8 */
        uint16_t    u16;                 /* MRP_DOMCTL_UINT16 */
        int16_t     s16;                 /* MRP_DOMCTL_INT16 */
        uint64_t    u64;                 /* MRP_DOMCTL_UINT64 */
        int64_t     s64;                 /* MRP_DOMCTL_INT64 */
        void       *arr;                 /* MRP_DOMCTL_ARRAY(*) */
    };
    uint32_t           size;             /* size for arrays */
} mrp_domctl_value_t;


/*
 * proxied invokation errors
 */

typedef enum {
    MRP_DOMAIN_OK = 0,                   /* no errors */
    MRP_DOMAIN_NOTFOUND,                 /* domain not found */
    MRP_DOMAIN_NOMETHOD,                 /* call domain method not found */
    MRP_DOMAIN_FAILED,                   /* called method remotely failed */
} mrp_domain_error_t;

/* Type for a proxied invocation argument. */
typedef mrp_domctl_value_t mrp_domctl_arg_t;

MRP_CDECL_END

#endif /* __MURPHY_CORE_DOMAIN_TYPES_H__ */
