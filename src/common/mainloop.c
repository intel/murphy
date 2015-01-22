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

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/json.h>
#include <murphy/common/msg.h>
#include <murphy/common/mainloop.h>

#define USECS_PER_SEC  (1000 * 1000)
#define USECS_PER_MSEC (1000)
#define NSECS_PER_USEC (1000)

/*
 * I/O watches
 */

struct mrp_io_watch_s {
    mrp_list_hook_t    hook;                     /* to list of watches */
    mrp_list_hook_t    deleted;                  /* to list of pending delete */
    int              (*free)(void *ptr);         /* cb to free memory */
    mrp_mainloop_t    *ml;                       /* mainloop */
    int                fd;                       /* file descriptor to watch */
    mrp_io_event_t     events;                   /* events of interest */
    mrp_io_watch_cb_t  cb;                       /* user callback */
    void              *user_data;                /* opaque user data */
    struct pollfd     *pollfd;                   /* associated pollfd */
    mrp_list_hook_t    slave;                    /* watches with the same fd */
    int                wrhup;                    /* EPOLLHUPs delivered */
};

#define is_master(w) !mrp_list_empty(&(w)->hook)
#define is_slave(w)  !mrp_list_empty(&(w)->slave)


/*
 * timers
 */

struct mrp_timer_s {
    mrp_list_hook_t  hook;                       /* to list of timers */
    mrp_list_hook_t  deleted;                    /* to list of pending delete */
    int            (*free)(void *ptr);           /* cb to free memory */
    mrp_mainloop_t  *ml;                         /* mainloop */
    unsigned int     msecs;                      /* timer interval */
    uint64_t         expire;                     /* next expiration time */
    mrp_timer_cb_t   cb;                         /* user callback */
    void            *user_data;                  /* opaque user data */
};


/*
 * deferred callbacks
 */

struct mrp_deferred_s {
    mrp_list_hook_t    hook;                     /* to list of cbs */
    mrp_list_hook_t    deleted;                  /* to list of pending delete */
    int              (*free)(void *ptr);         /* cb to free memory */
    mrp_mainloop_t    *ml;                       /* mainloop */
    mrp_deferred_cb_t  cb;                       /* user callback */
    void              *user_data;                /* opaque user data */
    int                inactive : 1;
};


/*
 * signal handlers
 */

struct mrp_sighandler_s {
    mrp_list_hook_t      hook;                   /* to list of handlers */
    mrp_list_hook_t      deleted;                /* to list of pending delete */
    int                (*free)(void *ptr);       /* cb to free memory */
    mrp_mainloop_t      *ml;                     /* mainloop */
    int                  signum;                 /* signal number */
    mrp_sighandler_cb_t  cb;                     /* user callback */
    void                *user_data;              /* opaque user data */
};


/*
 * wakeup notifications
 */

struct mrp_wakeup_s {
    mrp_list_hook_t      hook;                   /* to list of wakeup cbs */
    mrp_list_hook_t      deleted;                /* to list of pending delete */
    int                (*free)(void *ptr);       /* cb to free memory */
    mrp_mainloop_t      *ml;                     /* mainloop */
    mrp_wakeup_event_t   events;                 /* wakeup event mask */
    uint64_t             lpf;                    /* wakeup at most this often */
    uint64_t             next;                   /* next wakeup time */
    mrp_timer_t         *timer;                  /* forced interval timer */
    mrp_wakeup_cb_t      cb;                     /* user callback */
    void                *user_data;              /* opaque user data */
};

#define mark_deleted(o) do {                                    \
        (o)->cb = NULL;                                         \
        mrp_list_append(&(o)->ml->deleted, &(o)->deleted);      \
    } while (0)

#define is_deleted(o) ((o)->cb == NULL)


/*
 * any of the above data structures linked to the list of deleted items
 *
 * When deleted, the above data structures are first unlinked from their
 * native list and linked to the special list of deleted entries. At an
 * appropriate point upon every iteration of the main loop this list is
 * checked and all entries are freed. This structure is used to get a
 * pointer to the real structure that we need free. For this to work link
 * hooks in all of the above structures need to be kept at the same offset
 * as it is in deleted_t.
 */

typedef struct {
    mrp_list_hook_t  hook;                       /* unfreed deleted items */
    mrp_list_hook_t  deleted;                    /* to list of pending delete */
    int            (*free)(void *ptr);           /* cb to free memory */
} deleted_t;


/*
 * file descriptor table
 *
 * We do not want to associate direct pointers to related data structures
 * with epoll. We might get delivered pending events for deleted fds (at
 * least for unix domain sockets this seems to be the case) and with direct
 * pointers we'd get delivered a dangling pointer together with the event.
 * Instead we keep these structures in an fd table and use the fd to look
 * up the associated data structure for events. We ignore events for which
 * no data structure is found. In the fd table we keep a fixed size direct
 * table for a small amount of fds (we expect to be using at most in the
 * vast majority of cases) and we hash in the rest.
 */

#define FDTBL_SIZE 64

typedef struct {
    void       *t[FDTBL_SIZE];
    mrp_htbl_t *h;
} fdtbl_t;


/*
 * external mainloops
 */

struct mrp_subloop_s {
    mrp_list_hook_t      hook;                   /* to list of subloops */
    mrp_list_hook_t      deleted;                /* to list of pending delete */
    int                (*free)(void *ptr);       /* cb to free memory */
    mrp_mainloop_t      *ml;                     /* main loop */
    mrp_subloop_ops_t   *cb;                     /* subloop glue callbacks */
    void                *user_data;              /* opaque subloop data */
    int                  epollfd;                /* epollfd for this subloop */
    struct epoll_event  *events;                 /* epoll event buffer */
    int                  nevent;                 /* epoll event buffer size */
    fdtbl_t             *fdtbl;                  /* file descriptor table */
    mrp_io_watch_t      *w;                      /* watch for epollfd */
    struct pollfd       *pollfds;                /* pollfds for this subloop */
    int                  npollfd;                /* number of pollfds */
    int                  pending;                /* pending events */
    int                  poll;                   /* need to poll for events */
};


/*
 * event busses
 */

struct mrp_event_bus_s {
    char            *name;                       /* bus name */
    mrp_list_hook_t  hook;                       /* to list of busses */
    mrp_mainloop_t  *ml;                         /* associated mainloop */
    mrp_list_hook_t  watches;                    /* event watches on this bus */
    int              busy;                       /* whether pumping events */
    int              dead;
};


/*
 * event watches
 */

struct mrp_event_watch_s {
    mrp_list_hook_t       hook;                  /* to list of event watches */
    mrp_event_bus_t      *bus;                   /* associated event bus */
    mrp_event_mask_t      mask;                  /* mask of watched events */
    mrp_event_watch_cb_t  cb;                    /* notification callback */
    void                 *user_data;             /* opaque user data */
    int                   dead : 1;              /* marked for deletion */
};


/*
 * pending events
 */

typedef struct {
    mrp_list_hook_t  hook;                       /* to event queue */
    mrp_event_bus_t *bus;                        /* bus for this event */
    uint32_t         id;                         /* event id */
    int              format;                     /* attached data format */
    void            *data;                       /* attached data */
} pending_event_t;


/*
 * main loop
 */

struct mrp_mainloop_s {
    int                  epollfd;                /* our epoll descriptor */
    struct epoll_event  *events;                 /* epoll event buffer */
    int                  nevent;                 /* epoll event buffer size */
    fdtbl_t             *fdtbl;                  /* file descriptor table */

    mrp_list_hook_t      iowatches;              /* list of I/O watches */
    int                  niowatch;               /* number of I/O watches */
    mrp_io_event_t       iomode;                 /* default event trigger mode */

    mrp_list_hook_t      timers;                 /* list of timers */
    mrp_timer_t         *next_timer;             /* next expiring timer */

    mrp_list_hook_t      deferred;               /* list of deferred cbs */
    mrp_list_hook_t      inactive_deferred;      /* inactive defferred cbs */

    mrp_list_hook_t      wakeups;                /* list of wakeup cbs */

    int                  poll_timeout;           /* next poll timeout */
    int                  poll_result;            /* return value from poll */

    int                  sigfd;                  /* signal polling fd */
    sigset_t             sigmask;                /* signal mask */
    mrp_io_watch_t      *sigwatch;               /* sigfd I/O watch */
    mrp_list_hook_t      sighandlers;            /* signal handlers */

    mrp_list_hook_t      subloops;               /* external main loops */

    mrp_list_hook_t      deleted;                /* unfreed deleted items */
    int                  quit;                   /* TRUE if _quit called */
    int                  exit_code;              /* returned from _run */

    mrp_superloop_ops_t *super_ops;              /* superloop options */
    void                *super_data;             /* superloop glue data */
    void                *iow;                    /* superloop epollfd watch */
    void                *timer;                  /* superloop timer */
    void                *work;                   /* superloop deferred work */

