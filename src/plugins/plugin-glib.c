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

#include <glib.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>
#include <murphy/core/plugin.h>


/*
 * A simple glue layer to pump GMainLoop from mrp_mainloop_t. This
 * will pretty much be turned into a murphy plugin as such...
 */


typedef struct {
    GMainLoop     *ml;
    GMainContext  *mc;
    gint           maxprio;
    mrp_subloop_t *sl;
} glib_glue_t;

static glib_glue_t *glib_glue;


static int glib_prepare(void *user_data)
{
    glib_glue_t *glue = (glib_glue_t *)user_data;

    return g_main_context_prepare(glue->mc, &glue->maxprio);
}


static int glib_query(void *user_data, struct pollfd *fds, int nfd,
                      int *timeout)
{
    glib_glue_t *glue = (glib_glue_t *)user_data;

    return g_main_context_query(glue->mc, glue->maxprio, timeout,
                                (GPollFD *)fds, nfd);
}


static int glib_check(void *user_data, struct pollfd *fds, int nfd)
{
    glib_glue_t *glue = (glib_glue_t *)user_data;

    return g_main_context_check(glue->mc, glue->maxprio, (GPollFD *)fds, nfd);

}


static void glib_dispatch(void *user_data)
{
    glib_glue_t *glue = (glib_glue_t *)user_data;

    g_main_context_dispatch(glue->mc);

}


static int glib_pump_setup(mrp_mainloop_t *ml)
{
    static mrp_subloop_ops_t glib_ops = {
        .prepare  = glib_prepare,
        .query    = glib_query,
        .check    = glib_check,
        .dispatch = glib_dispatch
    };

    GMainContext *main_context;
    GMainLoop    *main_loop;

    if (sizeof(GPollFD) != sizeof(struct pollfd)) {
        mrp_log_error("sizeof(GPollFD:%zd) != sizeof(struct pollfd:%zd)\n",
                      sizeof(GPollFD), sizeof(struct pollfd));
        return FALSE;
    }

    main_context = NULL;
    main_loop    = NULL;
    glib_glue    = NULL;

    if ((main_context = g_main_context_default())             != NULL &&
        (main_loop    = g_main_loop_new(main_context, FALSE)) != NULL &&
        (glib_glue    = mrp_allocz(sizeof(*glib_glue)))       != NULL) {

        glib_glue->mc = main_context;
        glib_glue->ml = main_loop;
        glib_glue->sl = mrp_add_subloop(ml, &glib_ops, glib_glue);

        if (glib_glue->sl != NULL)
            return TRUE;
        else
            mrp_log_error("glib-pump failed to register subloop.");
    }

    /* all of these handle a NULL argument gracefully... */
    g_main_loop_unref(main_loop);
    g_main_context_unref(main_context);

    mrp_free(glib_glue);
    glib_glue = NULL;

    return FALSE;
}


static void glib_pump_cleanup(void)
{
    if (glib_glue != NULL) {
        mrp_del_subloop(glib_glue->sl);

        g_main_loop_unref(glib_glue->ml);
        g_main_context_unref(glib_glue->mc);

        mrp_free(glib_glue);
        glib_glue = NULL;
    }
}



static int plugin_init(mrp_plugin_t *plugin)
{
    mrp_log_info("%s() called...", __FUNCTION__);

    return glib_pump_setup(plugin->ctx->ml);
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_log_info("%s() called...", __FUNCTION__);

    glib_pump_cleanup();
}


#define GLIB_DESCRIPTION "Glib mainloop pump plugin."
#define GLIB_HELP        "Glib pump plugin (GMainLoop integration)."
#define GLIB_VERSION     MRP_VERSION_INT(0, 0, 1)
#define GLIB_AUTHORS     "Krisztian Litkey <krisztian.litkey@intel.com>"

MURPHY_REGISTER_PLUGIN("glib", GLIB_VERSION, GLIB_DESCRIPTION, GLIB_AUTHORS,
                       GLIB_HELP, MRP_SINGLETON,
                       plugin_init, plugin_exit,
                       NULL, 0, NULL, 0, NULL, 0, NULL);

