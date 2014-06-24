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

#ifndef __MURPHY_MAINLOOP_H__
#define __MURPHY_MAINLOOP_H__

#include <signal.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

typedef struct mrp_mainloop_s mrp_mainloop_t;

/*
 * I/O watches
 */

/** I/O events */
typedef enum {
    MRP_IO_EVENT_NONE  = 0x0,
    MRP_IO_EVENT_IN    = EPOLLIN,
    MRP_IO_EVENT_PRI   = EPOLLPRI,
    MRP_IO_EVENT_OUT   = EPOLLOUT,
    MRP_IO_EVENT_RDHUP = EPOLLRDHUP,
    MRP_IO_EVENT_WRHUP = EPOLLHUP,
    MRP_IO_EVENT_HUP   = EPOLLRDHUP|EPOLLHUP,
    MRP_IO_EVENT_ERR   = EPOLLERR,
    MRP_IO_EVENT_INOUT = EPOLLIN|EPOLLOUT,
    MRP_IO_EVENT_ALL   = EPOLLIN|EPOLLPRI|EPOLLOUT|EPOLLRDHUP|EPOLLERR
} mrp_io_event_t;

typedef struct mrp_io_watch_s mrp_io_watch_t;

/** I/O watch notification callback type. */
typedef void (*mrp_io_watch_cb_t)(mrp_io_watch_t *w, int fd,
                                  mrp_io_event_t events, void *user_data);
/** Register a new file descriptor to watch. */
mrp_io_watch_t *mrp_add_io_watch(mrp_mainloop_t *ml, int fd,
                                 mrp_io_event_t events,
                                 mrp_io_watch_cb_t cb, void *user_data);
/** Unregister an I/O watch. */
void mrp_del_io_watch(mrp_io_watch_t *watch);

/** Get the mainloop of an I/O watch. */
mrp_mainloop_t *mrp_get_io_watch_mainloop(mrp_io_watch_t *watch);


/*
 * timers
 */

typedef struct mrp_timer_s mrp_timer_t;

/** Timer notification callback type. */
typedef void (*mrp_timer_cb_t)(mrp_timer_t *t, void *user_data);

/** Add a new timer. */
mrp_timer_t *mrp_add_timer(mrp_mainloop_t *ml, unsigned int msecs,
                           mrp_timer_cb_t cb, void *user_data);
/** Modify the timeout or rearm the given timer. */
#define MRP_TIMER_RESTART (unsigned int)-1
void mrp_mod_timer(mrp_timer_t *t, unsigned int msecs);

/** Delete a timer. */
void mrp_del_timer(mrp_timer_t *t);

/** Get the mainloop of a timer. */
mrp_mainloop_t *mrp_get_timer_mainloop(mrp_timer_t *t);


/*
 * deferred callbacks
 */

typedef struct mrp_deferred_s mrp_deferred_t;

/** Deferred callback notification callback type. */
typedef void (*mrp_deferred_cb_t)(mrp_deferred_t *d, void *user_data);

/** Add a deferred callback. */
mrp_deferred_t *mrp_add_deferred(mrp_mainloop_t *ml, mrp_deferred_cb_t cb,
                                 void *user_data);
/** Remove a deferred callback. */
void mrp_del_deferred(mrp_deferred_t *d);

/** Disable a deferred callback. */
void mrp_disable_deferred(mrp_deferred_t *d);

/** Enable a deferred callback. */
void mrp_enable_deferred(mrp_deferred_t *d);

/** Get the mainloop of a deferred callback. */
mrp_mainloop_t *mrp_get_deferred_mainloop(mrp_deferred_t *d);


/*
 * signals
 */

typedef struct mrp_sighandler_s mrp_sighandler_t;

/* Signal handler callback type. */
typedef void (*mrp_sighandler_cb_t)(mrp_sighandler_t *h, int signum,
                                    void *user_data);
/** Register a signal handler. */
mrp_sighandler_t *mrp_add_sighandler(mrp_mainloop_t *ml, int signum,
                                     mrp_sighandler_cb_t cb, void *user_data);
/** Unregister a signal handler. */
void mrp_del_sighandler(mrp_sighandler_t *h);

/** Get the mainloop of a signal handler. */
mrp_mainloop_t *mrp_get_sighandler_mainloop(mrp_sighandler_t *h);


/*
 * wakeup callbacks
 */

/** I/O events */
typedef enum {
    MRP_WAKEUP_EVENT_NONE  = 0x0,        /* no event */
    MRP_WAKEUP_EVENT_TIMER = 0x1,        /* woken up by timeout */
    MRP_WAKEUP_EVENT_IO    = 0x2,        /* woken up by I/O (or signal) */
    MRP_WAKEUP_EVENT_ANY   = 0x3,        /* mask of all selectable events */
    MRP_WAKEUP_EVENT_LIMIT = 0x4,        /* woken up by forced trigger */
} mrp_wakeup_event_t;

typedef struct mrp_wakeup_s mrp_wakeup_t;

/** Wakeup callback notification type. */
typedef void (*mrp_wakeup_cb_t)(mrp_wakeup_t *w, mrp_wakeup_event_t event,
                                void *user_data);