    mrp_list_hook_t      busses;                 /* known event busses */
    mrp_list_hook_t      eventq;                 /* pending events */
    mrp_deferred_t      *eventd;                 /* deferred event pump cb */
};


static mrp_event_def_t *events;                  /* registered events */
static int              nevent;                  /* number of events */
static MRP_LIST_HOOK   (ewatches);               /* global, synchronous 'bus' */


static void dump_pollfds(const char *prefix, struct pollfd *fds, int nfd);
static void adjust_superloop_timer(mrp_mainloop_t *ml);
static size_t poll_events(void *id, mrp_mainloop_t *ml, void **bufp);
static void pump_events(mrp_deferred_t *d, void *user_data);

/*
 * fd table manipulation
 */

static int fd_cmp(const void *key1, const void *key2)
{
    return key2 - key1;
}


static uint32_t fd_hash(const void *key)
{
    uint32_t h;

    h = (uint32_t)(ptrdiff_t)key;

    return h;
}



static fdtbl_t *fdtbl_create(void)
{
    fdtbl_t           *ft;
    mrp_htbl_config_t  hcfg;

    if ((ft = mrp_allocz(sizeof(*ft))) != NULL) {
        mrp_clear(&hcfg);

        hcfg.comp    = fd_cmp;
        hcfg.hash    = fd_hash;
        hcfg.free    = NULL;
        hcfg.nbucket = 16;

        ft->h = mrp_htbl_create(&hcfg);

        if (ft->h != NULL)
            return ft;
        else
            mrp_free(ft);
    }

    return NULL;
}


static void fdtbl_destroy(fdtbl_t *ft)
{
    if (ft != NULL) {
        mrp_htbl_destroy(ft->h, FALSE);
        mrp_free(ft);
    }
}


static void *fdtbl_lookup(fdtbl_t *ft, int fd)
{
    if (fd >= 0 && ft != NULL) {
        if (fd < FDTBL_SIZE)
            return ft->t[fd];
        else
            return mrp_htbl_lookup(ft->h, (void *)(ptrdiff_t)fd);
    }

    return NULL;
}


static int fdtbl_insert(fdtbl_t *ft, int fd, void *ptr)
{
    if (fd >= 0 && ft != NULL) {
        if (fd < FDTBL_SIZE) {
            if (ft->t[fd] == NULL) {
                ft->t[fd] = ptr;
                return 0;
            }
            else
                errno = EEXIST;
        }
        else {
            if (mrp_htbl_insert(ft->h, (void *)(ptrdiff_t)fd, ptr))
                return 0;
            else
                errno = EEXIST;
        }
    }
    else
        errno = EINVAL;

    return -1;
}


static void fdtbl_remove(fdtbl_t *ft, int fd)
{
    if (fd >= 0 && ft != NULL) {
        if (fd < FDTBL_SIZE)
            ft->t[fd] = NULL;
        else
            mrp_htbl_remove(ft->h, (void *)(ptrdiff_t)fd, FALSE);
    }
}


/*
 * I/O watches
 */

static uint32_t epoll_event_mask(mrp_io_watch_t *master, mrp_io_watch_t *ignore)
{
    mrp_io_watch_t  *w;
    mrp_list_hook_t *p, *n;
    uint32_t         mask;

    mask = (master != ignore ?
            master->events : master->events & MRP_IO_TRIGGER_EDGE);

    mrp_list_foreach(&master->slave, p, n) {
        w = mrp_list_entry(p, typeof(*w), slave);

        if (w != ignore)
            mask |= w->events;
    }

    mrp_debug("epoll event mask for I/O watch %p: %d", master, mask);

    return mask;
}


static int epoll_add_slave(mrp_io_watch_t *master, mrp_io_watch_t *slave)
{
    mrp_mainloop_t     *ml = master->ml;
    struct epoll_event  evt;

    evt.events   = epoll_event_mask(master, NULL) | slave->events;
    evt.data.u64 = 0;
    evt.data.fd  = master->fd;

    if (epoll_ctl(ml->epollfd, EPOLL_CTL_MOD, master->fd, &evt) == 0) {
        mrp_list_append(&master->slave, &slave->slave);

        return 0;
    }

    return -1;
}


static int epoll_add(mrp_io_watch_t *w)
{
    mrp_mainloop_t     *ml = w->ml;
    mrp_io_watch_t     *master;
    struct epoll_event  evt;

    if (fdtbl_insert(ml->fdtbl, w->fd, w) == 0) {
        evt.events   = w->events;
        evt.data.u64 = 0;                /* init full union for valgrind... */
        evt.data.fd  = w->fd;

        if (epoll_ctl(ml->epollfd, EPOLL_CTL_ADD, w->fd, &evt) == 0) {
            mrp_list_append(&ml->iowatches, &w->hook);
            ml->niowatch++;

            return 0;
        }
        else
            fdtbl_remove(ml->fdtbl, w->fd);
    }
    else {
        if (errno == EEXIST) {
            master = fdtbl_lookup(ml->fdtbl, w->fd);

            if (master != NULL)
                return epoll_add_slave(master, w);
        }
    }

    return -1;
}


static int epoll_del(mrp_io_watch_t *w)
{
    mrp_mainloop_t     *ml = w->ml;
    mrp_io_watch_t     *master;
    struct epoll_event  evt;
    int                 status;

    if (is_master(w))
        master = w;
    else
        master = fdtbl_lookup(ml->fdtbl, w->fd);

    if (master != NULL) {
        evt.events   = epoll_event_mask(master, w);
        evt.data.u64 = 0;                /* init full union for valgrind... */
        evt.data.fd  = w->fd;

        if ((evt.events & MRP_IO_EVENT_ALL) == 0) {
            fdtbl_remove(ml->fdtbl, w->fd);
            status = epoll_ctl(ml->epollfd, EPOLL_CTL_DEL, w->fd, &evt);

            if (status == 0 || (errno == EBADF || errno == ENOENT))
                ml->niowatch--;
        }
        else
            status = epoll_ctl(ml->epollfd, EPOLL_CTL_MOD, w->fd, &evt);

        if (status == 0 || (errno == EBADF || errno == ENOENT))
            return 0;
        else
            mrp_log_error("Failed to update epoll for deleted I/O watch %p "
                          "(fd %d, %d: %s).", w, w->fd, errno, strerror(errno));
    }
    else {
        mrp_log_error("Failed to find master for deleted I/O watch %p "
                      "(fd %d).", w, w->fd);
        errno = EINVAL;
    }

    return -1;
}


static int free_io_watch(void *ptr)
{
    mrp_io_watch_t *w  = (mrp_io_watch_t *)ptr;
    mrp_mainloop_t *ml = w->ml;
    mrp_io_watch_t *master;

    master = fdtbl_lookup(ml->fdtbl, w->fd);

    if (master == w) {
        fdtbl_remove(ml->fdtbl, w->fd);

        if (!mrp_list_empty(&w->slave)) {
            /* relink first slave as new master to mainloop */
            master = mrp_list_entry(w->slave.next, typeof(*master), slave);
            mrp_list_append(&ml->iowatches, &master->hook);

            fdtbl_insert(ml->fdtbl, master->fd, master);
        }
    }

    mrp_list_delete(&w->slave);
    mrp_free(w);

    return TRUE;
}


mrp_io_watch_t *mrp_add_io_watch(mrp_mainloop_t *ml, int fd,
                                 mrp_io_event_t events,
                                 mrp_io_watch_cb_t cb, void *user_data)
{
    mrp_io_watch_t *w;

    if (fd < 0 || cb == NULL)
        return NULL;

    if ((w = mrp_allocz(sizeof(*w))) != NULL) {
        mrp_list_init(&w->hook);
        mrp_list_init(&w->deleted);
        mrp_list_init(&w->slave);
        w->ml        = ml;
        w->fd        = fd;
        w->events    = events & MRP_IO_EVENT_ALL;

        switch (events & MRP_IO_TRIGGER_MASK) {
        case 0:
            if (ml->iomode == MRP_IO_TRIGGER_EDGE)
                w->events |= MRP_IO_TRIGGER_EDGE;
            break;
        case MRP_IO_TRIGGER_EDGE:
            w->events |= MRP_IO_TRIGGER_EDGE;
            break;
        case MRP_IO_TRIGGER_LEVEL:
            break;
        default:
            mrp_log_warning("Invalid I/O event trigger mode 0x%x.",
                            events & MRP_IO_TRIGGER_MASK);
            break;
        }

        w->cb        = cb;
        w->user_data = user_data;
        w->free      = free_io_watch;

        if (epoll_add(w) != 0) {
            mrp_free(w);
            w = NULL;
        }
        else
            mrp_debug("added I/O watch %p (fd %d, events 0x%x)", w, w->fd, w->events);
    }

    return w;
}


