/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#ifndef __MURPHY_AUTH_H__
#define __MURPHY_AUTH_H__

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/core/context.h>

MRP_CDECL_BEGIN

#define MRP_AUTH_ANY NULL                /* any authenticator */

/*
 * authentication access modes
 */
typedef enum {
    MRP_AUTH_MODE_UNKNOWN = 0x0,         /* mode unknown / not applicabe */
    MRP_AUTH_MODE_NA      = 0x0,         /* alias for unknown */
    MRP_AUTH_MODE_READ    = 0x1,         /* 'read' access */
    MRP_AUTH_MODE_WRITE   = 0x2,         /* 'write' access */
    MRP_AUTH_MODE_EXEC    = 0x4,         /* 'execution' access */
} mrp_auth_mode_t;


/*
 * authentication results
 */
typedef enum {
    MRP_AUTH_RESULT_ERROR = -1,          /* authentiation failed with error */
    MRP_AUTH_RESULT_DENY  =  0,          /* requested access denied */
    MRP_AUTH_RESULT_GRANT =  1,          /* requested access granted */
} mrp_auth_result_t;


/** Type for authenticator backend callback. */
typedef int (*mrp_auth_cb_t)(const char *target, mrp_auth_mode_t mode,
                             const char *id, const char *token,
                             void *user_data);

/** Register an authentication backend. */
int mrp_register_authenticator(mrp_context_t *ctx, const char *name,
                               mrp_auth_cb_t cb, void *user_data);

/** Unregister an authentication backend. */
void mrp_unregister_authenticator(mrp_context_t *ctx, const char *name);

/** Check if the given id has the reqested access to the given target. */
int mrp_authenticate(mrp_context_t *ctx, const char *backend,
                     const char *target, mrp_auth_mode_t mode,
                     const char *id, const char *token);

/** Convenience macro for autoregistering an authentication backend. */
#define MRP_REGISTER_AUTHENTICATOR(name, init_cb, auth_cb)              \
    MRP_INIT static void register_authenticator(void)                   \
    {                                                                   \
        int  (*initfn)(void **) = init_cb;                              \
        void  *user_data        = NULL;                                 \
                                                                        \
        if (initfn == NULL || initfn(&user_data))                       \
            mrp_register_authenticator(NULL, name, auth_cb, user_data); \
        else                                                            \
            mrp_log_error("Failed to initialize user data for "         \
                          "authenticator '%s'.", name);                 \
    }                                                                   \
    struct __mrp_allow_trailing_semicolon


MRP_CDECL_END

#endif /* __MURPHY_AUTH_H__ */
