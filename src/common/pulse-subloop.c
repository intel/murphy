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

#include <stdbool.h>
#include <sys/time.h>

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/pulse-subloop.h>


struct pa_murphy_mainloop {
    mrp_mainloop_t  *ml;
    pa_mainloop_api  api;
    mrp_list_hook_t  io_events;
    mrp_list_hook_t  time_events;
    mrp_list_hook_t  defer_events;
    mrp_list_hook_t  io_dead;
    mrp_list_hook_t  time_dead;
    mrp_list_hook_t  defer_dead;
};


struct pa_io_event {
    pa_murphy_mainloop       *m;
    int                       fd;
    mrp_io_watch_t           *w;
    pa_io_event_cb_t          cb;
    pa_io_event_destroy_cb_t  destroy;
    void                     *userdata;
    mrp_list_hook_t           hook;
    int                       busy : 1;
    int                       dead : 1;
};


struct pa_time_event {
    pa_murphy_mainloop         *m;
    mrp_timer_t                *t;
    struct timeval              tv;
    pa_time_event_cb_t          cb;
    pa_time_event_destroy_cb_t  destroy;
    void                       *userdata;
    mrp_list_hook_t             hook;
    int                         busy : 1;
    int                         dead : 1;
};


struct pa_defer_event {
    pa_murphy_mainloop          *m;
    mrp_deferred_t              *d;
    pa_defer_event_cb_t          cb;
    pa_defer_event_destroy_cb_t  destroy;
    void                        *userdata;
    mrp_list_hook_t              hook;
    int                          busy : 1;
    int                          dead : 1;
};


pa_murphy_mainloop *pa_murphy_mainloop_new(mrp_mainloop_t *ml)
{
    pa_murphy_mainloop *m;

    if (ml == NULL)
        return NULL;

    m = mrp_allocz(sizeof(*m));

    if (m == NULL)
        return NULL;

    m->ml = ml;
    mrp_list_init(&m->io_events);
    mrp_list_init(&m->time_events);
    mrp_list_init(&m->defer_events);
    mrp_list_init(&m->io_dead);
    mrp_list_init(&m->time_dead);
    mrp_list_init(&m->defer_dead);

    return m;
}


static void cleanup_io_events(pa_murphy_mainloop *m)
{
    mrp_list_hook_t *p, *n;
    pa_io_event     *io;

    mrp_list_foreach(&m->io_events, p, n) {
        io = mrp_list_entry(p, typeof(*io), hook);

        mrp_list_delete(&io->hook);
        mrp_del_io_watch(io->w);
        io->w = NULL;

        if (io->destroy != NULL) {
            io->dead = true;
            io->destroy(&io->m->api, io, io->userdata);
        }

        mrp_free(io);
    }

    mrp_list_foreach(&m->io_dead, p, n) {
        io = mrp_list_entry(p, typeof(*io), hook);
        mrp_list_delete(&io->hook);
        mrp_free(io);
    }
}


static void cleanup_time_events(pa_murphy_mainloop *m)
{
    mrp_list_hook_t *p, *n;
    pa_time_event   *t;

    mrp_list_foreach(&m->time_events, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        mrp_list_delete(&t->hook);
        mrp_del_timer(t->t);
        t->t = NULL;

        if (t->destroy != NULL) {
            t->dead = true;
            t->destroy(&t->m->api, t, t->userdata);
        }

        mrp_free(t);
    }

    mrp_list_foreach(&m->time_dead, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);
        mrp_list_delete(&t->hook);
        mrp_free(t);
    }
}


static void cleanup_defer_events(pa_murphy_mainloop *m)
{
    mrp_list_hook_t *p, *n;
    pa_defer_event  *d;

    mrp_list_foreach(&m->defer_events, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);

        mrp_list_delete(&d->hook);
        mrp_del_deferred(d->d);
        d->d = NULL;

        if (d->destroy != NULL) {
            d->dead = true;
            d->destroy(&d->m->api, d, d->userdata);
        }

        mrp_free(d);
    }

    mrp_list_foreach(&m->defer_dead, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);
        mrp_list_delete(&d->hook);
        mrp_free(d);
    }
}


void pa_murphy_mainloop_free(pa_murphy_mainloop *m)
{
    if (m == NULL)
        return;

    cleanup_io_events(m);
    cleanup_time_events(m);
    cleanup_defer_events(m);
}


static void io_event_cb(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
                        void *userdata)
{
    pa_io_event         *io    = (pa_io_event *)userdata;
    pa_io_event_flags_t  flags = 0;

    MRP_UNUSED(w);

    mrp_debug("PA I/O event 0x%x for watch %p (fd %d)", events, io, fd);

    if (events & MRP_IO_EVENT_IN)  flags |= PA_IO_EVENT_INPUT;
    if (events & MRP_IO_EVENT_OUT) flags |= PA_IO_EVENT_OUTPUT;
    if (events & MRP_IO_EVENT_HUP) flags |= PA_IO_EVENT_HANGUP;
    if (events & MRP_IO_EVENT_ERR) flags |= PA_IO_EVENT_ERROR;

    io->busy = true;
    io->cb(&io->m->api, io, fd, flags, io->userdata);
    io->busy = false;

    if (io->dead) {
        mrp_list_delete(&io->hook);
        mrp_free(io);
    }
}


