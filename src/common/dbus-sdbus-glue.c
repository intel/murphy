/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>

#include <murphy/common/dbus-sdbus.h>

#define USEC_TO_MSEC(usec) ((unsigned int)((usec) / 1000))
#define MSEC_TO_USEC(msec) ((uint64_t)(msec) * 1000)

typedef struct {
    sd_bus         *bus;
    mrp_mainloop_t *ml;
    mrp_subloop_t  *sl;
    int             events;
} bus_glue_t;


static int bus_prepare(void *user_data)
{
    MRP_UNUSED(user_data);

    return FALSE;
}


static int bus_query(void *user_data, struct pollfd *fds, int nfd, int *timeout)
{
    bus_glue_t *b = (bus_glue_t *)user_data;
    uint64_t    usec;

    if (nfd > 0) {
        fds[0].fd      = sd_bus_get_fd(b->bus);
        fds[0].events  = sd_bus_get_events(b->bus) | POLLIN | POLLHUP;
        fds[0].revents = 0;

        if (sd_bus_get_timeout(b->bus, &usec) < 0)
            *timeout = -1;
        else
            *timeout = USEC_TO_MSEC(usec);

        mrp_debug("fd: %d, events: 0x%x, timeout: %u", fds[0].fd,
                  fds[0].events, *timeout);
    }

    return 1;
}


static int bus_check(void *user_data, struct pollfd *fds, int nfd)
{
    bus_glue_t *b = (bus_glue_t *)user_data;

    if (nfd > 0) {
        b->events = fds[0].revents;

        if (b->events != 0)
            return TRUE;
    }
    else
        b->events = 0;

    return FALSE;
}


static void bus_dispatch(void *user_data)
{
    bus_glue_t *b = (bus_glue_t *)user_data;

    mrp_debug("dispatching events 0x%x to sd_bus %p", b->events, b->bus);

    if (b->events & MRP_IO_EVENT_HUP)
        mrp_debug("sd_bus peer has closed the connection");

    while (sd_bus_process(b->bus, NULL) > 0)
        sd_bus_flush(b->bus);

    mrp_debug("done dispatching");
}


int mrp_dbus_setup_with_mainloop(mrp_mainloop_t *ml, sd_bus *bus)
{
    static mrp_subloop_ops_t bus_ops = {
        .prepare  = bus_prepare,
        .query    = bus_query,
        .check    = bus_check,
        .dispatch = bus_dispatch
    };

    bus_glue_t *b;


    if ((b = mrp_allocz(sizeof(*b))) != NULL) {
        /* XXX TODO: Hmm... is this really needed ? */
        while (sd_bus_process(bus, NULL) > 0)
            sd_bus_flush(bus);

        b->bus = bus;
        b->ml  = ml;
        b->sl  = mrp_add_subloop(ml, &bus_ops, b);

        if (b->sl != NULL)
            return TRUE;
        else
            mrp_free(b);
    }

    return FALSE;
}
