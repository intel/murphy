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

#ifndef __MURPHY_CONTEXT_H__
#define __MURPHY_CONTEXT_H__

#include <stdbool.h>

typedef struct mrp_context_s mrp_context_t;

#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>
#include <murphy/resolver/resolver.h>


typedef enum {
    MRP_STATE_INITIAL = 0,
    MRP_STATE_LOADING,
    MRP_STATE_STARTING,
    MRP_STATE_RUNNING,
    MRP_STATE_STOPPING
} mrp_context_state_t;


struct mrp_context_s {
    /* logging settings, path configuration, etc. */
    int         log_mask;                  /* what to log */
    const char *log_target;                /* and where to log to */

    const char *config_file;               /* configuration file */
    const char *config_dir;                /* plugin configuration directory */
    const char *plugin_dir;                /* plugin directory */
    bool        foreground;                /* whether to stay in foreground*/

    char       *resolver_ruleset;          /* resolver ruleset file */

    const char *blacklist_plugins;         /* blacklisted plugins */
    const char *blacklist_builtin;         /* blacklisted builtin plugins */
    const char *blacklist_dynamic;         /* blacklisted dynamic plugins */
    const char *whitelist_plugins;         /* whitelisted plugins */
    const char *whitelist_builtin;         /* whitelisted builtin plugins */
    const char *whitelist_dynamic;         /* whitelisted dynamic plugins */
    bool        disable_runtime_load;      /* disallow post-startup loading */
    bool        disable_console;           /* disable murphy console */

    /* actual runtime context data */
    int              state;                /* context/daemon state */
    mrp_mainloop_t  *ml;                   /* mainloop */
    mrp_list_hook_t  plugins;              /* list of loaded plugins */
    mrp_event_bus_t *plugin_bus;           /* bus for plugin events */
    mrp_event_bus_t *daemon_bus;           /* bus for daemon events */
    mrp_list_hook_t  cmd_groups;           /* console command groups */
    mrp_list_hook_t  consoles;             /* active consoles */
    mrp_resolver_t  *r;                    /* resolver context */
    void            *lua_state;            /* state for Lua bindings */
    mrp_list_hook_t  auth;                 /* authenticator backends */

    /*
     * Hmm, this is not very nice.  Most of the domain handling code (in
     * practice all) used to live in the domain-control plugin. To avoid
     * loading order dependencies on plugin-domain-control we now started
     * collecting registered handlers of proxied functions here. Calls by
     * the core to proxied functions of domain controllers and by domain-
     * controllers to the core are still handled in the domain-control
     * plugin (and in the domain-controller client library).
     *
     * It would be perhaps the cleanest not to have a domain-controller
     * specific function export mechanism at all. Instead the various
     * import/export mechanisms (at least plugins, resolver, and this) should
     * be replaced by / built on a single core implementation that is flexible
     * enough to handle all the needs of all these.
     */

    mrp_list_hook_t  domain_methods;       /* functions for domain controllers */
    void            *domain_invoke;        /* domain invoke handler */
    void            *domain_data;          /* domain invoke handler data */
};

/** Create a new murphy context. */
mrp_context_t *mrp_context_create(void);

/** Destroy an existing murphy context. */
void mrp_context_destroy(mrp_context_t *c);

/** Set the context state to the given state. */
void mrp_context_setstate(mrp_context_t *c, mrp_context_state_t state);

#endif /* __MURPHY_CONTEXT_H__ */
