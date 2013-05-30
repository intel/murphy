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

#include <sys/smack.h>

#include <murphy/common/debug.h>
#include <murphy/core/context.h>
#include <murphy/core/auth.h>


static int smack_auth(const char *target, mrp_auth_mode_t mode, const char *id,
                      const char *token, void *auth_data)
{
    char access[4];
    int  status;

    MRP_UNUSED(token);
    MRP_UNUSED(auth_data);

    if (target == NULL || id == NULL)
        goto error;

    access[0] = (mode & MRP_AUTH_MODE_READ)  ? 'r' : '-';
    access[1] = (mode & MRP_AUTH_MODE_WRITE) ? 'w' : '-';
    access[2] = (mode & MRP_AUTH_MODE_EXEC)  ? 'x' : '-';
    access[3] = '\0';

    status = smack_have_access(target, id, access);

    mrp_debug("SMACK '%s' access of %s to %s: %d", access, id, target, status);

    switch (status) {
    case 1:
        return MRP_AUTH_RESULT_GRANT;
    case 0:
        return MRP_AUTH_RESULT_DENY;
    default:
    error:
        return MRP_AUTH_RESULT_ERROR;
    }
}


MRP_REGISTER_AUTHENTICATOR("smack", NULL, smack_auth);
