#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/mainloop.h>

#include "sd-bus.h"
#include "bus-message.h"

#define USEC_TO_MSEC(usec) ((unsigned int)((usec) / 1000))

typedef struct {
    sd_bus         *bus;
    mrp_mainloop_t *ml;
    mrp_subloop_t  *sl;
} bus_t;

static void signal_handler(mrp_sighandler_t *h, int signum, void *user_data)
{
    MRP_UNUSED(user_data);

    switch (signum) {
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
        mrp_log_info("Received signal %d (%s), exiting...", signum,
                     strsignal(signum));
        mrp_mainloop_quit(mrp_get_sighandler_mainloop(h), 0);
    }
}


static int bus_prepare(void *user_data)
{
    MRP_UNUSED(user_data);

    return FALSE;
}


static int bus_query(void *user_data, struct pollfd *fds, int nfd,
                     int *timeout)
{
    bus_t    *b = (bus_t *)user_data;
    uint64_t  usec;

    mrp_log_info("nfd: %d", nfd);

    if (nfd > 0) {
        fds[0].fd      = sd_bus_get_fd(b->bus);
        fds[0].events  = sd_bus_get_events(b->bus) | POLLIN;
        fds[0].revents = 0;

        if (sd_bus_get_timeout(b->bus, &usec) < 0)
            *timeout = -1;
        else
            *timeout = USEC_TO_MSEC(usec);

        mrp_log_info("fd: %d, events: 0x%x, timeout: %u", fds[0].fd,
                     fds[0].events, *timeout);
    }

    return 1;
}


static int bus_check(void *user_data, struct pollfd *fds, int nfd)
{
    MRP_UNUSED(user_data);

    if (nfd > 0 && fds[0].revents != 0)
        return TRUE;
    else
        return FALSE;
}


static void bus_dispatch(void *user_data)
{
    bus_t *b = (bus_t *)user_data;

    if (sd_bus_process(b->bus, NULL) > 0)
        sd_bus_flush(b->bus);
}


static int bus_signal_cb(sd_bus *bus, int ret, sd_bus_message *m, void *user_data)
{
    mrp_log_info("%s(): got bus signal...", __FUNCTION__);

    bus_message_dump(m);

    return 0;
}


static int bus_method_cb(sd_bus *bus, int ret, sd_bus_message *m, void *user_data)
{
    mrp_log_info("%s(): got bus method call message %p...", __FUNCTION__, m);

    bus_message_dump(m);

    if (!strcmp(sd_bus_message_get_member(m), "unhandled"))
        return FALSE;
    else
        return TRUE;
}


static int bus_return_cb(sd_bus *bus, int ret, sd_bus_message *m, void *user_data)
{
    mrp_log_info("%s(): got bus method reply...", __FUNCTION__);

    bus_message_dump(m);

    return 0;
}


static void emit_signal(mrp_timer_t *t, void *user_data)
{
    sd_bus *bus = (sd_bus *)user_data;

    sd_bus_emit_signal(bus, "/foo/bar", "foo.bar", "foobar", NULL);
}


static void call_method(mrp_timer_t *t, void *user_data)
{
    sd_bus         *bus = (sd_bus *)user_data;
    sd_bus_message *msg = NULL;
    int             r;
    uint64_t        serial;

    r = sd_bus_message_new_method_call(bus, "org.freedesktop.DBus",
                                       "/", "org.freedesktop.DBus", "GetId",
                                       &msg);

    if (r != 0) {
        mrp_log_error("Failed to create new method call message.");
        return;
    }

    r = sd_bus_send_with_reply(bus, msg, bus_return_cb, NULL, 100000 * 1000, &serial);

    if (r != 0)
        mrp_log_error("Failed to call method... (r = %d)", r);
}


int main(int argc, char *argv[])
{
    static mrp_subloop_ops_t bus_ops = {
        .prepare  = bus_prepare,
        .query    = bus_query,
        .check    = bus_check,
        .dispatch = bus_dispatch
    };

    mrp_mainloop_t *ml  = NULL;
    mrp_timer_t    *ts  = NULL;
    mrp_timer_t    *tm  = NULL;
    sd_bus         *bus = NULL;
    int             r;
    bus_t          *b;

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_INFO));

    ml = mrp_mainloop_create();
    r  = sd_bus_open_user(&bus);

    if (ml == NULL || r != 0)
        goto fail;

    mrp_add_sighandler(ml, SIGINT , signal_handler, NULL);
    mrp_add_sighandler(ml, SIGTERM, signal_handler, NULL);
    mrp_add_sighandler(ml, SIGQUIT, signal_handler, NULL);

    b = mrp_allocz(sizeof(*b));

    if (b == NULL)
        goto fail;

    sd_bus_add_match(bus, "type='signal'"       , bus_signal_cb, bus);
#if 0
    sd_bus_add_match(bus, "type='method_call'"  , bus_method_cb, bus);
    sd_bus_add_match(bus, "type='method_return'", bus_return_cb, bus);
#else
    sd_bus_add_fallback(bus, "/", bus_method_cb, bus);
#endif

    while (sd_bus_process(bus, NULL) > 0)
        sd_bus_flush(bus);

    b->bus = bus;
    b->ml  = ml;
    b->sl  = mrp_add_subloop(ml, &bus_ops, b);

    if (b->sl == NULL) {
        mrp_log_error("Failed to register D-Bus subloop.");
        exit(1);
    }

#if 0
    if ((ts = mrp_add_timer(ml, 1000, emit_signal, bus)) == NULL) {
        mrp_log_error("Failed to create signal emission timer.");
        exit(1);
    }
#endif

    if ((ts = mrp_add_timer(ml, 1000, call_method, bus)) == NULL) {
        mrp_log_error("Failed to create method call timer.");
        exit(1);
    }

    mrp_mainloop_run(ml);

 fail:
    mrp_del_timer(ts);
    mrp_del_timer(tm);

    sd_bus_unref(bus);
    mrp_mainloop_destroy(ml);

    return 0;
}