void mrp_del_io_watch(mrp_io_watch_t *w)
{
    /*
     * Notes: It is not safe to free the watch here as there might be
     *        a delivered but unprocessed epoll event with a pointer
     *        to the watch. We just mark it deleted and take care of
     *        the actual deletion in the dispatching loop.
     */

    if (w != NULL && !is_deleted(w)) {
        mrp_debug("marking I/O watch %p (fd %d) deleted", w, w->fd);

        mark_deleted(w);
        w->events = 0;

        epoll_del(w);
    }
}


mrp_mainloop_t *mrp_get_io_watch_mainloop(mrp_io_watch_t *w)
{
    return w ? w->ml : NULL;
}


int mrp_set_io_event_mode(mrp_mainloop_t *ml, mrp_io_event_t mode)
{
    if (mode == MRP_IO_TRIGGER_LEVEL || mode == MRP_IO_TRIGGER_EDGE) {
        ml->iomode = mode;
        return TRUE;
    }
    else {
        mrp_log_error("Invalid I/O event mode 0x%x.", mode);
        return FALSE;
    }
}


mrp_io_event_t mrp_get_io_event_mode(mrp_mainloop_t *ml)
{
    return ml->iomode ? ml->iomode : MRP_IO_TRIGGER_LEVEL;
}


/*
 * timers
 */

static uint64_t time_now(void)
{
    struct timespec ts;
    uint64_t        now;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    now  = ts.tv_sec  * USECS_PER_SEC;
    now += ts.tv_nsec / NSECS_PER_USEC;

    return now;
}


static inline int usecs_to_msecs(uint64_t usecs)
{
    int msecs;

    msecs = (usecs + USECS_PER_MSEC - 1) / USECS_PER_MSEC;

    return msecs;
}


static void insert_timer(mrp_timer_t *t)
{
    mrp_mainloop_t  *ml = t->ml;
    mrp_list_hook_t *p, *n;
    mrp_timer_t     *t1, *next;
    int              inserted;

    /*
     * Notes:
     *     If there is ever a need to run a large number of
     *     simultaneous timers, we need to change this to a
     *     self-balancing data structure, eg. an red-black tree.
     */

    inserted = FALSE;
    next     = NULL;
    mrp_list_foreach(&ml->timers, p, n) {
        t1 = mrp_list_entry(p, mrp_timer_t, hook);

        if (!is_deleted(t1)) {
            if (t->expire <= t1->expire) {
                mrp_list_prepend(p->prev, &t->hook);
                inserted = TRUE;
                break;
            }
            if (next == NULL)
                next = t1;
        }
    }

    if (!inserted)
        mrp_list_append(&ml->timers, &t->hook);

    if (next)
        ml->next_timer = next;
    else {
        ml->next_timer = t;
        adjust_superloop_timer(ml);
    }
}


static inline void rearm_timer(mrp_timer_t *t)
{
    mrp_list_delete(&t->hook);
    t->expire = time_now() + t->msecs * USECS_PER_MSEC;
    insert_timer(t);
}


static mrp_timer_t *find_next_timer(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_timer_t     *t = NULL;

    mrp_list_foreach(&ml->timers, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        if (!is_deleted(t))
            break;
        else
            t = NULL;
    }

    ml->next_timer = t;
    return t;
}


static int free_timer(void *ptr)
{
    mrp_timer_t *t = (mrp_timer_t *)ptr;

    mrp_free(t);

    return TRUE;
}



mrp_timer_t *mrp_add_timer(mrp_mainloop_t *ml, unsigned int msecs,
                           mrp_timer_cb_t cb, void *user_data)
{
    mrp_timer_t *t;

    if (cb == NULL)
        return NULL;

    if ((t = mrp_allocz(sizeof(*t))) != NULL) {
        mrp_list_init(&t->hook);
        mrp_list_init(&t->deleted);
        t->ml        = ml;
        t->expire    = time_now() + msecs * USECS_PER_MSEC;
        t->msecs     = msecs;
        t->cb        = cb;
        t->user_data = user_data;
        t->free      = free_timer;

        insert_timer(t);
    }

    return t;
}


void mrp_mod_timer(mrp_timer_t *t, unsigned int msecs)
{
    if (t != NULL && !is_deleted(t)) {
        if (msecs != MRP_TIMER_RESTART)
            t->msecs = msecs;

        rearm_timer(t);
    }
}


void mrp_del_timer(mrp_timer_t *t)
{
    /*
     * Notes: It is not safe to simply free this entry here as we might
     *        be dispatching with this entry being the next to process.
     *        We check for this and if it is not the case we relink this
     *        to the list of deleted items which will be then processed
     *        at end of the mainloop iteration. Otherwise we only mark the
     *        this entry for deletion and the rest will be taken care of in
     *        dispatch_timers().
     */

    if (t != NULL && !is_deleted(t)) {
        mrp_debug("marking timer %p deleted", t);

        mark_deleted(t);

        if (t->ml->next_timer == t) {
            find_next_timer(t->ml);
            adjust_superloop_timer(t->ml);
        }
    }
}


mrp_mainloop_t *mrp_get_timer_mainloop(mrp_timer_t *t)
{
    return t ? t->ml : NULL;
}


/*
 * deferred/idle callbacks
 */

mrp_deferred_t *mrp_add_deferred(mrp_mainloop_t *ml, mrp_deferred_cb_t cb,
                                 void *user_data)
{
    mrp_deferred_t *d;

    if (cb == NULL)
        return NULL;

    if ((d = mrp_allocz(sizeof(*d))) != NULL) {
        mrp_list_init(&d->hook);
        mrp_list_init(&d->deleted);
        d->ml        = ml;
        d->cb        = cb;
        d->user_data = user_data;

        mrp_list_append(&ml->deferred, &d->hook);
        adjust_superloop_timer(ml);
    }

    return d;
}


void mrp_del_deferred(mrp_deferred_t *d)
{
    /*
     * Notes: It is not safe to simply free this entry here as we might
     *        be dispatching with this entry being the next to process.
     *        We just mark this here deleted and take care of the rest
     *        in the dispatching loop.
     */

    if (d != NULL && !is_deleted(d)) {
        mrp_debug("marking deferred %p deleted", d);
        mark_deleted(d);
    }
}


void mrp_disable_deferred(mrp_deferred_t *d)
{
    if (d != NULL)
        d->inactive = TRUE;
}


static inline void disable_deferred(mrp_deferred_t *d)
{
    if (MRP_LIKELY(d->inactive)) {
        mrp_list_delete(&d->hook);
        mrp_list_append(&d->ml->inactive_deferred, &d->hook);
    }

}


void mrp_enable_deferred(mrp_deferred_t *d)
{
    if (d != NULL) {
        if (!is_deleted(d)) {
            d->inactive = FALSE;
            mrp_list_delete(&d->hook);
            mrp_list_append(&d->ml->deferred, &d->hook);
        }
    }
}


mrp_mainloop_t *mrp_get_deferred_mainloop(mrp_deferred_t *d)
{
    return d ? d->ml : NULL;
}


/*
 * signal notifications
 */

static void dispatch_signals(mrp_io_watch_t *w, int fd,
                             mrp_io_event_t events, void *user_data)
{
    mrp_mainloop_t          *ml = mrp_get_io_watch_mainloop(w);
    struct signalfd_siginfo  sig;
    mrp_list_hook_t         *p, *n;
    mrp_sighandler_t        *h;
    int                      signum;

    MRP_UNUSED(events);
    MRP_UNUSED(user_data);

    while (read(fd, &sig, sizeof(sig)) > 0) {
        signum = sig.ssi_signo;

        mrp_list_foreach(&ml->sighandlers, p, n) {
            h = mrp_list_entry(p, typeof(*h), hook);

            if (!is_deleted(h)) {
                if (h->signum == signum)
                    h->cb(h, signum, h->user_data);
            }
        }
    }
}


static int setup_sighandlers(mrp_mainloop_t *ml)
{
    if (ml->sigfd == -1) {
        sigemptyset(&ml->sigmask);

        ml->sigfd = signalfd(-1, &ml->sigmask, SFD_NONBLOCK | SFD_CLOEXEC);

        if (ml->sigfd == -1)
            return FALSE;

        ml->sigwatch = mrp_add_io_watch(ml, ml->sigfd, MRP_IO_EVENT_IN,
                                        dispatch_signals, NULL);

        if (ml->sigwatch == NULL) {
            close(ml->sigfd);
            return FALSE;
        }
    }

    return TRUE;
}


mrp_sighandler_t *mrp_add_sighandler(mrp_mainloop_t *ml, int signum,
                                     mrp_sighandler_cb_t cb, void *user_data)
{
    mrp_sighandler_t *s;

    if (cb == NULL || ml->sigfd == -1)
        return NULL;

    if ((s = mrp_allocz(sizeof(*s))) != NULL) {
        mrp_list_init(&s->hook);
        mrp_list_init(&s->deleted);
        s->ml        = ml;
        s->signum    = signum;
        s->cb        = cb;
        s->user_data = user_data;

        mrp_list_append(&ml->sighandlers, &s->hook);
        sigaddset(&ml->sigmask, s->signum);
        signalfd(ml->sigfd, &ml->sigmask, SFD_NONBLOCK|SFD_CLOEXEC);
        sigprocmask(SIG_BLOCK, &ml->sigmask, NULL);
    }

    return s;
}


