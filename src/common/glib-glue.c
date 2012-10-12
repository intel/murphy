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

#include <sys/types.h>
#include <sys/socket.h>

#include <glib.h>

#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>


typedef struct {
    GMainLoop *gml;
} glib_glue_t;


typedef struct {
    GIOChannel        *gl_ioc;
    guint              gl_iow;
    void             (*cb)(void *glue_data,
                           void *id, int fd, mrp_io_event_t events,
                           void *user_data);
    mrp_io_event_t     mask;
    void              *user_data;
    void              *glue_data;
} io_t;


typedef struct {
    guint        gl_t;
    void       (*cb)(void *glue_data, void *id, void *user_data);
    void        *user_data;
    void        *glue_data;
} tmr_t;


typedef struct {
    guint        gl_t;
    void       (*cb)(void *glue_data, void *id, void *user_data);
    void        *user_data;
    void        *glue_data;
} dfr_t;


#define D(fmt, args...) do {                                     \
        printf("* [%s]: "fmt"\n", __FUNCTION__ , ## args);       \
    } while (0)


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


static gboolean io_cb(GIOChannel *ioc, GIOCondition cond, gpointer user_data)
{
    io_t           *io     = (io_t *)user_data;
    mrp_io_event_t  events = MRP_IO_EVENT_NONE;
    int             fd     = g_io_channel_unix_get_fd(ioc);

    if (cond & G_IO_IN)
        events |= MRP_IO_EVENT_IN;
    if (cond & G_IO_OUT)
        events |= MRP_IO_EVENT_OUT;
    if (cond & G_IO_ERR)
        events |= MRP_IO_EVENT_ERR;
    if (cond & G_IO_HUP)
        events |= MRP_IO_EVENT_HUP;

    io->cb(io->glue_data, io, fd, events, io->user_data);

    return TRUE;
}


static void *add_io(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data)
{
    GIOCondition  mask = 0;
    GIOChannel   *ioc;
    io_t         *io;

    ioc = g_io_channel_unix_new(fd);

    if (ioc == NULL)
        return NULL;

    io = mrp_allocz(sizeof(*io));

    if (io != NULL) {
        if (events & MRP_IO_EVENT_IN ) mask |= G_IO_IN;
        if (events & MRP_IO_EVENT_OUT) mask |= G_IO_OUT;
        if (events & MRP_IO_EVENT_HUP) mask |= G_IO_HUP;
        if (events & MRP_IO_EVENT_ERR) mask |= G_IO_ERR;

        io->mask   = events;
        io->gl_ioc = ioc;
        io->gl_iow = g_io_add_watch(ioc, mask, io_cb, io);

        if (io->gl_iow != 0) {
            io->cb        = cb;
            io->user_data = user_data;
            io->glue_data = glue_data;

            return io;
        }
        else {
            g_io_channel_unref(ioc);
            mrp_free(io);
        }
    }

    return NULL;
}


static void del_io(void *glue_data, void *id)
{
    io_t *io = (io_t *)id;

    MRP_UNUSED(glue_data);

    g_source_remove(io->gl_iow);
    g_io_channel_unref(io->gl_ioc);
    mrp_free(io);
}


static gboolean timer_cb(gpointer user_data)
{
    tmr_t *t = (tmr_t *)user_data;

    t->cb(t->glue_data, t, t->user_data);

    return TRUE;
}


static void *add_timer(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    tmr_t *t;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        t->gl_t = g_timeout_add(msecs, timer_cb, t);

        if (t->gl_t != 0) {
            t->cb        = cb;
            t->user_data = user_data;
            t->glue_data = glue_data;

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

    g_source_remove(t->gl_t);
    mrp_free(t);
}


static void mod_timer(void *glue_data, void *id, unsigned int msecs)
{
    tmr_t  *t = (tmr_t *)id;

    MRP_UNUSED(glue_data);

    if (t != NULL) {
        g_source_remove(t->gl_t);
        t->gl_t = g_timeout_add(msecs, timer_cb, t);
    }
}


static gboolean defer_cb(void *user_data)
{
    dfr_t *d = (dfr_t *)user_data;

    d->cb(d->glue_data, d, d->user_data);

    return TRUE;
}


static void *add_defer(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    dfr_t *d;

    d = mrp_allocz(sizeof(*d));

    if (d != NULL) {
        d->gl_t = g_timeout_add(0, defer_cb, d);

        if (d->gl_t != 0) {
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

    if (d->gl_t != 0)
        g_source_remove(d->gl_t);

    mrp_free(d);
}


static void mod_defer(void *glue_data, void *id, int enabled)
{
    dfr_t *d = (dfr_t *)id;

    MRP_UNUSED(glue_data);

    if (enabled && !d->gl_t)
        d->gl_t = g_timeout_add(0, defer_cb, d);
    else if (!enabled && d->gl_t) {
        g_source_remove(d->gl_t);
        d->gl_t = 0;
    }
}


static void unregister(void *data)
{
    glib_glue_t *glue = (glib_glue_t *)data;

    g_main_loop_unref(glue->gml);

    mrp_free(glue);
}


static mrp_superloop_ops_t glib_ops = {
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


int mrp_mainloop_register_with_glib(mrp_mainloop_t *ml, GMainLoop *gml)
{
    glib_glue_t *glue;

    glue = mrp_allocz(sizeof(*glue));

    if (glue != NULL) {
        glue->gml = g_main_loop_ref(gml);

        if (mrp_set_superloop(ml, &glib_ops, glue))
            return TRUE;
        else {
            g_main_loop_unref(gml);
            mrp_free(glue);
        }
    }

    return FALSE;
}


int mrp_mainloop_unregister_from_glib(mrp_mainloop_t *ml)
{
    return mrp_mainloop_unregister(ml);
}


mrp_mainloop_t *mrp_mainloop_glib_get(GMainLoop *gml)
{
    mrp_mainloop_t *ml;

    if (gml != NULL) {
        ml = mrp_mainloop_create();

        if (ml != NULL) {
            if (mrp_mainloop_register_with_glib(ml, gml))
                return ml;
            else
                mrp_mainloop_destroy(ml);
        }
    }

    return NULL;
}
