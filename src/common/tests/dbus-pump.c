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

#include <dbus/dbus.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>

typedef struct dbus_glue_s dbus_glue_t;

typedef struct {
    dbus_glue_t     *glue;
    mrp_io_watch_t  *mw;
    DBusWatch       *dw;
    mrp_list_hook_t  hook;
} watch_t;


typedef struct {
    dbus_glue_t     *glue;
    mrp_timer_t     *mt;
    DBusTimeout     *dt;
    mrp_list_hook_t  hook;
} timeout_t;


struct dbus_glue_s {
    DBusConnection  *conn;
    mrp_mainloop_t  *ml;
    mrp_list_hook_t  watches;
    mrp_list_hook_t  timers;
    mrp_deferred_t  *pump;
};


static dbus_int32_t data_slot = -1;

static void dispatch_watch(mrp_io_watch_t *mw, int fd, mrp_io_event_t events,
                           void *user_data)
{
    watch_t        *watch = (watch_t *)user_data;
    DBusConnection *conn  = watch->glue->conn;
    unsigned int    mask  = 0;

    MRP_UNUSED(mw);
    MRP_UNUSED(fd);

    if (events & MRP_IO_EVENT_IN)
        mask |= DBUS_WATCH_READABLE;
    if (events & MRP_IO_EVENT_OUT)
        mask |= DBUS_WATCH_WRITABLE;
    if (events & MRP_IO_EVENT_HUP)
        mask |= DBUS_WATCH_HANGUP;
    if (events & MRP_IO_EVENT_ERR)
        mask |= DBUS_WATCH_ERROR;

    dbus_connection_ref(conn);
    dbus_watch_handle(watch->dw, mask);
    dbus_connection_unref(conn);
}


static void watch_freed_cb(void *data)
{
    watch_t *watch = (watch_t *)data;

    if (watch != NULL) {
        mrp_list_delete(&watch->hook);
        mrp_del_io_watch(watch->mw);
        mrp_free(watch);
    }
}


static dbus_bool_t add_watch(DBusWatch *dw, void *data)
{
    dbus_glue_t    *glue = (dbus_glue_t *)data;
    watch_t        *watch;
    mrp_io_watch_t *mw;
    mrp_io_event_t  mask;
    int             fd;
    unsigned int    flags;

    if (!dbus_watch_get_enabled(dw))
        return TRUE;

    fd    = dbus_watch_get_unix_fd(dw);
    flags = dbus_watch_get_flags(dw);
    mask  = MRP_IO_EVENT_HUP | MRP_IO_EVENT_ERR;

    if (flags & DBUS_WATCH_READABLE)
        mask |= MRP_IO_EVENT_IN;
    if (flags & DBUS_WATCH_WRITABLE)
        mask |= MRP_IO_EVENT_OUT;

    if ((watch = mrp_allocz(sizeof(*watch))) != NULL) {
        mrp_list_init(&watch->hook);
        mw = mrp_add_io_watch(glue->ml, fd, mask, dispatch_watch, watch);

        if (mw != NULL) {
            watch->glue = glue;
            watch->mw   = mw;
            watch->dw   = dw;
            dbus_watch_set_data(dw, watch, watch_freed_cb);
            mrp_list_append(&glue->watches, &watch->hook);

            return TRUE;
        }
        else
            mrp_free(watch);
    }

    return FALSE;
}


static void del_watch(DBusWatch *dw, void *data)
{
    watch_t *watch = (watch_t *)dbus_watch_get_data(dw);

    MRP_UNUSED(data);

    if (watch != NULL) {
        mrp_del_io_watch(watch->mw);
        watch->mw = NULL;
    }
}


static void toggle_watch(DBusWatch *dw, void *data)
{
    if (dbus_watch_get_enabled(dw))
        add_watch(dw, data);
    else
        del_watch(dw, data);
}


static void dispatch_timeout(mrp_timer_t *mt, void *user_data)
{
    timeout_t *timer = (timeout_t *)user_data;

    MRP_UNUSED(mt);

    dbus_timeout_handle(timer->dt);
}


static void timeout_freed_cb(void *data)
{
    timeout_t *timer = (timeout_t *)data;

    if (timer != NULL) {
        mrp_list_delete(&timer->hook);
        mrp_del_timer(timer->mt);

        mrp_free(timer);
    }
}


