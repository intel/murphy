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

#ifdef GLIB_ENABLED

#include <murphy/common/glib-glue.h>

struct glib_config_s {
    GMainLoop *gml;
};


mrp_mainloop_t *glib_mainloop_create(test_config_t *cfg)
{
    glib_config_t  *glib;
    mrp_mainloop_t *ml;

    glib = mrp_allocz(sizeof(*glib));

    if (glib != NULL) {
        glib->gml = g_main_loop_new(NULL, FALSE);
        ml        = mrp_mainloop_glib_get(glib->gml);

        if (ml != NULL) {
            cfg->glib = glib;
            cfg->ml   = ml;

            return ml;
        }
        else {
            g_main_loop_unref(glib->gml);
            mrp_free(glib);
        }
    }

    return NULL;
}


int glib_mainloop_run(test_config_t *cfg)
{
    if (cfg->glib != NULL) {
        g_main_loop_run(cfg->glib->gml);
        return TRUE;
    }
    else
        return FALSE;
}


int glib_mainloop_quit(test_config_t *cfg)
{
    if (cfg->glib != NULL) {
        g_main_loop_quit(cfg->glib->gml);
        return TRUE;
    }
    else
        return FALSE;
}


int glib_mainloop_cleanup(test_config_t *cfg)
{
    if (cfg->glib != NULL) {
        mrp_mainloop_unregister(cfg->ml);
        mrp_mainloop_destroy(cfg->ml);
        cfg->ml = NULL;

        g_main_loop_unref(cfg->glib->gml);
        mrp_free(cfg->glib);
        cfg->glib = NULL;

        return TRUE;
    }
    else
        return FALSE;
}


#else


mrp_mainloop_t *glib_mainloop_create(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("glib mainloop support is not available.");
    exit(1);
}


int glib_mainloop_run(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("glib mainloop support is not available.");
    exit(1);
}


int glib_mainloop_quit(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("glib mainloop support is not available.");
    exit(1);
}


int glib_mainloop_cleanup(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("glib mainloop support is not available.");
    exit(1);
}


#endif
