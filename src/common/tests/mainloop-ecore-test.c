#ifdef ECORE_ENABLED

struct ecore_config_s {
    int dummy;
};


mrp_mainloop_t *ecore_mainloop_create(test_config_t *cfg)
{
    cfg->ml = mrp_mainloop_ecore_get();

    return cfg->ml;
}


int ecore_mainloop_run(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    ecore_main_loop_begin();

    return TRUE;
}


int ecore_mainloop_quit(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    ecore_main_loop_quit();

    return TRUE;
}


int ecore_mainloop_cleanup(test_config_t *cfg)
{
    mrp_mainloop_unregister(cfg->ml);
    mrp_mainloop_destroy(cfg->ml);

    cfg->ml = NULL;

    return TRUE;
}


#else


mrp_mainloop_t *ecore_mainloop_create(test_config_t *cfg)
{
    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_run(test_config_t *cfg)
{
    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_quit(test_config_t *cfg)
{
    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_cleanup(test_config_t *cfg)
{
    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


#endif