static void recalc_sigmask(mrp_mainloop_t *ml)
{
    mrp_list_hook_t  *p, *n;
    mrp_sighandler_t *h;

    sigprocmask(SIG_UNBLOCK, &ml->sigmask, NULL);
    sigemptyset(&ml->sigmask);

    mrp_list_foreach(&ml->sighandlers, p, n) {
        h = mrp_list_entry(p, typeof(*h), hook);
        if (!is_deleted(h))
            sigaddset(&ml->sigmask, h->signum);
    }

    sigprocmask(SIG_BLOCK, &ml->sigmask, NULL);
}


void mrp_del_sighandler(mrp_sighandler_t *h)
{
    if (h != NULL && !is_deleted(h)) {
        mrp_debug("marking sighandler %p deleted", h);

        mark_deleted(h);
        recalc_sigmask(h->ml);
    }
}


mrp_mainloop_t *mrp_get_sighandler_mainloop(mrp_sighandler_t *h)
{
    return h ? h->ml : NULL;
}


/*
 * wakeup notifications
 */

static void wakeup_cb(mrp_wakeup_t *w, mrp_wakeup_event_t event, uint64_t now)
{
    if (w->next > now) {
        mrp_debug("skipping wakeup %p because of low-pass filter", w);
        return;
    }

    w->cb(w, event, w->user_data);

    if (w->lpf != MRP_WAKEUP_NOLIMIT)
        w->next = now + w->lpf;

    if (w->timer != NULL)
        mrp_mod_timer(w->timer, MRP_TIMER_RESTART);
}


static void forced_wakeup_cb(mrp_timer_t *t, void *user_data)
{
    mrp_wakeup_t *w = (mrp_wakeup_t *)user_data;

    MRP_UNUSED(t);

    if (is_deleted(w))
        return;

    mrp_debug("dispatching forced wakeup cb %p", w);

    wakeup_cb(w, MRP_WAKEUP_EVENT_LIMIT, time_now());
}


mrp_wakeup_t *mrp_add_wakeup(mrp_mainloop_t *ml, mrp_wakeup_event_t events,
                             unsigned int lpf_msecs, unsigned int force_msecs,
                             mrp_wakeup_cb_t cb, void *user_data)
{
    mrp_wakeup_t *w;

    if (cb == NULL)
        return NULL;

    if (lpf_msecs > force_msecs && force_msecs != MRP_WAKEUP_NOLIMIT)
        return NULL;

    if ((w = mrp_allocz(sizeof(*w))) != NULL) {
        mrp_list_init(&w->hook);
        mrp_list_init(&w->deleted);
        w->ml        = ml;
        w->events    = events;
        w->cb        = cb;
        w->user_data = user_data;

        w->lpf = lpf_msecs * USECS_PER_MSEC;

        if (lpf_msecs != MRP_WAKEUP_NOLIMIT)
            w->next = time_now() + w->lpf;

        if (force_msecs != MRP_WAKEUP_NOLIMIT) {
            w->timer = mrp_add_timer(ml, force_msecs, forced_wakeup_cb, w);

            if (w->timer == NULL) {
                mrp_free(w);
                return NULL;
            }
        }

        mrp_list_append(&ml->wakeups, &w->hook);
    }

    return w;
}


void mrp_del_wakeup(mrp_wakeup_t *w)
{
    /*
     * Notes: It is not safe to simply free this entry here as we might
     *        be dispatching with this entry being the next to process.
     *        We just mark this here deleted and take care of the rest
     *        in the dispatching loop.
     */

    if (w != NULL && !is_deleted(w)) {
        mrp_debug("marking wakeup %p deleted", w);
        mark_deleted(w);
    }
}


mrp_mainloop_t *mrp_get_wakeup_mainloop(mrp_wakeup_t *w)
{
    return w ? w->ml : NULL;
}


/*
 * external mainloops we pump
 */

static int free_subloop(void *ptr)
{
    mrp_subloop_t *sl = (mrp_subloop_t *)ptr;

    mrp_debug("freeing subloop %p", sl);

    mrp_free(sl->pollfds);
    mrp_free(sl->events);
    mrp_free(sl);

    return TRUE;
}


static void subloop_event_cb(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
                             void *user_data)
{
    mrp_subloop_t *sl = (mrp_subloop_t *)user_data;

    MRP_UNUSED(w);
    MRP_UNUSED(fd);
    MRP_UNUSED(events);

    mrp_debug("subloop %p has events, setting poll to TRUE", sl);

    sl->poll = TRUE;
}


mrp_subloop_t *mrp_add_subloop(mrp_mainloop_t *ml, mrp_subloop_ops_t *ops,
                               void *user_data)
{
    mrp_subloop_t *sl;

    if (ops == NULL || user_data == NULL)
        return NULL;

    if ((sl = mrp_allocz(sizeof(*sl))) != NULL) {
        mrp_list_init(&sl->hook);
        mrp_list_init(&sl->deleted);
        sl->free      = free_subloop;
        sl->ml        = ml;
        sl->cb        = ops;
        sl->user_data = user_data;
        sl->epollfd   = epoll_create1(EPOLL_CLOEXEC);
        sl->fdtbl     = fdtbl_create();

        if (sl->epollfd >= 0 && sl->fdtbl != NULL) {
            sl->w = mrp_add_io_watch(ml, sl->epollfd, MRP_IO_EVENT_IN,
                                     subloop_event_cb, sl);

            if (sl->w != NULL)
                mrp_list_append(&ml->subloops, &sl->hook);
            else
                goto fail;
        }
        else {
        fail:
            close(sl->epollfd);
            fdtbl_destroy(sl->fdtbl);
            mrp_free(sl);
            sl = NULL;
        }
    }

    return sl;
}


void mrp_del_subloop(mrp_subloop_t *sl)
{
    struct epoll_event dummy;
    int                i;

    /*
     * Notes: It is not safe to free the loop here as there might be
     *        a delivered but unprocessed epoll event with a pointers
     *        to the loops pollfds. However, since we do not dispatch
     *        loops by traversing the list of loops, it is safe to relink
     *        it to the list of data structures to be deleted at the
     *        end of the next main loop iteration. So we just remove the
     *        pollfds from epoll, mark this as deleted and relink it.
     */

    if (sl != NULL && !is_deleted(sl)) {
        mrp_debug("deactivating and marking subloop %p deleted", sl);

        mrp_del_io_watch(sl->w);

        /* XXX TODO: Why ? close(sl->epollfd) should be enough... */
        for (i = 0; i < sl->npollfd; i++)
            epoll_ctl(sl->epollfd, EPOLL_CTL_DEL, sl->pollfds[i].fd, &dummy);

        close(sl->epollfd);
        sl->epollfd = -1;
        fdtbl_destroy(sl->fdtbl);
        sl->fdtbl = NULL;

        mark_deleted(sl);
    }
}


/*
 * external mainloop that pumps us
 */


static void super_io_cb(void *super_data, void *id, int fd,
                        mrp_io_event_t events, void *user_data)
{
    mrp_mainloop_t      *ml  = (mrp_mainloop_t *)user_data;
    mrp_superloop_ops_t *ops = ml->super_ops;

    MRP_UNUSED(super_data);
    MRP_UNUSED(id);
    MRP_UNUSED(fd);
    MRP_UNUSED(events);

    ops->mod_defer(ml->super_data, ml->work, TRUE);
}


static void super_timer_cb(void *super_data, void *id, void *user_data)
{
    mrp_mainloop_t      *ml  = (mrp_mainloop_t *)user_data;
    mrp_superloop_ops_t *ops = ml->super_ops;

    MRP_UNUSED(super_data);
    MRP_UNUSED(id);

    ops->mod_defer(ml->super_data, ml->work, TRUE);
}


static void super_work_cb(void *super_data, void *id, void *user_data)
{
    mrp_mainloop_t      *ml  = (mrp_mainloop_t *)user_data;
    mrp_superloop_ops_t *ops = ml->super_ops;
    unsigned int         timeout;

    MRP_UNUSED(super_data);
    MRP_UNUSED(id);

    mrp_mainloop_poll(ml, FALSE);
    mrp_mainloop_dispatch(ml);

    if (!ml->quit) {
        mrp_mainloop_prepare(ml);

        /*
         * Notes:
         *
         *     Some mainloop abstractions (eg. the one in PulseAudio)
         *     have deferred callbacks that starve all other event
         *     processing until no more deferred callbacks are pending.
         *     For this reason, we cannot map our deferred callbacks
         *     directly to superloop deferred callbacks (in some cases
         *     this could starve the superloop indefinitely). Hence, if
         *     we have enabled deferred callbacks, we arm our timer with
         *     0 timeout to let the superloop do one round of its event
         *     processing.
         */

        timeout = mrp_list_empty(&ml->deferred) ? ml->poll_timeout : 0;
        ops->mod_timer(ml->super_data, ml->timer, timeout);
        ops->mod_defer(ml->super_data, ml->work, FALSE);
    }
    else {
        ops->del_io(ml->super_data, ml->iow);
        ops->del_timer(ml->super_data, ml->timer);
        ops->del_defer(ml->super_data, ml->work);

        ml->iow   = NULL;
        ml->timer = NULL;
        ml->work  = NULL;
    }
}


