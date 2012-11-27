#include <murphy/common.h>
#include <breedline/breedline.h>

typedef struct {
    mrp_io_watch_t  *w;
    void           (*cb)(int fd, int events, void *user_data);
    void            *user_data;
} watch_t;


static void io_cb(mrp_mainloop_t *ml, mrp_io_watch_t *watch, int fd,
                  mrp_io_event_t events, void *user_data)
{
    watch_t *w = (watch_t *)user_data;
    int      e = 0;

    MRP_UNUSED(ml);
    MRP_UNUSED(watch);

    if (events & MRP_IO_EVENT_IN)
        e |= POLLIN;
    if (events & MRP_IO_EVENT_HUP)
        e |= POLLHUP;

    w->cb(fd, e, w->user_data);
}


static void *add_watch(void *mlp, int fd,
                       void (*cb)(int fd, int events, void *user_data),
                       void *user_data)
{
    mrp_mainloop_t *ml     = (mrp_mainloop_t *)mlp;
    mrp_io_event_t  events = MRP_IO_EVENT_IN | MRP_IO_EVENT_HUP;
    watch_t        *w;

    w = mrp_allocz(sizeof(*w));

    if (w != NULL) {
        w->cb        = cb;
        w->user_data = user_data;
        w->w         = mrp_add_io_watch(ml, fd, events, io_cb, w);

        if (w->w != NULL)
            return w;
        else
            mrp_free(w);
    }

    return NULL;
}


static void del_watch(void *wp)
{
    watch_t *w = (watch_t *)wp;

    if (w != NULL) {
        mrp_del_io_watch(w->w);
        mrp_free(w);
    }
}


static brl_allocator_t allocator = {
    .allocfn   = mrp_mm_alloc,
    .reallocfn = mrp_mm_realloc,
    .strdupfn  = mrp_mm_strdup,
    .freefn    = mrp_mm_free
};


static brl_mainloop_ops_t ml_ops = {
    .add_watch = add_watch,
    .del_watch = del_watch
};


brl_t *brl_create_with_murphy(int fd, const char *prompt, mrp_mainloop_t *ml,
                              brl_line_cb_t cb, void *user_data)
{
    brl_t *brl;

    brl_set_allocator(&allocator);

    brl = brl_create(fd, prompt);

    if (brl != NULL) {
        if (brl_use_mainloop(brl, ml, &ml_ops, cb, user_data) == 0)
            return brl;
        else
            brl_destroy(brl);
    }

    return NULL;
}
