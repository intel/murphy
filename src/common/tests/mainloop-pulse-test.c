#ifdef PULSE_ENABLED

#include <murphy/common/pulse-glue.h>

struct pulse_config_s {
    pa_mainloop     *pa_main;
    pa_mainloop_api *pa;
};


mrp_mainloop_t *pulse_mainloop_create(test_config_t *cfg)
{
    pulse_config_t *pulse;
    mrp_mainloop_t *ml;

    pulse = mrp_allocz(sizeof(*pulse));

    if (pulse != NULL) {
        pulse->pa_main = pa_mainloop_new();
        pulse->pa      = pa_mainloop_get_api(pulse->pa_main);
        ml             = mrp_mainloop_pulse_get(pulse->pa);

        if (ml != NULL) {
            cfg->pulse = pulse;
            cfg->ml    = ml;

            return ml;
        }
        else {
            pa_mainloop_free(pulse->pa_main);
            mrp_free(pulse);
        }
    }

    return NULL;
}


int pulse_mainloop_run(test_config_t *cfg)
{
    int retval;

    if (cfg->pulse && cfg->pulse->pa != NULL) {
        pa_mainloop_run(cfg->pulse->pa_main, &retval);

        return TRUE;
    }
    else
        return FALSE;
}


int pulse_mainloop_quit(test_config_t *cfg)
{
    if (cfg->pulse && cfg->pulse->pa) {
        pa_mainloop_quit(cfg->pulse->pa_main, 0);
        return TRUE;
    }
    else
        return FALSE;
}


int pulse_mainloop_cleanup(test_config_t *cfg)
{
    if (cfg->pulse != NULL) {
        mrp_mainloop_unregister(cfg->ml);
        mrp_mainloop_destroy(cfg->ml);
        cfg->ml = NULL;

        pa_mainloop_free(cfg->pulse->pa_main);
        mrp_free(cfg->pulse);
        cfg->pulse = NULL;

        return TRUE;
    }
    else
        return FALSE;
}


#else


mrp_mainloop_t *pulse_mainloop_create(test_config_t *cfg)
{
    mrp_log_error("PulseAudio mainloop support is not available.");
    exit(1);
}


int pulse_mainloop_run(test_config_t *cfg)
{
    mrp_log_error("PulseAudio mainloop support is not available.");
    exit(1);
}


int pulse_mainloop_quit(test_config_t *cfg)
{
    mrp_log_error("PulseAudio mainloop support is not available.");
    exit(1);
}


int pulse_mainloop_cleanup(test_config_t *cfg)
{
    mrp_log_error("PulseAudio mainloop support is not available.");
    exit(1);
}


#endif
