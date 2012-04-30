#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/mainloop.h>

#define USECS_PER_SEC  (1000 * 1000)
#define USECS_PER_MSEC (1000)
#define NSECS_PER_USEC (1000)

/*
 * I/O watches
 */

struct mrp_io_watch_s {
    mrp_list_hook_t    hook;                     /* to list of watches */
    int              (*free)(void *ptr);         /* cb to free memory */
    mrp_mainloop_t    *ml;                       /* mainloop */
    int                fd;                       /* file descriptor to watch */
    mrp_io_event_t     events;                   /* events of interest */
    mrp_io_watch_cb_t  cb;                       /* user callback */
    void              *user_data;                /* opaque user data */
    struct pollfd     *pollfd;                   /* associated pollfd */
    mrp_list_hook_t    slave;                    /* watches with the same fd */
};

#define is_master(w) !mrp_list_empty(&(w)->hook)
#define is_slave(w)  !mrp_list_empty(&(w)->slave)


/*
 * timers
 */

struct mrp_timer_s {
    mrp_list_hook_t  hook;                       /* to list of timers */
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
    int                (*free)(void *ptr);       /* cb to free memory */
    mrp_mainloop_t      *ml;                     /* mainloop */
    int                  signum;                 /* signal number */
    mrp_sighandler_cb_t  cb;                     /* user callback */
    void                *user_data;              /* opaque user data */
};


#define mark_deleted(o) ((o)->cb = NULL)
#define is_deleted(o)   ((o)->cb == NULL)


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
    int            (*free)(void *ptr);           /* cb to free memory */
} deleted_t;


/*
 * external mainloops
 */

struct mrp_subloop_s {
    mrp_list_hook_t     hook;                    /* to list of subloops */
    int               (*free)(void *ptr);        /* cb to free memory */
    mrp_subloop_ops_t  *cb;                      /* subloop glue callbacks */
    void               *user_data;               /* opaque subloop data */
    int                 epollfd;                 /* epollfd for this subloop */
    struct epoll_event *events;                  /* epoll event buffer */
    int                 nevent;                  /* epoll event buffer size */
    mrp_io_watch_t     *w;                       /* watch for epollfd */
    struct pollfd      *pollfds;                 /* pollfds for this subloop */
    int                 npollfd;                 /* number of pollfds */
    int                 pending;                 /* pending events */
    int                 poll;                    /* need to poll for events */
};



/*
 * main loop
 */

struct mrp_mainloop_s {
    int                 epollfd;                 /* our epoll descriptor */
    struct epoll_event *events;                  /* epoll event buffer */
    int                 nevent;                  /* epoll event buffer size */

    mrp_list_hook_t     iowatches;               /* list of I/O watches */
    int                 niowatch;                /* number of I/O watches */

    mrp_list_hook_t     timers;                  /* list of timers */
    mrp_timer_t        *next_timer;              /* next expiring timer */

    mrp_list_hook_t     deferred;                /* list of deferred cbs */
    mrp_list_hook_t     inactive_deferred;       /* inactive defferred cbs */

    int                 poll_timeout;            /* next poll timeout */
    int                 poll_result;             /* return value from poll */

    int                 sigfd;                   /* signal polling fd */
    sigset_t            sigmask;                 /* signal mask */
    mrp_io_watch_t     *sigwatch;                /* sigfd I/O watch */
    mrp_list_hook_t     sighandlers;             /* signal handlers */

    mrp_list_hook_t     subloops;                /* external main loops */
    
    mrp_list_hook_t     deleted;                 /* unfreed deleted items */
    int                 quit;                    /* TRUE if _quit called */
    int                 exit_code;               /* returned from _run */
};


static void dump_pollfds(const char *prefix, struct pollfd *fds, int nfd);


/*
 * I/O watches
 */


