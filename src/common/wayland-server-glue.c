/*
 * Copyright (c) 2012-2014, Intel Corporation
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

#include <limits.h>
#include <wayland-server.h>

#include "murphy/common/mm.h"
#include "murphy/common/mainloop.h"

#include "murphy/common/wayland-server-glue.h"


typedef struct {
    struct wl_event_loop *wl;
} wayland_glue_t;


typedef struct {
    struct wl_event_source  *wl_io;
    void                   (*cb)(void *glue_data,
                                 void *id, int fd, mrp_io_event_t events,
                                 void *user_data);
    void                    *user_data;
    void                    *glue_data;
} io_t;


typedef struct {
    struct wl_event_source  *wl_t;
    void                   (*cb)(void *glue_data, void *id, void *user_data);
    void                    *user_data;
    void                    *glue_data;
} tmr_t;


typedef struct {
    struct wl_event_source  *wl_d;
    void                   (*cb)(void *glue_data, void *id, void *user_data);
    void                    *user_data;
    void                    *glue_data;
    int                      enabled;
} dfr_t;


static void *add_io(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data);
static void  del_io(void *glue_data, void *id);

static void *add_timer(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
static void  del_timer(void *glue_data, void *id);
static void  mod_timer(void *glue_data, void *id, unsigned int msecs);

static void *add_defer(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
static void  del_defer(void *glue_data, void *id);
static void  mod_defer(void *glue_data, void *id, int enabled);



static int io_cb(int fd, uint32_t mask, void *data)
{
    io_t           *io     = (io_t *)data;
    mrp_io_event_t  events = MRP_IO_EVENT_NONE;

    if (mask & WL_EVENT_READABLE) events |= MRP_IO_EVENT_IN;
    if (mask & WL_EVENT_WRITABLE) events |= MRP_IO_EVENT_OUT;
    if (mask & WL_EVENT_HANGUP)   events |= MRP_IO_EVENT_HUP;
    if (mask & WL_EVENT_ERROR)    events |= MRP_IO_EVENT_ERR;

    io->cb(io->glue_data, io, fd, events, io->user_data);

    return 1;
}


static void *add_io(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data)
{
    wayland_glue_t       *glue = (wayland_glue_t *)glue_data;
    struct wl_event_loop *wl   = glue->wl;
    uint32_t              mask = 0;
    io_t                 *io;

    io = mrp_allocz(sizeof(*io));

    if (io != NULL) {
        if (events & MRP_IO_EVENT_IN)  mask |= WL_EVENT_READABLE;
        if (events & MRP_IO_EVENT_OUT) mask |= WL_EVENT_WRITABLE;
        if (events & MRP_IO_EVENT_HUP) mask |= WL_EVENT_HANGUP;
        if (events & MRP_IO_EVENT_ERR) mask |= WL_EVENT_ERROR;

        io->wl_io = wl_event_loop_add_fd(wl, fd, mask, io_cb, io);

        if (io->wl_io != NULL) {
            io->cb        = cb;
            io->user_data = user_data;
            io->glue_data = glue_data;

            return io;
        }
        else
            mrp_free(io);
    }

    return NULL;
}


static void del_io(void *glue_data, void *id)
{
    io_t *io = (io_t *)id;

    MRP_UNUSED(glue_data);

    wl_event_source_remove(io->wl_io);
    mrp_free(io);
}


static int timer_cb(void *user_data)
{
    tmr_t *t = (tmr_t *)user_data;

    t->cb(t->glue_data, t, t->user_data);

    return 1;
}


static void *add_timer(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    wayland_glue_t       *glue = (wayland_glue_t *)glue_data;
    struct wl_event_loop *wl   = glue->wl;
    tmr_t                *t;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        t->wl_t = wl_event_loop_add_timer(wl, timer_cb, t);

        if (t->wl_t != NULL) {
            t->cb        = cb;
            t->user_data = user_data;
            t->glue_data = glue_data;

            if (msecs == (unsigned int)-1)
                msecs = INT_MAX;

            wl_event_source_timer_update(t->wl_t, msecs);

            return t;
        }
        else
            mrp_free(t);
    }

    return NULL;
}


static void del_timer(void *glue_data, void *id)
{
    tmr_t *t = (tmr_t *)id;

    MRP_UNUSED(glue_data);

    wl_event_source_remove(t->wl_t);
    mrp_free(t);
}


static void mod_timer(void *glue_data, void *id, unsigned int msecs)
{
    tmr_t *t = (tmr_t *)id;

    MRP_UNUSED(glue_data);

    if (t != NULL) {
        if (msecs == (unsigned int)-1)
            msecs = INT_MAX;

        wl_event_source_timer_update(t->wl_t, msecs);
    }
}


static void defer_cb(void *user_data)
{
    dfr_t                *d    = (dfr_t *)user_data;
    wayland_glue_t       *glue = (wayland_glue_t *)d->glue_data;
    struct wl_event_loop *wl   = glue->wl;

    d->cb(d->glue_data, d, d->user_data);
    if (d->enabled)
        d->wl_d = wl_event_loop_add_idle(wl, defer_cb, d);
    else
        d->wl_d = NULL;
}


static void *add_defer(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    wayland_glue_t       *glue = (wayland_glue_t *)glue_data;
    struct wl_event_loop *wl   = glue->wl;
    dfr_t                *d;

    d = mrp_allocz(sizeof(*d));

    if (d != NULL) {
        d->wl_d = wl_event_loop_add_idle(wl, defer_cb, d);

        if (d->wl_d != NULL) {
            d->cb        = cb;
            d->user_data = user_data;
            d->glue_data = glue_data;

            return d;
        }
        else
            mrp_free(d);
    }

    return NULL;
}


static void del_defer(void *glue_data, void *id)
{
    dfr_t *d = (dfr_t *)id;

    MRP_UNUSED(glue_data);

    wl_event_source_remove(d->wl_d);
    mrp_free(d);
}


static void mod_defer(void *glue_data, void *id, int enabled)
{
    wayland_glue_t       *glue = (wayland_glue_t *)glue_data;
    struct wl_event_loop *wl   = glue->wl;
    dfr_t                *d    = (dfr_t *)id;

    if (enabled) {
        if (d->wl_d == NULL) {
            d->wl_d = wl_event_loop_add_idle(wl, defer_cb, d);
        }
    }
    else {
        if (d->wl_d != NULL) {
            wl_event_source_remove(d->wl_d);
            d->wl_d = NULL;
        }
    }
}


static void unregister(void *data)
{
    wayland_glue_t *glue = (wayland_glue_t *)data;

    mrp_free(glue);
}


static mrp_superloop_ops_t wl_ops = {
    .add_io     = add_io,
    .del_io     = del_io,
    .add_timer  = add_timer,
    .del_timer  = del_timer,
    .mod_timer  = mod_timer,
    .add_defer  = add_defer,
    .del_defer  = del_defer,
    .mod_defer  = mod_defer,
    .unregister = unregister,
};


int mrp_mainloop_register_with_wayland(mrp_mainloop_t *ml,
                                       struct wl_event_loop *wl)
{
    wayland_glue_t *glue;

    glue = mrp_allocz(sizeof(*glue));

    if (glue != NULL) {
        glue->wl = wl;

        if (mrp_set_superloop(ml, &wl_ops, glue))
            return TRUE;
        else
            mrp_free(glue);
    }

    return FALSE;
}


int mrp_mainloop_unregister_from_wayland(mrp_mainloop_t *ml)
{
    return mrp_mainloop_unregister(ml);
}



static mrp_mainloop_t *wayland_ml;

mrp_mainloop_t *mrp_mainloop_wayland_get(struct wl_event_loop *wl)
{
    if (wayland_ml == NULL) {
        wayland_ml = mrp_mainloop_create();

        if (wayland_ml != NULL) {
            if (!mrp_mainloop_register_with_wayland(wayland_ml, wl)) {
                mrp_mainloop_destroy(wayland_ml);
                wayland_ml = NULL;
            }
        }
    }

    return wayland_ml;
}
