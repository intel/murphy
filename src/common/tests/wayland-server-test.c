#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <weston/compositor.h>

#include "murphy/common/wayland-server-glue.h"


#define INTERVALS { 1500, 4500, 9000 }
#define NINTERVAL 3


uint32_t timeval_diff(struct timeval *prev, struct timeval *next)
{
    uint32_t msec;

    msec  = (next->tv_sec  - prev->tv_sec ) * 1000;
    msec += (next->tv_usec - prev->tv_usec) / 1000;

    return msec;
}


static void timer_cb(mrp_timer_t *t, void *data)
{
    static int            intervals[]      = INTERVALS;
    static struct timeval prevs[NINTERVAL] = { [0 ... NINTERVAL-1] { 0, 0 } };
    struct timeval        nexts[NINTERVAL];
    int                   idx  = (int)(ptrdiff_t)data;
    struct timeval       *prev = prevs + idx;
    struct timeval       *next = nexts + idx;

    MRP_UNUSED(t);

    gettimeofday(next, NULL);

    if (prev->tv_sec != 0) {
        printf("timer@%d expired (%d.%d, diff %u msecs)\n", intervals[idx],
               (int)next->tv_sec, (int)next->tv_usec,
               timeval_diff(prev, next));
    }
    else
        printf("timer@%d expired (first expiration)\n", intervals[idx]);

    *prev = *next;
}


WL_EXPORT int
module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
    struct wl_event_loop *wl;
    mrp_mainloop_t       *ml;
    int                   intervals[] = INTERVALS;
    int                   ninterval   = NINTERVAL;
    int                   i;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    wl = wl_display_get_event_loop(compositor->wl_display);
    ml = mrp_mainloop_wayland_get(wl);

    if (ml == NULL) {
        fprintf(stderr, "failed to create wayland-pumped mainloop");
        return -1;
    }

    for (i = 0; i < ninterval; i++)
        mrp_add_timer(ml, intervals[i], timer_cb, (void *)(ptrdiff_t)i);

    return 0;
}