/** Add a wakeup callback for the specified events. lpf_msecs and
 *  force_msecs specifiy two limiting intervals in milliseconds.
 *  lpf_msecs is a low-pass filtering interval. It is guaranteed that
 *  the wakeup callback will not be invoked more ofter than this.
 *  force_msecs is a forced trigger interval. It is guaranteed that
 *  the wakeup callback will be triggered at least this often. You can
 *  MRP_WAKEUP_NOLIMIT to omit either or both limiting intervals. */
#define MRP_WAKEUP_NOLIMIT ((unsigned int)0)
mrp_wakeup_t *mrp_add_wakeup(mrp_mainloop_t *ml, mrp_wakeup_event_t events,
                             unsigned int lpf_msecs, unsigned int force_msecs,
                             mrp_wakeup_cb_t cb, void *user_data);

/** Remove a wakeup callback. */
void mrp_del_wakeup(mrp_wakeup_t *w);

/** Get the mainloop of a wakeup callback. */
mrp_mainloop_t *mrp_get_wakeup_mainloop(mrp_wakeup_t *w);


/*
 * subloops - external mainloops pumped by this mainloop
 */

typedef struct mrp_subloop_s mrp_subloop_t;

typedef struct {
    int  (*prepare)(void *user_data);
    int  (*query)(void *user_data, struct pollfd *fds, int nfd, int *timeout);
    int  (*check)(void *user_data, struct pollfd *fds, int nfd);
    void (*dispatch)(void *user_data);
} mrp_subloop_ops_t;


/** Register an external mainloop to be pumped by the given mainloop. */
mrp_subloop_t *mrp_add_subloop(mrp_mainloop_t *ml, mrp_subloop_ops_t *ops,
                               void *user_data);

/** Stop pumping a registered external mainloop. */
void mrp_del_subloop(mrp_subloop_t *sl);


/*
 * superloops - external mainloop to pump murphy mainloops
 */

typedef struct {
    void *(*add_io)(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data);
    void (*del_io)(void *glue_data, void *id);

    void *(*add_timer)(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
    void (*del_timer)(void *glue_data, void *id);
    void (*mod_timer)(void *glue_data, void *id, unsigned int msecs);

    void *(*add_defer)(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
    void  (*del_defer)(void *glue_data, void *id);
    void  (*mod_defer)(void *glue_data, void *id, int enabled);
    void  (*unregister)(void *glue_data);

    /*
     * Notes:
     *
     *     This is a band-aid attempt to get our mainloop run under the
     *     threaded event loop of xwalk which has strict limitations about
     *     what (type of) thread can access which functionality of the
     *     event loop. In particular, the I/O watch equivalent is limited
     *     for the I/O thread. In the current media element resource infra
     *     integration code of xwalk, the related code is run in the UI
     *     thread. This makes interacting from there with the daemon using
     *     the stock resource libraries (in particular, pumping the mainloop)
     *     non-straightforward.
     *
     *     The superloop glue code is supposed to use poll_events to trigger
     *     a nonblocking epoll_wait on our epoll fd to retrieve (and cache)
     *     pending epoll events from the kernel. poll_io is then used by us
     *     to retrieve pending epoll events from the glue code. The glue code
     *     needs to take care of any necessary locking to protect itself/us
     *     from potentially concurrent invocations of poll_io and poll_events
     *     from different threads.
     *
     *     The superloop abstraction now became really really ugly. poll_io
     *     and poll_events implicitly assume/know that add_io/del_io is only
     *     used for adding a single superloop I/O watch for our epoll fd.
     *     The I/O watch id in the functions below is passed around only for
     *     doublechecing this. Probably we should change the I/O watch part
     *     of the superloop API to be actually less generic and only usable
     *     for the epoll fd to avoid further confusion.
     */
    size_t (*poll_events)(void *id, mrp_mainloop_t *ml, void **events);
    size_t (*poll_io)(void *glue_data, void *id, void *buf, size_t size);
} mrp_superloop_ops_t;


/** Set a superloop to pump the given mainloop. */
int mrp_set_superloop(mrp_mainloop_t *ml, mrp_superloop_ops_t *ops,
                      void *loop_data);

/** Clear the superloop that pumps the given mainloop. */
int mrp_clear_superloop(mrp_mainloop_t *ml);

/** Unregister a mainloop from its superloop if it has one. */
int mrp_mainloop_unregister(mrp_mainloop_t *ml);

/*
 * mainloop
 */

/** Create a new mainloop. */
mrp_mainloop_t *mrp_mainloop_create(void);

/** Destroy an existing mainloop, free all I/O watches, timers, etc. */
void mrp_mainloop_destroy(mrp_mainloop_t *ml);

/** Keep iterating the mainloop until mrp_mainloop_quit is called. */
int mrp_mainloop_run(mrp_mainloop_t *ml);

/** Prepare the mainloop for polling. */
int mrp_mainloop_prepare(mrp_mainloop_t *ml);

/** Poll the mainloop. */
int mrp_mainloop_poll(mrp_mainloop_t *ml, int may_block);

/** Dispatch pending events of the mainloop. */
int mrp_mainloop_dispatch(mrp_mainloop_t *ml);

/** Run a single prepare-poll-dispatch cycle of the mainloop. */
int mrp_mainloop_iterate(mrp_mainloop_t *ml);

/** Quit the mainloop. */
void mrp_mainloop_quit(mrp_mainloop_t *ml, int exit_code);

MRP_CDECL_END

#endif /* __MURPHY_MAINLOOP_H__ */