static int add_slave_io_watch(mrp_io_watch_t *w)
{
    mrp_mainloop_t     *ml = w->ml;
    struct epoll_event  evt;
    mrp_io_watch_t     *master, *slave;
    mrp_list_hook_t    *p, *n, *sp, *sn;

    mrp_list_foreach(&ml->iowatches, p, n) {
	master = mrp_list_entry(p, typeof(*master), hook);
	if (master->fd != w->fd)
	    continue;
	
	evt.events   = master->events;
	evt.data.ptr = master;
	
	mrp_list_foreach(&master->slave, sp, sn) {
	    slave = mrp_list_entry(sp, typeof(*slave), slave);
	    evt.events |= slave->events;
	}
	
	slave = w;
	evt.events |= slave->events;
	if (epoll_ctl(ml->epollfd, EPOLL_CTL_MOD, master->fd, &evt) == 0) {
	    mrp_list_append(&master->slave, &slave->slave);
	    return TRUE;
	}
	else
	    break;
    }

    return FALSE;
}


static uint32_t slave_io_events(mrp_io_watch_t *watch, mrp_io_watch_t **master)
{
    mrp_list_hook_t *p, *n;
    mrp_io_watch_t  *w;
    uint32_t         events;

    events = watch->events;
    mrp_list_foreach(&watch->slave, p, n) {
	w       = mrp_list_entry(p, typeof(*w), slave);
	events |= w->events;
	
	if (master != NULL && !mrp_list_empty(&w->hook))
	    *master = w;
    }

    return events;
}


static int free_io_watch(void *ptr)
{
    mrp_io_watch_t     *w = (typeof(w))ptr;
    struct epoll_event  evt;

    /*
     * try removing from epoll if we failed to do so
     */
    
    if (w->fd != -1) {
	if (epoll_ctl(w->ml->epollfd, EPOLL_CTL_DEL, w->fd, &evt) != 0)
	    if (errno != EBADF)
		return FALSE;
    }
    
    mrp_free(w);
    
    return TRUE;
}