static void adjust_superloop_timer(mrp_mainloop_t *ml)
{
    mrp_superloop_ops_t *ops = ml->super_ops;
    unsigned int         timeout;

    if (ops == NULL)
        return;

    mrp_mainloop_prepare(ml);
    timeout = mrp_list_empty(&ml->deferred) ? ml->poll_timeout : 0;
    ops->mod_timer(ml->super_data, ml->timer, timeout);
}


int mrp_set_superloop(mrp_mainloop_t *ml, mrp_superloop_ops_t *ops,
                      void *loop_data)
{
    mrp_io_event_t events;
    int            timeout;

    if (ml->super_ops == NULL) {
        if (ops->poll_io != NULL)
            ops->poll_events = poll_events;

        ml->super_ops  = ops;
        ml->super_data = loop_data;

        mrp_mainloop_prepare(ml);

        events    = MRP_IO_EVENT_IN | MRP_IO_EVENT_OUT | MRP_IO_EVENT_HUP;
        ml->iow   = ops->add_io(ml->super_data, ml->epollfd, events,
                                super_io_cb, ml);
        ml->work  = ops->add_defer(ml->super_data, super_work_cb, ml);

        /*
         * Notes:
         *
         *     Some mainloop abstractions (eg. the one in PulseAudio)
         *     have deferred callbacks that starve all other event
         *     processing until no more deferred callbacks are pending.
         *     For this reason, we cannot map our deferred callbacks
         *     directly to superloop deferred callbacks (in some cases
         *     this could starve the superloop indefinitely). Hence, if
         *     we have enabled deferred callbacks, we arm our timer with
         *     0 timeout to let the superloop do one round of its event
         *     processing.
         */

        timeout   = mrp_list_empty(&ml->deferred) ? ml->poll_timeout : 0;
        ml->timer = ops->add_timer(ml->super_data, timeout, super_timer_cb, ml);

        if (ml->iow != NULL && ml->timer != NULL && ml->work != NULL)
            return TRUE;
        else
            mrp_clear_superloop(ml);
    }

    return FALSE;
}


int mrp_clear_superloop(mrp_mainloop_t *ml)
{
    mrp_superloop_ops_t *ops  = ml->super_ops;
    void                *data = ml->super_data;

    if (ops != NULL) {
        if (ml->iow != NULL) {
            ops->del_io(data, ml->iow);
            ml->iow = NULL;
        }

        if (ml->work != NULL) {
            ops->del_defer(data, ml->work);
            ml->work = NULL;
        }

        if (ml->timer != NULL) {
            ops->del_timer(data, ml->timer);
            ml->timer = NULL;
        }

        ml->super_ops  = NULL;
        ml->super_data = NULL;

        ops->unregister(data);

        return TRUE;
    }
    else
        return FALSE;
}


int mrp_mainloop_unregister(mrp_mainloop_t *ml)
{
    return mrp_clear_superloop(ml);
}


/*
 * mainloop
 */

static void purge_io_watches(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n, *sp, *sn;
    mrp_io_watch_t  *w, *s;

    mrp_list_foreach(&ml->iowatches, p, n) {
        w = mrp_list_entry(p, typeof(*w), hook);
        mrp_list_delete(&w->hook);
        mrp_list_delete(&w->deleted);

        mrp_list_foreach(&w->slave, sp, sn) {
            s = mrp_list_entry(sp, typeof(*s), slave);
            mrp_list_delete(&s->slave);
            mrp_free(s);
        }

        mrp_free(w);
    }
}


static void purge_timers(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_timer_t     *t;

    mrp_list_foreach(&ml->timers, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);
        mrp_list_delete(&t->hook);
        mrp_list_delete(&t->deleted);
        mrp_free(t);
    }
}


static void purge_deferred(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_deferred_t  *d;

    mrp_list_foreach(&ml->deferred, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);
        mrp_list_delete(&d->hook);
        mrp_list_delete(&d->deleted);
        mrp_free(d);
    }

    mrp_list_foreach(&ml->inactive_deferred, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);
        mrp_list_delete(&d->hook);
        mrp_list_delete(&d->deleted);
        mrp_free(d);
    }
}


static void purge_sighandlers(mrp_mainloop_t *ml)
{
    mrp_list_hook_t  *p, *n;
    mrp_sighandler_t *s;

    mrp_list_foreach(&ml->sighandlers, p, n) {
        s = mrp_list_entry(p, typeof(*s), hook);
        mrp_list_delete(&s->hook);
        mrp_list_delete(&s->deleted);
        mrp_free(s);
    }
}


static void purge_wakeups(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_wakeup_t    *w;

    mrp_list_foreach(&ml->wakeups, p, n) {
        w = mrp_list_entry(p, typeof(*w), hook);
        mrp_list_delete(&w->hook);
        mrp_list_delete(&w->deleted);
        mrp_free(w);
    }
}


static void purge_deleted(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    deleted_t       *d;

    mrp_list_foreach(&ml->deleted, p, n) {
        d = mrp_list_entry(p, typeof(*d), deleted);
        mrp_list_delete(&d->deleted);
        mrp_list_delete(&d->hook);
        if (d->free == NULL) {
            mrp_debug("purging deleted object %p", d);
            mrp_free(d);
        }
        else {
            mrp_debug("purging deleted object %p (free cb: %p)", d, d->free);
            if (!d->free(d)) {
                mrp_log_error("Failed to free purged item %p.", d);
                mrp_list_prepend(p, &d->deleted);
            }
        }
    }
}


static void purge_subloops(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_subloop_t   *sl;

    mrp_list_foreach(&ml->subloops, p, n) {
        sl = mrp_list_entry(p, typeof(*sl), hook);
        mrp_list_delete(&sl->hook);
        mrp_list_delete(&sl->deleted);
        free_subloop(sl);
    }
}


mrp_mainloop_t *mrp_mainloop_create(void)
{
    mrp_mainloop_t *ml;

    if ((ml = mrp_allocz(sizeof(*ml))) != NULL) {
        ml->epollfd = epoll_create1(EPOLL_CLOEXEC);
        ml->sigfd   = -1;
        ml->fdtbl   = fdtbl_create();

        if (ml->epollfd >= 0 && ml->fdtbl != NULL) {
            mrp_list_init(&ml->iowatches);
            mrp_list_init(&ml->timers);
            mrp_list_init(&ml->deferred);
            mrp_list_init(&ml->inactive_deferred);
            mrp_list_init(&ml->sighandlers);
            mrp_list_init(&ml->wakeups);
            mrp_list_init(&ml->deleted);
            mrp_list_init(&ml->subloops);
            mrp_list_init(&ml->busses);
            mrp_list_init(&ml->eventq);

            ml->eventd = mrp_add_deferred(ml, pump_events, ml);
            if (ml->eventd == NULL)
                goto fail;
            mrp_disable_deferred(ml->eventd);

            if (!setup_sighandlers(ml))
                goto fail;
        }
        else {
        fail:
            close(ml->epollfd);
            fdtbl_destroy(ml->fdtbl);
            mrp_free(ml);
            ml = NULL;
        }
    }



    return ml;
}


void mrp_mainloop_destroy(mrp_mainloop_t *ml)
{
    if (ml != NULL) {
        mrp_clear_superloop(ml);
        purge_io_watches(ml);
        purge_timers(ml);
        purge_deferred(ml);
        purge_sighandlers(ml);
        purge_wakeups(ml);
        purge_subloops(ml);
        purge_deleted(ml);

        close(ml->sigfd);
        close(ml->epollfd);
        fdtbl_destroy(ml->fdtbl);

        mrp_free(ml->events);
        mrp_free(ml);
    }
}


