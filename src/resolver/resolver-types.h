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

#ifndef __MURPHY_RESOLVER_TYPES_H__
#define __MURPHY_RESOLVER_TYPES_H__

#include <stdint.h>

#include <murphy/common/mainloop.h>
#include <murphy/common/hashtbl.h>
#include <murphy/core/context.h>
#include <murphy/core/scripting.h>

#include <murphy-db/mqi.h>

typedef struct target_s target_t;        /* opaque type for resolver targets */
typedef struct fact_s   fact_t;          /* opaque type for tracked facts */

/*
 * a resolver target
 */
struct target_s {
    char            *name;               /* target name */
    uint32_t         stamp;              /* touch-stamp */
    char           **depends;            /* dependencies stated in the input */
    int              ndepend;            /* number of dependencies */
    int             *update_facts;       /* facts to check when updating */
    int             *update_targets;     /* targets to check when updating */
    int             *directs;            /* direct dependencies */
    int              ndirect;            /* number of direct dependencies */
    uint32_t        *fact_stamps;        /* stamps of facts at last update */
    mrp_scriptlet_t *script;             /* update script if any, or NULL */
    int              prepared : 1;       /* ready for resolution */
    int              precompiled : 1;
};


/*
 * a tracked fact
 */
struct fact_s {
    char         *name;                  /* fact name */
    mqi_handle_t  table;                 /* associated DB table */
    uint32_t      stamp;                 /* touch-stamp */
};


struct mrp_resolver_s {
    mrp_context_t     *ctx;              /* murphy context we're running in */
    mrp_event_bus_t   *bus;              /* bus for resolver events */
    target_t          *targets;          /* targets defined in the ruleset */
    int                ntarget;          /* number of targets */
    fact_t            *facts;            /* facts tracked as dependencies */
    int                nfact;            /* number of tracked facts */
    target_t          *auto_update;      /* target to resolve on fact changes */
    mrp_deferred_t    *auto_scheduled;   /* scheduled auto_update */
    uint32_t           stamp;            /* update stamp */
    mrp_context_tbl_t *ctbl;             /* context variable table */
    int                level;            /* target update nesting level */
};


#endif /* __MURPHY_RESOLVER_TYPES_H__ */
