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