static pa_io_event *io_new(pa_mainloop_api *api, int fd, pa_io_event_flags_t e,
                           pa_io_event_cb_t cb, void *userdata)
{
    pa_murphy_mainloop *m      = (pa_murphy_mainloop *)api->userdata;
    mrp_io_event_t      events = 0;
    pa_io_event        *io;

    mrp_debug("PA create I/O watch for fd %d, events 0x%x", fd, e);

    io = mrp_allocz(sizeof(*io));

    if (io == NULL)
        return NULL;

    mrp_list_init(&io->hook);

    if (e & PA_IO_EVENT_INPUT)  events |= MRP_IO_EVENT_IN;
    if (e & PA_IO_EVENT_OUTPUT) events |= MRP_IO_EVENT_OUT;
    if (e & PA_IO_EVENT_HANGUP) events |= MRP_IO_EVENT_HUP; /* RDHUP ? */
    if (e & PA_IO_EVENT_ERROR)  events |= MRP_IO_EVENT_ERR;

    io->m        = m;
    io->fd       = fd;
    io->cb       = cb;
    io->userdata = userdata;
    io->w        = mrp_add_io_watch(m->ml, fd, events, io_event_cb, io);

    if (io->w != NULL)
        mrp_list_append(&m->io_events, &io->hook);
    else {
        mrp_free(io);
        io = NULL;
    }

    return io;
}


static void io_enable(pa_io_event *io, pa_io_event_flags_t e)
{
    pa_murphy_mainloop *m      = io->m;
    mrp_io_event_t      events = 0;

    mrp_debug("PA enable events 0x%x for I/O watch %p (fd %d)", e, io, io->fd);

    mrp_del_io_watch(io->w);
    io->w = NULL;

    if (e & PA_IO_EVENT_INPUT)  events |= MRP_IO_EVENT_IN;
    if (e & PA_IO_EVENT_OUTPUT) events |= MRP_IO_EVENT_OUT;
    if (e & PA_IO_EVENT_HANGUP) events |= MRP_IO_EVENT_HUP; /* RDHUP ? */
    if (e & PA_IO_EVENT_ERROR)  events |= MRP_IO_EVENT_ERR;

    io->w = mrp_add_io_watch(m->ml, io->fd, events, io_event_cb, io);
}


static void io_free(pa_io_event *io)
{
    pa_murphy_mainloop *m = io->m;

    mrp_debug("PA free I/O watch %p (fd %d)", io, io->fd);

    mrp_list_delete(&io->hook);
    mrp_del_io_watch(io->w);
    io->w = NULL;

    io->dead = true;

    if (!io->busy && !io->dead) {
        io->busy = true;
        if (io->destroy != NULL)
            io->destroy(&io->m->api, io, io->userdata);
        mrp_free(io);
    }
    else
        mrp_list_append(&m->io_dead, &io->hook);
}


static void io_set_destroy(pa_io_event *io, pa_io_event_destroy_cb_t cb)
{
    mrp_debug("PA set I/O watch destroy callback for %p (fd %d) to %p",
              io, io->fd, cb);

    io->destroy = cb;
}




static void time_event_cb(mrp_timer_t *tmr, void *userdata)
{
    pa_time_event *t = (pa_time_event *)userdata;

    MRP_UNUSED(tmr);

    mrp_debug("PA time event for timer %p", t);

    mrp_del_timer(t->t);
    t->t = NULL;

    t->busy = true;
    t->cb(&t->m->api, t, &t->tv, t->userdata);
    t->busy = false;

    if (t->dead) {
        mrp_del_timer(t->t);
        mrp_list_delete(&t->hook);
        mrp_free(t);
    }
}


static unsigned int timeval_diff(const struct timeval *from,
                                 const struct timeval *to)
{
    int msecs, musecs, diff;

    msecs  = (to->tv_sec - from->tv_sec) * 1000;
    musecs = ((int)to->tv_usec - (int)from->tv_usec) / 1000;

    diff = msecs + musecs;

    if (diff >= 0)
        return (unsigned int)diff;
    else
        return 0;
}