static dbus_bool_t add_timeout(DBusTimeout *dt, void *data)
{
    dbus_glue_t  *glue = (dbus_glue_t *)data;
    timeout_t    *timer;
    mrp_timer_t  *mt;
    unsigned int  msecs;

    if ((timer = mrp_allocz(sizeof(*timer))) != NULL) {
        mrp_list_init(&timer->hook);
        msecs = dbus_timeout_get_interval(dt);
        mt    = mrp_add_timer(glue->ml, msecs, dispatch_timeout, timer);

        if (mt != NULL) {
            timer->glue = glue;
            timer->mt   = mt;
            timer->dt   = dt;
            dbus_timeout_set_data(dt, timer, timeout_freed_cb);
            mrp_list_append(&glue->timers, &timer->hook);

            return TRUE;
        }
        else
            mrp_free(timer);
    }

    return FALSE;
}


static void del_timeout(DBusTimeout *dt, void *data)
{
    timeout_t *timer = (timeout_t *)dbus_timeout_get_data(dt);

    MRP_UNUSED(data);

    if (timer != NULL) {
        mrp_del_timer(timer->mt);
        timer->mt = NULL;
    }
}


static void toggle_timeout(DBusTimeout *dt, void *data)
{
    if (dbus_timeout_get_enabled(dt))
        add_timeout(dt, data);
    else
        del_timeout(dt, data);
}


static void wakeup_mainloop(void *data)
{
    dbus_glue_t *glue = (dbus_glue_t *)data;

    mrp_enable_deferred(glue->pump);
}


static void glue_free_cb(void *data)
{
    dbus_glue_t     *glue = (dbus_glue_t *)data;
    mrp_list_hook_t *p, *n;
    watch_t         *watch;
    timeout_t       *timer;

    mrp_list_foreach(&glue->watches, p, n) {
        watch = mrp_list_entry(p, typeof(*watch), hook);

        mrp_list_delete(&watch->hook);
        mrp_del_io_watch(watch->mw);

        mrp_free(watch);
    }

    mrp_list_foreach(&glue->timers, p, n) {
        timer = mrp_list_entry(p, typeof(*timer), hook);

        mrp_list_delete(&timer->hook);
        mrp_del_timer(timer->mt);

        mrp_free(timer);
    }

    mrp_free(glue);
}


static void pump_cb(mrp_deferred_t *d, void *user_data)
{
    dbus_glue_t *glue = (dbus_glue_t *)user_data;

    if (dbus_connection_dispatch(glue->conn) == DBUS_DISPATCH_COMPLETE)
        mrp_disable_deferred(d);
}


static void dispatch_status_cb(DBusConnection *conn, DBusDispatchStatus status,
                               void *user_data)
{
    dbus_glue_t *glue = (dbus_glue_t *)user_data;

    MRP_UNUSED(conn);

    switch (status) {
    case DBUS_DISPATCH_COMPLETE:
        mrp_disable_deferred(glue->pump);
        break;

    case DBUS_DISPATCH_DATA_REMAINS:
    case DBUS_DISPATCH_NEED_MEMORY:
    default:
        mrp_enable_deferred(glue->pump);
        break;
    }
}


int mrp_setup_dbus_connection(mrp_mainloop_t *ml, DBusConnection *conn)
{
    dbus_glue_t *glue;

    if (!dbus_connection_allocate_data_slot(&data_slot))
        return FALSE;

    if (dbus_connection_get_data(conn, data_slot) != NULL)
        return FALSE;

    if ((glue = mrp_allocz(sizeof(*glue))) != NULL) {
        mrp_list_init(&glue->watches);
        mrp_list_init(&glue->timers);
        glue->pump = mrp_add_deferred(ml, pump_cb, glue);

        if (glue->pump == NULL) {
            mrp_free(glue);
            return FALSE;
        }

        glue->ml   = ml;
        glue->conn = conn;
    }
    else
        return FALSE;

    if (!dbus_connection_set_data(conn, data_slot, glue, glue_free_cb))
        return FALSE;

    dbus_connection_set_dispatch_status_function(conn, dispatch_status_cb,
                                                 glue, NULL);

    dbus_connection_set_wakeup_main_function(conn, wakeup_mainloop,
                                             glue, NULL);

    return
        dbus_connection_set_watch_functions(conn, add_watch, del_watch,
                                            toggle_watch, glue, NULL) &&
            dbus_connection_set_timeout_functions(conn, add_timeout, del_timeout,
                                              toggle_timeout, glue, NULL);
}