mrp_io_watch_t *mrp_add_io_watch(mrp_mainloop_t *ml, int fd,
				 mrp_io_event_t events,
				 mrp_io_watch_cb_t cb, void *user_data)
{
    struct epoll_event  evt;
    mrp_io_watch_t     *w;

    if (fd < 0 || cb == NULL)
	return NULL;
    
    if ((w = mrp_allocz(sizeof(*w))) != NULL) {
	mrp_list_init(&w->hook);
	mrp_list_init(&w->slave);
	w->ml        = ml;
	w->fd        = fd;
	w->events    = events & MRP_IO_EVENT_ALL;
	w->cb        = cb;
	w->user_data = user_data;
	w->free      = free_io_watch;
	
	evt.events   = w->events;
	evt.data.ptr = w;
	
	if (epoll_ctl(ml->epollfd, EPOLL_CTL_ADD, w->fd, &evt) == 0) {
	    mrp_list_append(&ml->iowatches, &w->hook);
	    ml->niowatch++;
	}
	else {
	    if (errno != EEXIST || !add_slave_io_watch(w)) {
		mrp_free(w);
		w = NULL;
	    }
	}
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

    if (w != NULL)
	mark_deleted(w);
}


static void delete_io_watch(mrp_io_watch_t *w)
{
    mrp_mainloop_t     *ml = w->ml;
    mrp_io_watch_t     *master;
    struct epoll_event  evt;
    int                 op, was_master;

    was_master = is_master(w);
    mrp_list_delete(&w->hook);

    if (was_master) {
	if (mrp_list_empty(&w->slave)) {
	    op = EPOLL_CTL_DEL;
	}
	else {
	    /* relink first slave as new master to mainloop */
	    master = mrp_list_entry(w->slave.next, typeof(*master), slave);
	    mrp_list_delete(&w->slave);
	    mrp_list_append(&ml->iowatches, &master->hook);
	    /* relink old master to deleted list */
	    mrp_list_init(&w->slave);
	    mrp_list_append(&ml->deleted, &w->hook);

	    op           = EPOLL_CTL_MOD;
	    evt.events   = master->events | slave_io_events(master, NULL);
	    evt.data.ptr = master;
	}
    }
    else {
	master       = NULL;
	w->events    = 0;
	op           = EPOLL_CTL_MOD;
	evt.events   = slave_io_events(w, &master);
	evt.data.ptr = master;

	mrp_list_delete(&w->slave);
	mrp_list_init(&w->slave);
	mrp_list_append(&ml->deleted, &w->hook);
    }
    
    if (epoll_ctl(ml->epollfd, op, w->fd, &evt) == 0 ||
	errno == EBADF || errno == ENOENT)
	w->fd = -1;
    else
	mrp_log_error("Failed to update epoll for deleted I/O watch %p.", w);
    
    ml->niowatch--;
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
    mrp_timer_t     *t1;
    int              inserted;
    
    /*
     * Notes:
     *     If there is ever a need to run a large number of
     *     simultaneous timers, we need to change this to a
     *     self-balancing data structure, eg. an red-black tree.
     */

    inserted = FALSE;
    mrp_list_foreach(&ml->timers, p, n) {
	t1 = mrp_list_entry(p, mrp_timer_t, hook);
	
	if (!is_deleted(t1) && t->expire <= t1->expire) {
	    mrp_list_prepend(p->prev, &t->hook);
	    inserted = TRUE;
	    break;
	}
    }
    
    if (!inserted)
	mrp_list_append(&ml->timers, &t->hook);
    
    if (ml->next_timer == NULL || t->expire < ml->next_timer->expire)
	ml->next_timer = t;
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


mrp_timer_t *mrp_add_timer(mrp_mainloop_t *ml, unsigned int msecs,
			   mrp_timer_cb_t cb, void *user_data)
{
    mrp_timer_t *t;
    
    if (cb == NULL)
	return NULL;

    if ((t = mrp_allocz(sizeof(*t))) != NULL) {
	mrp_list_init(&t->hook);
	t->ml        = ml;
	t->expire    = time_now() + msecs * USECS_PER_MSEC;
	t->msecs     = msecs;
	t->cb        = cb;
	t->user_data = user_data;
	
	insert_timer(t);
    }

    return t;
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
    
    if (t != NULL) {
	mark_deleted(t);
	
	if (t->ml->next_timer == t)
	    find_next_timer(t->ml);
    }
}


static inline void delete_timer(mrp_timer_t *t)
{
    mrp_list_delete(&t->hook);
    mrp_list_append(&t->ml->deleted, &t->hook);
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
	d->ml        = ml;
	d->cb        = cb;
	d->user_data = user_data;

	mrp_list_append(&ml->deferred, &d->hook);
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
    
    if (d != NULL)
	mark_deleted(d);
}


static inline void delete_deferred(mrp_deferred_t *d)
{
    mrp_list_delete(&d->hook);
    mrp_list_append(&d->ml->deleted, &d->hook);
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


/*
 * signal notifications
 */

static inline void delete_sighandler(mrp_sighandler_t *h)
{
    mrp_list_delete(&h->hook);
    mrp_list_append(&h->ml->deleted, &h->hook);
}


static void dispatch_signals(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			     mrp_io_event_t events, void *user_data)
{
    struct signalfd_siginfo  sig;
    mrp_list_hook_t         *p, *n;
    mrp_sighandler_t        *h;
    int                      signum;

    MRP_UNUSED(w);
    MRP_UNUSED(events);
    MRP_UNUSED(user_data);

    while (read(fd, &sig, sizeof(sig)) > 0) {
	signum = sig.ssi_signo;
	
	mrp_list_foreach(&ml->sighandlers, p, n) {
	    h = mrp_list_entry(p, typeof(*h), hook);

	    if (!is_deleted(h)) {
		if (h->signum == signum)
		    h->cb(ml, h, signum, h->user_data);
	    }

	    if (is_deleted(h))
		delete_sighandler(h);
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
    if (h != NULL) {
	if (!is_deleted(h)) {
	    mark_deleted(h);
	    recalc_sigmask(h->ml);
	}
    }
}


/*
 * external mainloops
 */

static int free_subloop(void *ptr)
{
    mrp_subloop_t *sl = (mrp_subloop_t *)ptr;
    
    mrp_free(sl->pollfds);
    mrp_free(sl->events);
    mrp_free(sl);

    return TRUE;
}


static void subloop_event_cb(mrp_mainloop_t *ml, mrp_io_watch_t *w, int fd,
			     mrp_io_event_t events, void *user_data)
{
    mrp_subloop_t *sl = (mrp_subloop_t *)user_data;
    
    MRP_UNUSED(ml);
    MRP_UNUSED(w);
    MRP_UNUSED(fd);
    MRP_UNUSED(events);

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
	sl->free      = free_subloop;
	sl->cb        = ops;
	sl->user_data = user_data;
	sl->epollfd   = epoll_create1(EPOLL_CLOEXEC);

	if (sl->epollfd < 0) {
	    mrp_free(sl);
	    return NULL;
	}

	sl->w = mrp_add_io_watch(ml, sl->epollfd, MRP_IO_EVENT_IN,
				 subloop_event_cb, sl);

	if (sl->w == NULL) {
	    close(sl->epollfd);
	    mrp_free(sl);
	    return NULL;
	}

	mrp_list_append(&ml->subloops, &sl->hook);
    }
    
    return sl;
}


void mrp_del_subloop(mrp_subloop_t *sl)
{
    mrp_mainloop_t     *ml;
    struct epoll_event  dummy;
    int                 i;

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
	ml = sl->w->ml;
	mrp_del_io_watch(sl->w);

	for (i = 0; i < sl->npollfd; i++)
	    epoll_ctl(sl->epollfd, EPOLL_CTL_DEL, sl->pollfds[i].fd, &dummy);
	
	close(sl->epollfd);
	sl->epollfd = -1;
	
	mark_deleted(sl);
	mrp_list_delete(&sl->hook);
	mrp_list_append(&ml->deleted, &sl->hook);
    }
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
	mrp_free(d);
    }

    mrp_list_foreach(&ml->inactive_deferred, p, n) {
	d = mrp_list_entry(p, typeof(*d), hook);
	mrp_list_delete(&d->hook);
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
	mrp_free(s);
    }
}


static void purge_deleted(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    deleted_t       *d;

    mrp_list_foreach(&ml->deleted, p, n) {
	d = mrp_list_entry(p, typeof(*d), hook);
	mrp_list_delete(&d->hook);
	if (d->free == NULL)
	    mrp_free(d);
	else {
	    if (!d->free(d)) {
		mrp_log_error("Failed to free purged item %p.", d);
		mrp_list_prepend(p, &d->hook);
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
	free_subloop(sl);
    }
}


mrp_mainloop_t *mrp_mainloop_create(void)
{
    mrp_mainloop_t *ml;

    if ((ml = mrp_allocz(sizeof(*ml))) != NULL) {
	ml->epollfd = epoll_create1(EPOLL_CLOEXEC);
	ml->sigfd   = -1;

	if (ml->epollfd >= 0) {
	    mrp_list_init(&ml->iowatches);
	    mrp_list_init(&ml->timers);
	    mrp_list_init(&ml->deferred);
	    mrp_list_init(&ml->inactive_deferred);
	    mrp_list_init(&ml->sighandlers);
	    mrp_list_init(&ml->deleted);
	    mrp_list_init(&ml->subloops);

	    if (!setup_sighandlers(ml)) {
		close(ml->epollfd);
		goto fail;
	    }

	}
	else {
	fail:
	    mrp_free(ml);
	    ml = NULL;
	}
    }
    
    return ml;
}


void mrp_mainloop_destroy(mrp_mainloop_t *ml)
{
    if (ml != NULL) {
	purge_io_watches(ml);
	purge_timers(ml);
	purge_deferred(ml);
	purge_sighandlers(ml);
	purge_subloops(ml);
	purge_deleted(ml);
	
	
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

    pollfds = sl->pollfds;
    npollfd = sl->npollfd;
    
    if (sl->cb->prepare(sl->user_data))
	sl->cb->dispatch(sl->user_data);
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

    for (i = 0 ; i < npollfd; i++)
	epoll_ctl(sl->epollfd, EPOLL_CTL_DEL, pollfds[i].fd, &evt);
    
    for (i = 0; i < nfd; i++) {
	evt.events   = fds[i].events;
	evt.data.ptr = &fds[i];
	epoll_ctl(sl->epollfd, EPOLL_CTL_ADD, fds[i].fd, &evt);
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
    }

    return min_timeout;
}


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

    if (ext_timeout != -1)
	ml->poll_timeout = MRP_MIN(timeout, ext_timeout);
    else
	ml->poll_timeout = timeout;

    if (ml->nevent < ml->niowatch) {
	ml->nevent = ml->niowatch;
	ml->events = mrp_realloc(ml->events, ml->nevent * sizeof(*ml->events));
	
	MRP_ASSERT(ml->events != NULL, "can't allocate epoll event buffer");
    }

    return TRUE;
}


int mrp_mainloop_poll(mrp_mainloop_t *ml)
{
    int n;
   
    if (ml->nevent > 0) {
	n = epoll_wait(ml->epollfd, ml->events, ml->nevent, ml->poll_timeout);

	if (n < 0 && errno == EINTR)
	    n = 0;
	
	ml->poll_result = n;
    }
    else {
	/*
	 * Notes: Practically we should never branch here because
	 *     we always have at least ml->sigfd registered for epoll.
	 */
	if (ml->poll_timeout > 0)
	    usleep(ml->poll_timeout * USECS_PER_MSEC);
	
	ml->poll_result = 0;
    }
    
    return TRUE;
}


static int poll_subloop(mrp_subloop_t *sl)
{
    struct epoll_event *e;
    struct pollfd      *pfd;
    int                 n, i;

    if (sl->poll) {
	n = epoll_wait(sl->epollfd, sl->events, sl->nevent, 0);
	
	if (n < 0 && errno == EINTR)
	    n = 0;

	for (i = 0, e = sl->events; i < n; i++, e++) {
	    pfd = (struct pollfd *)sl->events[i].data.ptr;

	    pfd->revents = e->events;
	}

	return n;
    }
    else
	return 0;
}


static void dispatch_deferred(mrp_mainloop_t *ml)
{
    mrp_list_hook_t *p, *n;
    mrp_deferred_t  *d;
    
    mrp_list_foreach(&ml->deferred, p, n) {
	d = mrp_list_entry(p, typeof(*d), hook);

	if (!is_deleted(d) && !d->inactive)
	    d->cb(ml, d, d->user_data);
	
	if (is_deleted(d))
	    delete_deferred(d);
	else if (d->inactive)
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
		t->cb(ml, t, t->user_data);
	
		if (!is_deleted(t))
		    rearm_timer(t);
	    }
	    else
		break;
	}
	
	if (is_deleted(t))
	    delete_timer(t);
	
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
			      sl->npollfd))
		sl->cb->dispatch(sl->user_data);
	}
    }
}


