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
#include <errno.h>

#include <Ecore.h>

#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>


typedef struct {
    int ecore;
} ecore_glue_t;


typedef struct {
    Ecore_Fd_Handler *ec_io;
    void            (*cb)(void *glue_data,
                          void *id, int fd, mrp_io_event_t events,
                          void *user_data);
    mrp_io_event_t    mask;
    void             *user_data;
    void             *glue_data;
} io_t;


typedef struct {
    Ecore_Timer *ec_t;
    void       (*cb)(void *glue_data, void *id, void *user_data);
    void        *user_data;
    void        *glue_data;
} tmr_t;


typedef struct {
    Ecore_Timer *ec_t;
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


static int io_check_hup(int fd)
{
    char buf[1];
    int  saved_errno, n;

    saved_errno = errno;
    n = recv(fd, buf, 1, MSG_PEEK);
    errno = saved_errno;

    return (n == 0);
}


static Eina_Bool io_cb(void *user_data, Ecore_Fd_Handler *ec_io)
{
    io_t           *io     = (io_t *)user_data;
    mrp_io_event_t  events = MRP_IO_EVENT_NONE;
    int             fd     = ecore_main_fd_handler_fd_get(ec_io);

    if (ecore_main_fd_handler_active_get(ec_io, ECORE_FD_READ))
        events |= MRP_IO_EVENT_IN;
    if (ecore_main_fd_handler_active_get(ec_io, ECORE_FD_WRITE))
        events |= MRP_IO_EVENT_OUT;
    if (ecore_main_fd_handler_active_get(ec_io, ECORE_FD_ERROR))
        events |= MRP_IO_EVENT_ERR;

#if 0 /* Pfoof... ecore cannot monitor for HUP. */
    if (ecore_main_fd_handler_active_get(ec_io, NO_SUCH_ECORE_EVENT))
        events |= MRP_IO_EVENT_HUP;
#else
    if ((io->mask & MRP_IO_EVENT_HUP) && (events & MRP_IO_EVENT_IN))
        if (io_check_hup(fd))
            events |= MRP_IO_EVENT_HUP;
#endif

    io->cb(io->glue_data, io, fd, events, io->user_data);

    return ECORE_CALLBACK_RENEW;
}


static void *add_io(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data)
{
    int   mask = 0;
    io_t *io;

    io = mrp_allocz(sizeof(*io));

    if (io != NULL) {
        if (events & MRP_IO_EVENT_IN)  mask |= ECORE_FD_READ;
        if (events & MRP_IO_EVENT_OUT) mask |= ECORE_FD_WRITE;
#if 0 /* Pfoof... ecore cannot monitor for HUP. */
        if (events & MRP_IO_EVENT_HUP) mask |= NO_SUCH_ECORE_EVENT;
#endif
        if (events & MRP_IO_EVENT_ERR) mask |= ECORE_FD_ERROR;

        io->mask  = events;
        io->ec_io = ecore_main_fd_handler_add(fd, mask, io_cb, io, NULL, NULL);

        if (io->ec_io != NULL) {
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
    io_t *io   = (io_t *)id;

    MRP_UNUSED(glue_data);

    ecore_main_fd_handler_del(io->ec_io);
    mrp_free(io);
}


static Eina_Bool timer_cb(void *user_data)
{
    tmr_t *t = (tmr_t *)user_data;

    t->cb(t->glue_data, t, t->user_data);

    return ECORE_CALLBACK_RENEW;
}


static void *add_timer(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    double  interval = (1.0 * msecs) / 1000.0;
    tmr_t  *t;

    t = mrp_allocz(sizeof(*t));

    if (t != NULL) {
        t->ec_t = ecore_timer_add(interval, timer_cb, t);

        if (t->ec_t != NULL) {
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

    ecore_timer_del(t->ec_t);
    mrp_free(t);
}


static void mod_timer(void *glue_data, void *id, unsigned int msecs)
{
    tmr_t  *t        = (tmr_t *)id;
    double  interval = (1.0 * msecs) / 1000.0;

    MRP_UNUSED(glue_data);

    if (t != NULL) {
        /*
         * Notes:
         *     ecore_timer_reset needs to be called after updating the
         *     interval. Otherwise the change will not be effective.
         *
         *     In practice, since we use mod_timer to update our super-
         *     loop briding timer to latch at the next mrp_mainloop_t
         *     timer moment, this could cause our mainloop to hang
         *     right in the beginning, since the bridging timer has an
         *     initial interval of (uint32_t)-1 (no next event). If we
         *     have no other event sources than timers this would cause
         *     our mainloop to hang indefinitely. If we have other event
         *     sources (I/O or signals), the mainloop would hang till a
         *     non-timer event comes in.
         */
        ecore_timer_interval_set(t->ec_t, interval);
        ecore_timer_reset(t->ec_t);
    }
}


static Eina_Bool defer_cb(void *user_data)
{
    dfr_t *d = (dfr_t *)user_data;

    d->cb(d->glue_data, d, d->user_data);

    return ECORE_CALLBACK_RENEW;
}


static void *add_defer(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    dfr_t *d;

    d = mrp_allocz(sizeof(*d));

    if (d != NULL) {
        d->ec_t = ecore_timer_add(0.0, defer_cb, d);

        if (d->ec_t != NULL) {
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

    ecore_timer_del(d->ec_t);
    mrp_free(d);
}


static void mod_defer(void *glue_data, void *id, int enabled)
{
    dfr_t *d = (dfr_t *)id;

    MRP_UNUSED(glue_data);

    if (enabled)
        ecore_timer_thaw(d->ec_t);
    else
        ecore_timer_freeze(d->ec_t);
}


static void unregister(void *data)
{
    MRP_UNUSED(data);
}


static mrp_superloop_ops_t ecore_ops = {
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


static const char     *ecore_glue = "murphy-ecore-glue";
static mrp_mainloop_t *ecore_ml;


int mrp_mainloop_register_with_ecore(mrp_mainloop_t *ml)
{
    return mrp_set_superloop(ml, &ecore_ops, (void *)ecore_glue);
}


int mrp_mainloop_unregister_from_ecore(mrp_mainloop_t *ml)
{
    return mrp_mainloop_unregister(ml);
}


mrp_mainloop_t *mrp_mainloop_ecore_get(void)
{
    if (ecore_ml == NULL) {
        ecore_init();

        ecore_ml = mrp_mainloop_create();

        if (ecore_ml != NULL) {
            if (!mrp_mainloop_register_with_ecore(ecore_ml)) {
                mrp_mainloop_destroy(ecore_ml);
                ecore_ml = NULL;
            }
        }
    }

    return ecore_ml;
}