static int prepare_subloop(mrp_subloop_t *sl)
{
    /*
     * Notes:
     *
     *     If we have a relatively large number of file descriptors to
     *     poll but typically only a small fraction of them has pending
     *     events per mainloop iteration epoll has significant advantages
     *     over poll. This is the main reason why our mainloop uses epoll.
     *     However, there is a considerable amount of pain one needs to
     *     go through to integrate an external poll-based (sub-)mainloop
     *     (e.g. glib's GMainLoop) with an epoll-based mainloop. I mean,
     *     just look at the code below !
     *
     *     If it eventually turns out that we typically only have a small
     *     number of file descriptors while at the same time we practically
     *     always need to pump GMainLoop, it is probably a good idea to
     *     bite the bullet and change our mainloop to be poll-based as well.
     *     But let's not go there yet...
     */


    struct epoll_event  evt;
    struct pollfd      *fds, *pollfds;
    int                 timeout;
    int                 nfd, npollfd, n, i;
    int                 nmatch;
    int                 fd, idx;

    MRP_UNUSED(dump_pollfds);

    mrp_debug("preparing subloop %p", sl);

    pollfds = sl->pollfds;
    npollfd = sl->npollfd;

    if (sl->cb->prepare(sl->user_data)) {
        mrp_debug("subloop %p prepare reported ready, dispatching it", sl);
        sl->cb->dispatch(sl->user_data);
    }
    sl->poll = FALSE;

    nfd = npollfd;
    fds = nfd ? mrp_allocz(nfd * sizeof(*fds)) : NULL;

    MRP_ASSERT(nfd == 0 || fds != NULL, "failed to allocate pollfd's");

    while ((n = sl->cb->query(sl->user_data, fds, nfd, &timeout)) > nfd) {
        fds = mrp_reallocz(fds, nfd, n);
        nfd = n;
        MRP_ASSERT(fds != NULL, "failed to allocate pollfd's");
    }
    nfd = n;


#if 0
    printf("-------------------------\n");
    dump_pollfds("old: ", sl->pollfds, sl->npollfd);
    dump_pollfds("new: ", fds, nfd);
    printf("-------------------------\n");
#endif


    /*
     * skip over the identical portion of the old and new pollfd's
     */

    for (i = nmatch = 0; i < npollfd && i < n; i++, nmatch++) {
        if (fds[i].fd     != pollfds[i].fd ||
            fds[i].events != pollfds[i].events)
            break;
        else
            fds[i].revents = pollfds[i].revents = 0;
    }


    if (nmatch == npollfd && npollfd == nfd) {
        mrp_free(fds);
        goto out;
    }


    /*
     * replace file descriptors with the new set (remove old, add new)
     */

    for (i = 0; i < npollfd; i++) {
        fd = pollfds[i].fd;
        fdtbl_remove(sl->fdtbl, fd);
        if (epoll_ctl(sl->epollfd, EPOLL_CTL_DEL, fd, &evt) < 0) {
            if (errno != EBADF && errno != ENOENT)
                mrp_log_error("Failed to delete subloop fd %d from epoll "
                              "(%d: %s)", fd, errno, strerror(errno));
        }
    }

    for (i = 0; i < nfd; i++) {
        fd  = fds[i].fd;
        idx = i + 1;

        evt.events   = fds[i].events;
        evt.data.u64 = 0;                /* init full union for valgrind... */
        evt.data.fd  = fd;

        if (fdtbl_insert(sl->fdtbl, fd, (void *)(ptrdiff_t)idx) == 0) {
            if (epoll_ctl(sl->epollfd, EPOLL_CTL_ADD, fd, &evt) != 0) {
                mrp_log_error("Failed to add subloop fd %d to epoll "
                              "(%d: %s)", fd, errno, strerror(errno));
            }
        }
        else {
            mrp_log_error("Failed to add subloop fd %d to fd table "
                          "(%d: %s)", fd, errno, strerror(errno));
        }

        fds[i].revents = 0;
    }

    mrp_free(sl->pollfds);
    sl->pollfds = fds;
    sl->npollfd = nfd;


    /*
     * resize event buffer if needed
     */

    if (sl->nevent < nfd) {
        sl->nevent = nfd;
        sl->events = mrp_realloc(sl->events, sl->nevent * sizeof(*sl->events));

        MRP_ASSERT(sl->events != NULL || sl->nevent == 0,
                   "can't allocate epoll event buffer");
    }

 out:
    mrp_debug("subloop %p: fds: %d, timeout: %d, poll: %s",
              sl, sl->npollfd, timeout, sl->poll ? "TRUE" : "FALSE");

    return timeout;
}


static int prepare_subloops(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_subloop_t   *sl;
    int              ext_timeout, min_timeout;

    min_timeout = INT_MAX;

    mrp_list_foreach(&ml->subloops, p, n) {
        sl = mrp_list_entry(p, typeof(*sl), hook);

        if (!is_deleted(sl)) {
            ext_timeout = prepare_subloop(sl);
            min_timeout = MRP_MIN(min_timeout, ext_timeout);
        }
        else
            mrp_debug("skipping deleted subloop %p", sl);
    }

    return min_timeout;
}


#if 0
static inline void dump_timers(mrp_mainloop_t *ml)
{
    mrp_timer_t     *t;
    mrp_list_hook_t *p, *n;
    int              i;
    mrp_timer_t     *next = NULL;

    mrp_debug("timer dump:");
    i = 0;
    mrp_list_foreach(&ml->timers, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        mrp_debug("  #%d: %p, @%u, next %llu (%s)", i, t, t->msecs, t->expire,
                  is_deleted(t) ? "DEAD" : "alive");

        if (!is_deleted(t) && next == NULL)
            next = t;

        i++;
    }

    mrp_debug("next timer: %p", ml->next_timer);
    mrp_debug("poll timer: %d", ml->poll_timeout);

    if (next != NULL && ml->next_timer != NULL &&
        !is_deleted(ml->next_timer) && next != ml->next_timer) {
        mrp_debug("*** BUG ml->next_timer is not the nearest !!! ***");
        if (getenv("__MURPHY_TIMER_CHECK_ABORT") != NULL)
            abort();
    }
}
#endif


int mrp_mainloop_prepare(mrp_mainloop_t *ml)
{
    mrp_timer_t *next_timer;
    int          timeout, ext_timeout;
    uint64_t     now;

    if (!mrp_list_empty(&ml->deferred)) {
        timeout = 0;
    }
    else {
        next_timer = ml->next_timer;

        if (next_timer == NULL)
            timeout = -1;
        else {
            now = time_now();
            if (MRP_UNLIKELY(next_timer->expire <= now))
                timeout = 0;
            else
                timeout = usecs_to_msecs(next_timer->expire - now);
        }
    }

    ext_timeout = prepare_subloops(ml);

    if (ext_timeout != -1 && timeout != -1)
        ml->poll_timeout = MRP_MIN(timeout, ext_timeout);
    else if (ext_timeout == -1)
        ml->poll_timeout = timeout;
    else
        ml->poll_timeout = ext_timeout;

    if (ml->nevent < ml->niowatch) {
        ml->nevent = ml->niowatch;
        ml->events = mrp_realloc(ml->events, ml->nevent * sizeof(*ml->events));

        MRP_ASSERT(ml->events != NULL, "can't allocate epoll event buffer");
    }

    mrp_debug("mainloop %p prepared: %d I/O watches, timeout %d", ml,
              ml->niowatch, ml->poll_timeout);

    return TRUE;
}


static size_t poll_events(void *id, mrp_mainloop_t *ml, void **bufp)
{
    void *buf;
    int   n;

    if (MRP_UNLIKELY(id != ml->iow)) {
        mrp_log_error("superloop polling with invalid I/O watch (%p != %p)",
                      id, ml->iow);
        *bufp = NULL;
        return 0;
    }

    buf = mrp_allocz(ml->nevent * sizeof(ml->events[0]));

    if (buf != NULL) {
        n = epoll_wait(ml->epollfd, buf, ml->nevent, 0);

        if (n < 0)
            n = 0;
    }
    else
        n = 0;

    *bufp = buf;
    return n * sizeof(ml->events[0]);
}


int mrp_mainloop_poll(mrp_mainloop_t *ml, int may_block)
{
    int n, timeout;

    timeout = may_block && mrp_list_empty(&ml->deferred) ? ml->poll_timeout : 0;

    if (ml->nevent > 0) {
        if (ml->super_ops == NULL || ml->super_ops->poll_io == NULL) {
            mrp_debug("polling %d descriptors with timeout %d",
                      ml->nevent, timeout);

            n = epoll_wait(ml->epollfd, ml->events, ml->nevent, timeout);

            if (n < 0 && errno == EINTR)
                n = 0;
        }
        else {
            mrp_superloop_ops_t *super_ops  = ml->super_ops;
            void                *super_data = ml->super_data;
            void                *id         = ml->iow;
            void                *buf        = ml->events;
            size_t               size       = ml->nevent * sizeof(ml->events[0]);

            size = super_ops->poll_io(super_data, id, buf, size);
            n    = size / sizeof(ml->events[0]);

            MRP_ASSERT(n * sizeof(ml->events[0]) == size,
                       "superloop passed us a partial epoll_event");
        }

        mrp_debug("mainloop %p has %d/%d I/O events waiting", ml, n,
                  ml->nevent);

        ml->poll_result = n;
    }
    else {
        /*
         * Notes: Practically we should never branch here because
         *     we always have at least ml->sigfd registered for epoll.
         */
        if (timeout > 0)
            usleep(timeout * USECS_PER_MSEC);

        ml->poll_result = 0;
    }

    return TRUE;
}


