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

#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/core/context.h>
#include <murphy/core/console-priv.h>
#include <murphy/core/domain.h>

mrp_context_t *mrp_context_create(void)
{
    mrp_context_t *c;

    if ((c = mrp_allocz(sizeof(*c))) != NULL) {
        mrp_list_init(&c->plugins);
        console_setup(c);
        domain_setup(c);

        mrp_list_init(&c->auth);


        if ((c->ml = mrp_mainloop_create()) == NULL) {
            mrp_log_error("Failed to create mainloop.");
            mrp_free(c);
            c = NULL;
        }
    }

    return c;
}


void mrp_context_destroy(mrp_context_t *c)
{
    if (c != NULL) {
        console_cleanup(c);
        mrp_mainloop_destroy(c->ml);
        mrp_free(c);
    }


}


void mrp_context_setstate(mrp_context_t *c, mrp_context_state_t state)
{
    c->state = state;
}
