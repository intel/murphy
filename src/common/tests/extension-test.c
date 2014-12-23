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

#include <stdlib.h>
#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/object.h>
#include <murphy/common/mainloop.h>
#include <murphy/resolver/resolver.h>

#define VERIFY(_expr, ...) do {                         \
        if (!(_expr)) {                                 \
            mrp_log_error("%s:%d: test failure: %s",    \
                          __FILE__, __LINE__, #_expr);  \
            mrp_log_error(__VA_ARGS__);                 \
            exit(1);                                    \
        }                                               \
    } while (0)

typedef struct {
    int              log_mask;
    const char      *log_target;
    const char      *config_file;
    const char      *config_dir;
    const char      *plugin_dir;
    bool             foreground;
    mrp_list_hook_t  plugins;
    mrp_mainloop_t  *ml;
    MRP_EXTENSIBLE;
} context_t;


static void free_r(void *obj, uint32_t id, void *value)
{
    mrp_log_info("should free resolver %p (%p[#%u])", value, obj, id);
}

static void free_bl(void *obj, uint32_t id, void *value)
{
    mrp_log_info("should free blacklist %p (%p[#%u])", value, obj, id);
}

static void free_wl(void *obj, uint32_t id, void *value)
{
    mrp_log_info("should free whitelist %p (%p[#%u])", value, obj, id);
}


int main(int argc, char *argv[])
{
    context_t       ctx;
    uint32_t        ext_r, ext_bl, ext_wl, ext_lua, context_id;
    mrp_resolver_t *r;
    char           *bl, *wl;
    void           *lua;
    ptrdiff_t       i;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_INFO));
    mrp_debug_enable(true);
    mrp_debug_set("@object.c");

    ext_r   = MRP_EXTEND_TYPE(context_t, mrp_resolver_t *, resolver , free_r);
    ext_bl  = MRP_EXTEND_TYPE(context_t, char *          , blacklist, free_bl);
    ext_wl  = MRP_EXTEND_TYPE(context_t, char *          , whitelist, free_wl);
    ext_lua = MRP_EXTEND_TYPE(context_t, void *          , lua      , NULL);

    VERIFY(ext_r  , "failed to register extension 'resolver'");
    VERIFY(ext_bl , "failed to register extension 'blacklist'");
    VERIFY(ext_wl , "failed to register extension 'whitelist'");
    VERIFY(ext_lua, "failed to register extension 'lua'");

    mrp_log_info("extensions registered successfully...");

    VERIFY((context_id = MRP_EXTENSIBLE_TYPE(context_t)),
           "failed to register context_t as an extensible type");

    mrp_clear(&ctx);

    VERIFY(mrp_extensible_check(&ctx, context_id) != 0,
           "mrp_extensible_check sould have failed");
    VERIFY(!mrp_extensible_of_type(&ctx, context_t),
           "mrp_extensible_of_type should have failed");

    mrp_extensible_init(&ctx, context_id);

    VERIFY(mrp_extensible_check(&ctx, context_id) == 0,
           "mrp_extensible_check failed");
    VERIFY(mrp_extensible_of_type(&ctx, context_t),
           "mrp_extensible_of_type failed");

    for (i = 0; i < 5; i++) {
        r   = (mrp_resolver_t *)(i * 4 +  0x1);
        bl  = (char *)(i * 4 + 0x2);
        wl  = (char *)(i * 4 + 0x3);
        lua = (void *)(i * 4 + 0x4);

        VERIFY(MRP_EXTEND(&ctx, ext_r, mrp_resolver_t *, r) == 0,
               "failed to set resolver extension");
        VERIFY(MRP_EXTEND(&ctx, ext_bl, char *, bl) == 0,
               "failed to set blacklist extension");
        VERIFY(MRP_EXTEND(&ctx, ext_wl, char *, wl) == 0,
               "failed to set whitelist extension");
        VERIFY(MRP_EXTEND(&ctx, ext_lua, void *, lua) == 0,
               "failed to set lua extension");

        mrp_log_info("extensions set successfully...");

        VERIFY(mrp_extension_set(&ctx, ext_r, mrp_resolver_t *, r) == 0,
               "failed to set resolver extension");
        VERIFY(mrp_extension_set(&ctx, ext_bl, char *, bl) == 0,
               "failed to set blacklist extension");
        VERIFY(mrp_extension_set(&ctx, ext_wl, char *, wl) == 0,
               "failed to set whitelist extension");
        VERIFY(mrp_extension_set(&ctx, ext_lua, void *, lua) == 0,
               "failed to set lua extension");

        mrp_log_info("extensions set successfully...");

        VERIFY(MRP_EXTENSION(&ctx, ext_r, mrp_resolver_t *) == r,
               "extension check failed for resolver");
        VERIFY(MRP_EXTENSION(&ctx, ext_wl, char *) == wl,
               "extension check failed for whitelist");
        VERIFY(MRP_EXTENSION(&ctx, ext_bl, char *) == bl,
               "extension check failed for blacklist");
        VERIFY(MRP_EXTENSION(&ctx, ext_lua, void *) == lua,
               "extension check failed for lua");

        mrp_log_info("extensions retrieved successfully...");

        VERIFY(mrp_extension_get(&ctx, mrp_resolver_t *, ext_r) == r,
               "extension check failed for resolver");
        VERIFY(mrp_extension_get(&ctx, char *, ext_wl) == wl,
               "extension check failed for whitelist");
        VERIFY(mrp_extension_get(&ctx, char *, ext_bl) == bl,
               "extension check failed for blacklist");
        VERIFY(mrp_extension_get(&ctx, void *, ext_lua) == lua,
               "extension check failed for lua");

        mrp_log_info("extensions retrieved successfully...");
    }

    VERIFY(mrp_extension_set(&ctx, 213, void *, NULL) != 0,
           "setting invalid extension did not fail !");

    VERIFY(mrp_extension_set(&ctx, ext_r, char *, r) != 0,
           "extension type check should have failed !");

    mrp_extension_typecheck(ext_r, false);

    VERIFY(mrp_extension_set(&ctx, ext_r, char *, r) == 0,
           "extension type check shouldn't have failed !");

    mrp_extension_typecheck(ext_r, true);

    VERIFY(mrp_extension_set(&ctx, ext_r, char *, r) != 0,
           "extension type check should have failed !");

    mrp_extension_free_all(&ctx, context_id);

    return 0;
}