static void dispatch_slaves(mrp_io_watch_t *w, struct epoll_event *e)
{
    mrp_mainloop_t  *ml = w->ml;
    mrp_io_watch_t  *s;
    mrp_list_hook_t *p, *n;
    mrp_io_event_t   events;

    events = e->events & ~(MRP_IO_EVENT_INOUT & w->events);
    
    mrp_list_foreach(&w->slave, p, n) {
	if (events == MRP_IO_EVENT_NONE)
	    break;
	
	s = mrp_list_entry(p, typeof(*s), slave);
	
	if (!is_deleted(s))
	    s->cb(ml, s, s->fd, events, s->user_data);
	
	events &= ~(MRP_IO_EVENT_INOUT & s->events);
	
	if (is_deleted(s))
	    delete_io_watch(s);
    }
}


static void dispatch_poll_events(mrp_mainloop_t *ml)
{
    struct epoll_event *e;
    mrp_io_watch_t     *w;
    int                 i;

    for (i = 0, e = ml->events; i < ml->poll_result; i++, e++) {
	w = e->data.ptr;

	if (!is_deleted(w))
	    w->cb(ml, w, w->fd, e->events, w->user_data);
	
	if (!mrp_list_empty(&w->slave))
	    dispatch_slaves(w, e);
	
	if (e->events & EPOLLRDHUP)
	    epoll_ctl(ml->epollfd, EPOLL_CTL_DEL, w->fd, e);

	if (is_deleted(w))
	    delete_io_watch(w);
	
	if (ml->quit)
	    break;
    }

    if (ml->quit)
	return;

    dispatch_subloops(ml);
}


int mrp_mainloop_dispatch(mrp_mainloop_t *ml)
{
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
	mrp_mainloop_poll(ml) && 
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