static int poll_subloop(mrp_subloop_t *sl)
{
    struct epoll_event *e;
    struct pollfd      *pfd;
    int                 fd, idx, n, i;

    if (sl->poll) {
        n = epoll_wait(sl->epollfd, sl->events, sl->nevent, 0);

        if (n < 0 && errno == EINTR)
            n = 0;

        for (i = 0, e = sl->events; i < n; i++, e++) {
            fd  = e->data.fd;
            idx = ((int)(ptrdiff_t)fdtbl_lookup(sl->fdtbl, fd)) - 1;

            if (0 <= idx && idx < sl->npollfd) {
                pfd = sl->pollfds + idx;
                pfd->revents = e->events;
            }
        }

        mrp_debug("subloop %p has %d fds ready", sl, sl->npollfd);

        return n;
    }
    else {
        mrp_debug("subloop %p has poll flag off", sl);

        return 0;
    }
}


static void dispatch_wakeup(mrp_mainloop_t *ml)
{
    mrp_list_hook_t    *p, *n;
    mrp_wakeup_t       *w;
    mrp_wakeup_event_t  event;
    uint64_t            now;

    if (ml->poll_timeout == 0) {
        mrp_debug("skipping wakeup callbacks (poll timeout was 0)");
        return;
    }

    if (ml->poll_result == 0) {
        mrp_debug("woken up by timeout");
        event = MRP_WAKEUP_EVENT_TIMER;
    }
    else {
        mrp_debug("woken up by I/O (or signal)");
        event = MRP_WAKEUP_EVENT_IO;
    }

    now = time_now();

    mrp_list_foreach(&ml->wakeups, p, n) {
        w = mrp_list_entry(p, typeof(*w), hook);

        if (!(w->events & event))
            continue;

        if (!is_deleted(w)) {
            mrp_debug("dispatching wakeup cb %p", w);
            wakeup_cb(w, event, now);
        }
        else
            mrp_debug("skipping deleted wakeup cb %p", w);

        if (ml->quit)
            break;
    }
}


static void dispatch_deferred(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_deferred_t  *d;

    mrp_list_foreach(&ml->deferred, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);

        if (!is_deleted(d) && !d->inactive) {
            mrp_debug("dispatching active deferred cb %p", d);
            d->cb(d, d->user_data);
        }
        else
            mrp_debug("skipping %s deferred cb %p",
                      is_deleted(d) ? "deleted" : "inactive", d);

        if (!is_deleted(d) && d->inactive)
            disable_deferred(d);

        if (ml->quit)
            break;
    }
}


static void dispatch_timers(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_timer_t     *t;
    uint64_t         now;

    now = time_now();

    mrp_list_foreach(&ml->timers, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        if (!is_deleted(t)) {
            if (t->expire <= now) {
                mrp_debug("dispatching expired timer %p", t);

                t->cb(t, t->user_data);

                if (!is_deleted(t))
                    rearm_timer(t);
            }
            else
                break;
        }
        else
            mrp_debug("skipping deleted timer %p", t);

        if (ml->quit)
            break;
    }
}


static void dispatch_subloops(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_subloop_t   *sl;

    mrp_list_foreach(&ml->subloops, p, n) {
        sl = mrp_list_entry(p, typeof(*sl), hook);

        if (!is_deleted(sl)) {
            poll_subloop(sl);

            if (sl->cb->check(sl->user_data, sl->pollfds,
                              sl->npollfd)) {
                mrp_debug("dispatching subloop %p", sl);
                sl->cb->dispatch(sl->user_data);
            }
            else
                mrp_debug("skipping subloop %p, check said no", sl);
        }
    }
}


static void dispatch_slaves(mrp_io_watch_t *w, struct epoll_event *e)
{
    mrp_io_watch_t  *s;
    mrp_list_hook_t *p, *n;
    mrp_io_event_t   events;

    events = e->events & ~(MRP_IO_EVENT_INOUT & w->events);

    mrp_list_foreach(&w->slave, p, n) {
        if (events == MRP_IO_EVENT_NONE)
            break;

        s = mrp_list_entry(p, typeof(*s), slave);

        if (!is_deleted(s)) {
            mrp_debug("dispatching slave I/O watch %p (fd %d)", s, s->fd);
            s->cb(s, s->fd, events, s->user_data);
        }
        else
            mrp_debug("skipping slave I/O watch %p (fd %d)", s, s->fd);

        events &= ~(MRP_IO_EVENT_INOUT & s->events);
    }
}


static void dispatch_poll_events(mrp_mainloop_t *ml)
{
    struct epoll_event *e;
    mrp_io_watch_t     *w, *tblw;
    int                 i, fd;

    for (i = 0, e = ml->events; i < ml->poll_result; i++, e++) {
        fd = e->data.fd;
        w  = fdtbl_lookup(ml->fdtbl, fd);

        if (w == NULL) {
            mrp_debug("ignoring event for deleted fd %d", fd);
            continue;
        }

        if (!is_deleted(w)) {
            mrp_debug("dispatching I/O watch %p (fd %d)", w, fd);
            w->cb(w, w->fd, e->events, w->user_data);
        }
        else
            mrp_debug("skipping deleted I/O watch %p (fd %d)", w, fd);

        if (!mrp_list_empty(&w->slave))
            dispatch_slaves(w, e);

        if (e->events & EPOLLRDHUP) {
            tblw = fdtbl_lookup(ml->fdtbl, w->fd);

            if (tblw == w) {
                mrp_debug("forcibly stop polling fd %d for watch %p", w->fd, w);
                epoll_del(w);
            }
            else if (tblw != NULL)
                mrp_debug("don't stop polling reused fd %d of watch %p",
                          w->fd, w);
        }
        else {
            if ((e->events & EPOLLHUP) && !is_deleted(w)) {
                /*
                 * Notes:
                 *
                 *    If the user does not react to EPOLLHUPs delivered
                 *    we stop monitoring the fd to avoid sitting in an
                 *    infinite busy loop just delivering more EPOLLHUP
                 *    notifications...
                 */

                if (w->wrhup++ > 5) {
                    tblw = fdtbl_lookup(ml->fdtbl, w->fd);

                    if (tblw == w) {
                        mrp_debug("forcibly stop polling fd %d for watch %p",
                                  w->fd, w);
                        epoll_del(w);
                    }
                    else if (tblw != NULL)
                        mrp_debug("don't stop polling reused fd %d of watch %p",
                                  w->fd, w);
                }
            }
        }

        if (ml->quit)
            break;
    }

    if (ml->quit)
        return;

    dispatch_subloops(ml);

    mrp_debug("done dispatching poll events");
}


int mrp_mainloop_dispatch(mrp_mainloop_t *ml)
{
    dispatch_wakeup(ml);

    if (ml->quit)
        goto quit;

    dispatch_deferred(ml);

    if (ml->quit)
        goto quit;

    dispatch_timers(ml);

    if (ml->quit)
        goto quit;

    dispatch_poll_events(ml);

 quit:
    purge_deleted(ml);

    return !ml->quit;
}


int mrp_mainloop_iterate(mrp_mainloop_t *ml)
{
    return
        mrp_mainloop_prepare(ml) &&
        mrp_mainloop_poll(ml, TRUE) &&
        mrp_mainloop_dispatch(ml) &&
        !ml->quit;
}


int mrp_mainloop_run(mrp_mainloop_t *ml)
{
    while (mrp_mainloop_iterate(ml))
        ;

    return ml->exit_code;
}


void mrp_mainloop_quit(mrp_mainloop_t *ml, int exit_code)
{
    ml->exit_code = exit_code;
    ml->quit      = TRUE;
}


/*
 * debugging routines
 */


static void dump_pollfds(const char *prefix, struct pollfd *fds, int nfd)
{
    char *t;
    int   i;

    printf("%s (%d): ", prefix, nfd);
    for (i = 0, t = ""; i < nfd; i++, t = ", ")
        printf("%s%d/0x%x", t, fds[i].fd, fds[i].events);
    printf("\n");
}


/*
 * event bus and events
 */

static inline void *ref_event_data(void *data, int format)
{
    switch (format & MRP_EVENT_FORMAT_MASK) {
    case MRP_EVENT_FORMAT_JSON:
        return mrp_json_ref((mrp_json_t *)data);
    case MRP_EVENT_FORMAT_MSG:
        return mrp_msg_ref((mrp_msg_t *)data);
    default:
        return data;
    }
}


static inline void unref_event_data(void *data, int format)
{
    switch (format & MRP_EVENT_FORMAT_MASK) {
    case MRP_EVENT_FORMAT_JSON:
        mrp_json_unref((mrp_json_t *)data);
        break;
    case MRP_EVENT_FORMAT_MSG:
        mrp_msg_unref((mrp_msg_t *)data);
        break;
    default:
        break;
    }
}


