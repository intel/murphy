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

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include <murphy/core/context.h>
#include <murphy/core/auth.h>


typedef struct {
    char            *name;               /* backend name */
    mrp_auth_cb_t    cb;                 /* backend method */
    void            *auth_data;          /* backend data */
    mrp_list_hook_t  hook;               /* to list of backends */
} auth_backend_t;


static MRP_LIST_HOOK(pending);


static auth_backend_t *find_auth(mrp_list_hook_t *backends, const char *name)
{
    mrp_list_hook_t *p, *n;
    auth_backend_t  *auth;

    mrp_list_foreach(backends, p, n) {
        auth = mrp_list_entry(p, typeof(*auth), hook);

        if (!strcmp(auth->name, name))
            return auth;
    }

    return NULL;
}


static int register_auth(mrp_list_hook_t *backends, const char *name,
                         mrp_auth_cb_t cb, void *auth_data)
{
    auth_backend_t *auth;

    if (find_auth(backends, name) != NULL)
        return FALSE;

    auth = mrp_allocz(sizeof(*auth));

    if (auth != NULL) {
        mrp_list_init(&auth->hook);

        auth->name      = mrp_strdup(name);
        auth->cb        = cb;
        auth->auth_data = auth_data;

        if (auth->name != NULL) {
            /*
             * Notes:
             *     Prepending here is a crude hack to make sure the first
             *     registered backend, which is 'deny', ends up being the
             *     last in the list of authenticators. Maybe we should add
             *     a priority to the backend registration interface and
             *     use it to make this more explicit...
             */

            mrp_list_prepend(backends, &auth->hook);

            mrp_debug("registered authentication backend %s", auth->name);

            return TRUE;
        }

        mrp_free(auth);
    }

    return FALSE;
}


static void unregister_auth(mrp_list_hook_t *backends, const char *name)
{
    auth_backend_t *auth;

    auth = find_auth(backends, name);

    if (auth != NULL) {
        mrp_list_delete(&auth->hook);
        mrp_free(auth->name);
        mrp_free(auth);
    }
}


int mrp_register_authenticator(mrp_context_t *ctx, const char *name,
                               mrp_auth_cb_t cb, void *auth_data)
{
    mrp_list_hook_t *backends;

    if (ctx != NULL) {
        if (MRP_UNLIKELY(!mrp_list_empty(&pending)))
            mrp_list_move(&ctx->auth, &pending);

        backends = &ctx->auth;
    }
    else
        backends = &pending;

    if (register_auth(backends, name, cb, auth_data) == 0)
        return TRUE;
    else
        return FALSE;
}


void mrp_unregister_authenticator(mrp_context_t *ctx, const char *name)
{
    mrp_list_hook_t *backends = ctx ? &ctx->auth : &pending;

    unregister_auth(backends, name);
}


int mrp_authenticate(mrp_context_t *ctx, const char *backend,
                     const char *target, mrp_auth_mode_t mode,
                     const char *id, const char *token)
{
    auth_backend_t  *auth;
    mrp_list_hook_t *p, *n;
    int              status, result;

    if (MRP_UNLIKELY(!mrp_list_empty(&pending)))
        mrp_list_move(&ctx->auth, &pending);

    /*
     * Notes:
     *
     * Currently we let the caller request authentication by any available
     * backend by using MRP_AUTH_ANY. If requested so, access is granted
     * if any of the backends grants access.
     *
     * We might want to change this in the future, probably by either
     * requiring the caller to always specify a valid authentication backend,
     * or by having one of the backends be marked as default which then would
     * be used for authentication in these cases. Either of those would make
     * it more difficult to grant unwanted access accidentially in the case
     * of multiple available backends.
     */

    result = MRP_AUTH_RESULT_ERROR;

    mrp_list_foreach(&ctx->auth, p, n) {
        auth = mrp_list_entry(p, typeof(*auth), hook);

        if (backend == MRP_AUTH_ANY || !strcmp(backend, auth->name)) {
            status = auth->cb(target, mode, id, token, auth->auth_data);

            mrp_debug("backend %s, access 0x%x of %s/%s to %s: %d", auth->name,
                      mode, id, token ? token : "<none>", target, status);

            if (backend != MRP_AUTH_ANY)
                return status;

            switch (status) {
            case MRP_AUTH_RESULT_GRANT:
                return MRP_AUTH_RESULT_GRANT;
            case MRP_AUTH_RESULT_DENY:
                result = MRP_AUTH_RESULT_DENY;
                break;
            default:
                break;
            }
        }
    }

    return result;
}
