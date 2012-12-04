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
    MRP_UNUSED(cfg);

    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_run(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_quit(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


int ecore_mainloop_cleanup(test_config_t *cfg)
{
    MRP_UNUSED(cfg);

    mrp_log_error("EFL/ecore mainloop support is not available.");
    exit(1);
}


#endif