mrp_event_bus_t *mrp_event_bus_get(mrp_mainloop_t *ml, const char *name)
{
    mrp_list_hook_t *p, *n;
    mrp_event_bus_t *bus;

    if (name == NULL || !strcmp(name, MRP_GLOBAL_BUS_NAME))
        return MRP_GLOBAL_BUS;

    mrp_list_foreach(&ml->busses, p, n) {
        bus = mrp_list_entry(p, typeof(*bus), hook);

        if (!strcmp(bus->name, name))
            return bus;
    }

    bus = mrp_allocz(sizeof(*bus));

    if (bus == NULL)
        return NULL;

    bus->name = mrp_strdup(name);

    if (bus->name == NULL) {
        mrp_free(bus);
        return NULL;
    }

    mrp_list_init(&bus->hook);
    mrp_list_init(&bus->watches);
    bus->ml = ml;

    mrp_list_append(&ml->busses, &bus->hook);

    return bus;
}


uint32_t mrp_event_id(const char *name)
{
    mrp_event_def_t *e;
    int              i;

    if (events != NULL)
        for (i = 0, e = events; i < nevent; i++, e++)
            if (!strcmp(e->name, name))
                return e->id;

    if (!mrp_reallocz(events, nevent, nevent + 1))
        return 0;

    e = events + nevent;

    e->id   = nevent;
    e->name = mrp_strdup(name);

    if (e->name == NULL) {
        mrp_reallocz(events, nevent + 1, nevent);
        return 0;
    }

    nevent++;

    return e->id;
}


const char *mrp_event_name(uint32_t id)
{
    if ((int)id < nevent)
        return events[id].name;
    else
        return MRP_EVENT_UNKNOWN_NAME;
}


char *mrp_event_dump_mask(mrp_event_mask_t *mask, char *buf, size_t size)
{
    char *p, *t;
    int   l, n, id;

    p = buf;
    l = (int)size;
    t = "";

    MRP_MASK_FOREACH_SET(mask, id, 1) {
        n = snprintf(p, l, "%s%s", t, mrp_event_name(id));
        t = "|";

        if (n >= l)
            return "<insufficient mask dump buffer>";

        p += n;
        l -= n;
    }

    return buf;
}


mrp_event_watch_t *mrp_event_add_watch(mrp_event_bus_t *bus, uint32_t id,
                                       mrp_event_watch_cb_t cb, void *user_data)
{
    mrp_list_hook_t   *watches = bus ? &bus->watches : &ewatches;
    mrp_event_watch_t *w;

    w = mrp_allocz(sizeof(*w));

    if (w == NULL)
        return NULL;

    mrp_list_init(&w->hook);
    mrp_mask_init(&w->mask);
    w->bus       = bus;
    w->cb        = cb;
    w->user_data = user_data;

    if (!mrp_mask_set(&w->mask, id)) {
        mrp_free(w);
        return NULL;
    }

    mrp_list_append(watches, &w->hook);

    mrp_debug("added event watch %p for event %d (%s) on bus %s", w, id,
              mrp_event_name(id), bus ? bus->name : MRP_GLOBAL_BUS_NAME);

    return w;
}


mrp_event_watch_t *mrp_event_add_watch_mask(mrp_event_bus_t *bus,
                                            mrp_event_mask_t *mask,
                                            mrp_event_watch_cb_t cb,
                                            void *user_data)
{
    mrp_list_hook_t   *watches = bus ? &bus->watches : &ewatches;
    mrp_event_watch_t *w;
    char               events[512];

    w = mrp_allocz(sizeof(*w));

    if (w == NULL)
        return NULL;

    mrp_list_init(&w->hook);
    mrp_mask_init(&w->mask);
    w->bus       = bus;
    w->cb        = cb;
    w->user_data = user_data;

    if (!mrp_mask_copy(&w->mask, mask)) {
        mrp_free(w);
        return NULL;
    }

    mrp_list_append(watches, &w->hook);

    mrp_debug("added event watch %p for events <%s> on bus %s", w,
              mrp_event_dump_mask(&w->mask, events, sizeof(events)),
              bus ? bus->name : MRP_GLOBAL_BUS_NAME);

    return w;
}


void mrp_event_del_watch(mrp_event_watch_t *w)
{
    if (w == NULL)
        return;

    if (w->bus != NULL && w->bus->busy) {
        w->dead = TRUE;
        w->bus->dead++;
        return;
    }

    mrp_list_delete(&w->hook);
    mrp_mask_reset(&w->mask);
    mrp_free(w);
}


void bus_purge_dead(mrp_event_bus_t *bus)
{
    mrp_event_watch_t *w;
    mrp_list_hook_t   *p, *n;

    if (!bus->dead)
        return;

    mrp_list_foreach(&bus->watches, p, n) {
        w = mrp_list_entry(p, typeof(*w), hook);

        if (!w->dead)
            continue;

        mrp_list_delete(&w->hook);
        mrp_mask_reset(&w->mask);
        mrp_free(w);
    }

    bus->dead = 0;
}


static int queue_event(mrp_event_bus_t *bus, uint32_t id, void *data,
                       mrp_event_flag_t flags)
{
    pending_event_t *e;

    e = mrp_allocz(sizeof(*e));

    if (e == NULL)
        return -1;

    mrp_list_init(&e->hook);
    e->bus    = bus;
    e->id     = id;
    e->format = flags & MRP_EVENT_FORMAT_MASK;
    e->data   = ref_event_data(data, e->format);
    mrp_list_append(&bus->ml->eventq, &e->hook);

    mrp_enable_deferred(bus->ml->eventd);

    return 0;
}


static int emit_event(mrp_event_bus_t *bus, uint32_t id, void *data,
                      mrp_event_flag_t flags)
{
    mrp_list_hook_t   *watches;
    mrp_event_watch_t *w;
    mrp_list_hook_t   *p, *n;

    if (bus)
        watches = &bus->watches;
    else {
        if (!(flags & MRP_EVENT_SYNCHRONOUS)) {
            errno = EINVAL;
            return -1;
        }
        watches = &ewatches;
    }

    if (bus)
        bus->busy++;

    mrp_debug("emitting event 0x%x (%s) on bus <%s>", id, mrp_event_name(id),
              bus ? bus->name : MRP_GLOBAL_BUS_NAME);

    mrp_list_foreach(watches, p, n) {
        w = mrp_list_entry(p, typeof(*w), hook);

        if (w->dead)
            continue;

        if (mrp_mask_test(&w->mask, id))
            w->cb(w, id, flags & MRP_EVENT_FORMAT_MASK, data, w->user_data);
    }

    if (bus) {
        bus->busy--;

        if (!bus->busy)
            bus_purge_dead(bus);
    }

    return 0;
}


static void pump_events(mrp_deferred_t *d, void *user_data)
{
    mrp_mainloop_t  *ml = (mrp_mainloop_t *)user_data;
    mrp_list_hook_t *p, *n;
    pending_event_t *e;

 pump:
    mrp_list_foreach(&ml->eventq, p, n) {
        e = mrp_list_entry(p, typeof(*e), hook);

        emit_event(e->bus, e->id, e->data, e->format);

        mrp_list_delete(&e->hook);
        unref_event_data(e->data, e->format);

        mrp_free(e);
    }

    if (!mrp_list_empty(&ml->eventq))
        goto pump;

    mrp_disable_deferred(d);
}


int mrp_emit_event(mrp_event_bus_t *bus, uint32_t id, mrp_event_flag_t flags,
                   void *data)
{
    int status;

    if (flags & MRP_EVENT_SYNCHRONOUS) {
        ref_event_data(data, flags);
        status = emit_event(bus, id, data, flags);
        unref_event_data(data, flags);

        return status;
    }
    else {
        if (bus != NULL)
            return queue_event(bus, id, data, flags);

        errno = EOPNOTSUPP;
        return -1;
    }
}


int _mrp_event_emit_msg(mrp_event_bus_t *bus, uint32_t id,
                        mrp_event_flag_t flags, ...)
{
    mrp_msg_t *msg;
    uint16_t   tag;
    va_list    ap;
    int        status;

    va_start(ap, flags);
    tag = va_arg(ap, unsigned int);
    msg = tag ? mrp_msg_createv(tag, ap) : NULL;
    va_end(ap);

    flags &= ~MRP_EVENT_FORMAT_MASK;
    status = mrp_emit_event(bus, id, flags | MRP_EVENT_FORMAT_MSG, msg);
    mrp_msg_unref(msg);

    return status;
}


MRP_INIT static void init_events(void)
{
    MRP_ASSERT(mrp_event_id(MRP_EVENT_UNKNOWN_NAME) == MRP_EVENT_UNKNOWN,
               "reserved id 0x%x for builtin event <%s> already taken",
               MRP_EVENT_UNKNOWN, MRP_EVENT_UNKNOWN_NAME);
}