static pa_time_event *time_new(pa_mainloop_api *api, const struct timeval *tv,
                               pa_time_event_cb_t cb, void *userdata)
{
    pa_murphy_mainloop *m = (pa_murphy_mainloop *)api->userdata;
    pa_time_event      *t;
    struct timeval      now;

    gettimeofday(&now, NULL);

    mrp_debug("PA create timer for %u msecs", timeval_diff(&now, tv));

    t = mrp_allocz(sizeof(*t));

    if (t == NULL)
        return NULL;

    mrp_list_init(&t->hook);

    t->m        = m;
    t->cb       = cb;
    t->userdata = userdata;
    t->t        = mrp_add_timer(m->ml, timeval_diff(&now, tv), time_event_cb, t);

    if (t->t != NULL)
        mrp_list_append(&m->time_events, &t->hook);
    else {
        mrp_free(t);
        t = NULL;
    }

    return t;
}


static void time_restart(pa_time_event *t, const struct timeval *tv)
{
    pa_murphy_mainloop *m = t->m;
    struct timeval      now;

    gettimeofday(&now, NULL);

    mrp_debug("PA restart timer %p with %u msecs", t, timeval_diff(&now, tv));

    mrp_del_timer(t->t);
    t->t = NULL;

    t->t = mrp_add_timer(m->ml, timeval_diff(&now, tv), time_event_cb, t);
}


static void time_free(pa_time_event *t)
{
    pa_murphy_mainloop *m = t->m;

    mrp_debug("PA free timer %p",  t);

    mrp_list_delete(&t->hook);
    mrp_del_timer(t->t);
    t->t = NULL;

    t->dead = true;

    if (!t->busy && !t->dead) {
        t->busy = true;
        if (t->destroy != NULL)
            t->destroy(&t->m->api, t, t->userdata);
        mrp_free(t);
    }
    else
        mrp_list_append(&m->time_dead, &t->hook);
}


static void time_set_destroy(pa_time_event *t, pa_time_event_destroy_cb_t cb)
{
    mrp_debug("PA set timer destroy callback for %p to %p", t, cb);

    t->destroy = cb;
}




static void defer_event_cb(mrp_deferred_t *def, void *userdata)
{
    pa_defer_event *d = (pa_defer_event *)userdata;

    MRP_UNUSED(def);

    mrp_debug("PA defer event for %p", d);

    d->busy = true;
    d->cb(&d->m->api, d, d->userdata);
    d->busy = false;

    if (d->dead) {
        mrp_del_deferred(d->d);
        mrp_list_delete(&d->hook);
        mrp_free(d);
    }
}


static pa_defer_event *defer_new(pa_mainloop_api *api, pa_defer_event_cb_t cb,
                                 void *userdata)
{
    pa_murphy_mainloop *m = (pa_murphy_mainloop *)api->userdata;
    pa_defer_event     *d;

    mrp_debug("PA create defer event");

    d = mrp_allocz(sizeof(*d));

    if (d == NULL)
        return NULL;

    mrp_list_init(&d->hook);

    d->m        = m;
    d->cb       = cb;
    d->userdata = userdata;
    d->d        = mrp_add_deferred(m->ml, defer_event_cb, d);

    if (d->d != NULL)
        mrp_list_append(&m->defer_events, &d->hook);
    else {
        mrp_free(d);
        d = NULL;
    }

    return d;
}


static void defer_enable(pa_defer_event *d, int enable)
{
    mrp_debug("PA %s defer event %p", enable ? "enable" : "disable", d);

    if (enable)
        mrp_enable_deferred(d->d);
    else
        mrp_disable_deferred(d->d);
}


static void defer_free(pa_defer_event *d)
{
    pa_murphy_mainloop *m = d->m;

    mrp_debug("PA free defer event %p", d);

    mrp_list_delete(&d->hook);
    mrp_del_deferred(d->d);
    d->d = NULL;

    d->dead = true;

    if (!d->busy && !d->dead) {
        d->busy = true;
        if (d->destroy != NULL)
            d->destroy(&d->m->api, d, d->userdata);
        mrp_free(d);
    }
    else
        mrp_list_append(&m->defer_dead, &d->hook);
}


static void defer_set_destroy(pa_defer_event *d, pa_defer_event_destroy_cb_t cb)
{
    mrp_debug("PA set defer event destroy callback for %p to %p", d, cb);

    d->destroy = cb;
}


static void quit(pa_mainloop_api *api, int retval)
{
    pa_murphy_mainloop *m = (pa_murphy_mainloop *)api->userdata;

    mrp_mainloop_quit(m->ml, retval);
}



pa_mainloop_api *pa_murphy_mainloop_get_api(pa_murphy_mainloop *m)
{
    pa_mainloop_api api = {
        .userdata          = m,
        .io_new            = io_new,
        .io_enable         = io_enable,
        .io_free           = io_free,
        .io_set_destroy    = io_set_destroy,
        .time_new          = time_new,
        .time_restart      = time_restart,
        .time_free         = time_free,
        .time_set_destroy  = time_set_destroy,
        .defer_new         = defer_new,
        .defer_enable      = defer_enable,
        .defer_free        = defer_free,
        .defer_set_destroy = defer_set_destroy,
        .quit              = quit
    };

    m->api = api;

    return &m->api;
}
