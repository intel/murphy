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

#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/refcnt.h>
#include <murphy/common/utils.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/dbus-sdbus.h>

#define BUS_SERVICE      "org.freedesktop.DBus"
#define BUS_PATH         "/org/freedesktop/DBus"
#define BUS_INTERFACE    "org.freedesktop.DBus"
#define BUS_NAME_CHANGED "NameOwnerChanged"
#define BUS_GET_OWNER    "GetNameOwner"

/* XXX check these... */
#define SDBUS_ERROR_FAILED   "org.DBus.error.failed"

/* D-Bus name request flags and reply statuses. */
#define SDBUS_NAME_REQUEST_REPLACE 0x2
#define SDBUS_NAME_REQUEST_DONTQ   0x4

#define SDBUS_NAME_STATUS_OWNER    0x1
#define SDBUS_NAME_STATUS_QUEUING  0x2
#define SDBUS_NAME_STATUS_EXISTS   0x3
#define SDBUS_NAME_STATUS_GOTIT    0x4

#define SDBUS_NAME_STATUS_RELEASED 0x1
#define SDBUS_NAME_STATUS_UNKNOWN  0x2
#define SDBUS_NAME_STATUS_FOREIGN  0x3

#define USEC_TO_MSEC(usec) ((unsigned int)((usec) / 1000))
#define MSEC_TO_USEC(msec) ((uint64_t)(msec) * 1000)

struct mrp_dbus_s {
    char            *address;            /* bus address */
    sd_bus          *bus;                /* actual D-BUS connection */
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
    mrp_subloop_t   *sl;                 /* subloop for pumping the bus */
    mrp_htbl_t      *objects;            /* object path (refcount) table */
    mrp_htbl_t      *methods;            /* method handler table */
    mrp_htbl_t      *signals;            /* signal handler table */
    mrp_list_hook_t  name_trackers;      /* peer (name) watchers */
    mrp_list_hook_t  calls;              /* pending calls */
    uint32_t         call_id;            /* next call id */
    const char      *unique_name;        /* our unique D-BUS address */
    int              priv;               /* whether a private connection */
    int              signal_filter;      /* if signal dispatching is set up */
    int              register_fallback;  /* if the fallback object is set up */
    mrp_refcnt_t     refcnt;             /* reference count */
};


struct mrp_dbus_msg_s {
    sd_bus_message  *msg;                /* actual D-Bus message */
    mrp_refcnt_t     refcnt;             /* reference count */
    mrp_list_hook_t  arrays;             /* implicitly freed related arrays */
};


typedef struct {
    mrp_dbus_type_t   type;
    mrp_list_hook_t   hook;
    void             *items;
    size_t            nitem;
} msg_array_t;

/*
 * Notes:
 *
 * At the moment we administer DBUS method and signal handlers
 * in a very primitive way (subject to be changed later). For
 * every bus instance we maintain two hash tables, one for methods
 * and another for signals. Each method and signal handler is
 * hashed in only by it's method/signal name to a linked list of
 * method or signal handlers.
 *
 * When dispatching a method, we look up the chain with a matching
 * method name, or the chain for "" in case a matching chain is
 * not found, and invoke the handler which best matches the
 * received message (by looking at the path, interface and name).
 * Only one such handler is invoked at most.
 *
 * For signals we look up both the chain with a matching name and
 * the chain for "" and invoke all signal handlers that match the
 * received message (regardless of their return value).
 */

typedef struct {
    char *path;                         /* object path */
    int   cnt;                          /* reference count */
} object_t;

typedef struct {
    char            *member;            /* signal/method name */
    mrp_list_hook_t  handlers;          /* handlers with matching member */
} handler_list_t;

typedef struct {
    mrp_list_hook_t     hook;
    char               *sender;
    char               *path;
    char               *interface;
    char               *member;
    mrp_dbus_handler_t  handler;
    void               *user_data;
} handler_t;

#define method_t handler_t
#define signal_t handler_t


typedef struct {
    mrp_list_hook_t     hook;           /* hook to name tracker list */
    char               *name;           /* name to track */
    mrp_dbus_name_cb_t  cb;             /* status change callback */
    void               *user_data;      /* opaque callback user data */
    int32_t             qid;            /* initial query ID */
} name_tracker_t;


typedef struct {
    mrp_dbus_t          *dbus;           /* DBUS connection */
    int32_t              id;             /* call id */
    mrp_dbus_reply_cb_t  cb;             /* completion notification callback */
    void                *user_data;      /* opaque callback data */
    uint64_t             serial;         /* DBUS call */
    mrp_list_hook_t      hook;           /* hook to list of pending calls */
    sd_bus_message      *msg;            /* original message */
} call_t;


typedef struct {
    mrp_mainloop_t *ml;                  /* mainloop for bus connection */
    const char     *address;             /* address of bus */
} bus_spec_t;

static mrp_htbl_t *buses;



static int dispatch_signal(sd_bus *b, int r, sd_bus_message *msg, void *data);
static int dispatch_method(sd_bus *b, int r, sd_bus_message *msg, void *data);

static void purge_name_trackers(mrp_dbus_t *dbus);
static void purge_calls(mrp_dbus_t *dbus);
static void handler_list_free_cb(void *key, void *entry);
static void handler_free(handler_t *h);
static int name_owner_change_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *m,
                                void *data);
static void call_free(call_t *call);
static void object_free_cb(void *key, void *entry);


static int purge_objects(void *key, void *entry, void *user_data)
{
    mrp_dbus_t *dbus = (mrp_dbus_t *)user_data;
    object_t   *o    = (object_t *)entry;

    MRP_UNUSED(key);

    sd_bus_remove_object(dbus->bus, o->path, dispatch_method, dbus);

    return MRP_HTBL_ITER_MORE;
}


static int purge_filters(void *key, void *entry, void *user_data)
{
    mrp_dbus_t      *dbus = (mrp_dbus_t *)user_data;
    handler_list_t  *l    = (handler_list_t *)entry;
    mrp_list_hook_t *p, *n;
    handler_t       *h;

    MRP_UNUSED(key);

    mrp_list_foreach(&l->handlers, p, n) {
        h = mrp_list_entry(p, handler_t, hook);
        mrp_dbus_remove_filter(dbus,
                               h->sender, h->path, h->interface,
                               h->member, NULL);
    }

    return MRP_HTBL_ITER_MORE;
}


static void dbus_disconnect(mrp_dbus_t *dbus)
{
    if (dbus) {
        mrp_htbl_remove(buses, dbus->bus, FALSE);

        if (dbus->objects) {
            mrp_htbl_foreach(dbus->signals, purge_objects, dbus);
            mrp_htbl_destroy(dbus->objects, TRUE);
        }

        if (dbus->signals) {
            mrp_htbl_foreach(dbus->signals, purge_filters, dbus);
            mrp_htbl_destroy(dbus->signals, TRUE);
        }
        if (dbus->methods)
            mrp_htbl_destroy(dbus->methods, TRUE);

        purge_name_trackers(dbus);
        purge_calls(dbus);

        if (dbus->bus != NULL) {
            if (dbus->signal_filter)
                sd_bus_remove_filter(dbus->bus, dispatch_signal, dbus);
            if (dbus->register_fallback)
                sd_bus_remove_fallback(dbus->bus, "/", dispatch_method, dbus);
            if (dbus->priv)
                sd_bus_close(dbus->bus);
            else
                sd_bus_unref(dbus->bus);
        }

        mrp_free(dbus->address);
        dbus->bus = NULL;
        dbus->ml  = NULL;

        mrp_free(dbus);
    }
}


static int bus_cmp(const void *key1, const void *key2)
{
    return key2 - key1;
}


static uint32_t bus_hash(const void *key)
{
    uint32_t h;

    h   = (ptrdiff_t)key;
    h >>= 2 * sizeof(key);

    return h;
}


static int find_bus_by_spec(void *key, void *object, void *user_data)
{
    mrp_dbus_t *dbus = (mrp_dbus_t *)object;
    bus_spec_t *spec = (bus_spec_t *)user_data;

    MRP_UNUSED(key);

    if (dbus->ml == spec->ml && !strcmp(dbus->address, spec->address))
        return TRUE;
    else
        return FALSE;
}


static mrp_dbus_t *dbus_get(mrp_mainloop_t *ml, const char *address)
{
    mrp_htbl_config_t hcfg;
    bus_spec_t        spec;

    if (buses == NULL) {
        mrp_clear(&hcfg);

        hcfg.comp = bus_cmp;
        hcfg.hash = bus_hash;
        hcfg.free = NULL;

        buses = mrp_htbl_create(&hcfg);

        return NULL;
    }
    else {
        spec.ml      = ml;
        spec.address = address;

        return mrp_htbl_find(buses, find_bus_by_spec, &spec);
    }
}


mrp_dbus_t *mrp_dbus_connect(mrp_mainloop_t *ml, const char *address,
                             mrp_dbus_err_t *errp)
{
    mrp_htbl_config_t  hcfg;
    mrp_dbus_t        *dbus;

    if ((dbus = dbus_get(ml, address)) != NULL)
        return mrp_dbus_ref(dbus);

    if ((dbus = mrp_allocz(sizeof(*dbus))) == NULL)
        return NULL;

    mrp_list_init(&dbus->calls);
    mrp_list_init(&dbus->name_trackers);
    mrp_refcnt_init(&dbus->refcnt);

    dbus->ml = ml;

    mrp_dbus_error_init(errp);

    /*
     * connect to the bus
     */

    if (!strcmp(address, "system")) {
        if (sd_bus_open_system(&dbus->bus) != 0)
            goto fail;
    }
    else if (!strcmp(address, "session")) {
        if (sd_bus_open_user(&dbus->bus) != 0)
            goto fail;
    }
    else {
        dbus->priv = TRUE;

        if (sd_bus_new(&dbus->bus) != 0)
            goto fail;
        else {
            if (sd_bus_set_address(dbus->bus, address) != 0)
                goto fail;

            if (sd_bus_start(dbus->bus) != 0)
                goto fail;
        }
    }

    dbus->address = mrp_strdup(address);
    if (sd_bus_get_unique_name(dbus->bus, &dbus->unique_name) != 0)
        goto fail;

    /*
     * set up with mainloop
     */

    if (!mrp_dbus_setup_with_mainloop(ml, dbus->bus))
        goto fail;

    /*
     * set up our message dispatchers and take our name on the bus
     */

    if (sd_bus_add_filter(dbus->bus, dispatch_signal, dbus) != 0) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                         "Failed to set up signal dispatching.");
        goto fail;
    }
    dbus->signal_filter = TRUE;

    if (sd_bus_add_fallback(dbus->bus, "/", dispatch_method, dbus) != 0) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                           "Failed to set up method dispatching.");
        goto fail;
    }
    dbus->register_fallback = TRUE;

    mrp_clear(&hcfg);
    hcfg.comp = mrp_string_comp;
    hcfg.hash = mrp_string_hash;
    hcfg.free = object_free_cb;

    if ((dbus->objects = mrp_htbl_create(&hcfg)) == NULL) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                           "Failed to create DBUS object path table.");
        goto fail;
    }

    hcfg.free = handler_list_free_cb;

    if ((dbus->methods = mrp_htbl_create(&hcfg)) == NULL) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                           "Failed to create DBUS method table.");
        goto fail;
    }

    if ((dbus->signals = mrp_htbl_create(&hcfg)) == NULL) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                           "Failed to create DBUS signal table.");
        goto fail;
    }


    /*
     * install handler for NameOwnerChanged for tracking clients/peers
     */

    if (!mrp_dbus_add_signal_handler(dbus,
                                     BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                                     BUS_NAME_CHANGED, name_owner_change_cb,
                                     NULL)) {
        mrp_dbus_error_set(errp, SDBUS_ERROR_FAILED,
                           "Failed to install NameOwnerChanged handler.");
        goto fail;
    }

    /* install a 'safe' filter to avoid receiving all name change signals */
    mrp_dbus_install_filter(dbus,
                            BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                            BUS_NAME_CHANGED, BUS_SERVICE, NULL);

    mrp_list_init(&dbus->name_trackers);
    dbus->call_id = 1;

    if (mrp_htbl_insert(buses, dbus->bus, dbus))
        return dbus;

 fail:
    dbus_disconnect(dbus);
    return NULL;
}


mrp_dbus_t *mrp_dbus_ref(mrp_dbus_t *dbus)
{
    return mrp_ref_obj(dbus, refcnt);
}


int mrp_dbus_unref(mrp_dbus_t *dbus)
{
    if (mrp_unref_obj(dbus, refcnt)) {
        dbus_disconnect(dbus);

        return TRUE;
    }
    else
        return FALSE;
}


int mrp_dbus_acquire_name(mrp_dbus_t *dbus, const char *name,
                          mrp_dbus_err_t *error)
{
    int flags = SDBUS_NAME_REQUEST_REPLACE | SDBUS_NAME_REQUEST_DONTQ;
    int status;

    mrp_dbus_error_init(error);

    status = sd_bus_request_name(dbus->bus, name, flags);

    if (status == SDBUS_NAME_STATUS_OWNER || status == SDBUS_NAME_STATUS_GOTIT)
        return TRUE;
    else {
        mrp_dbus_error_set(error, SDBUS_ERROR_FAILED, "failed to request name");

        return FALSE;
    }
}


int mrp_dbus_release_name(mrp_dbus_t *dbus, const char *name,
                          mrp_dbus_err_t *error)
{
    int status;

    mrp_dbus_error_init(error);

    status = sd_bus_release_name(dbus->bus, name);

    if (status == SDBUS_NAME_STATUS_RELEASED)
        return TRUE;
    else {
        mrp_dbus_error_set(error, SDBUS_ERROR_FAILED, "failed to release name");

        return FALSE;
    }
}


const char *mrp_dbus_get_unique_name(mrp_dbus_t *dbus)
{
    return dbus->unique_name;
}


static void name_owner_query_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *m, void *data)
{
    name_tracker_t *t = (name_tracker_t *)data;
    const char     *owner;
    int             state;

    if (t->cb != NULL) {                /* tracker still active */
        t->qid = 0;
        state  = !mrp_dbus_msg_is_error(m);

        if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_STRING, &owner))
            owner = "<unknown>";

        t->cb(dbus, t->name, state, owner, t->user_data);
    }
    else                                /* already requested to delete */
        mrp_free(t);
}


static int name_owner_change_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *m, void *data)
{
    const char      *name, *prev, *next;
    mrp_list_hook_t *p, *n;
    name_tracker_t  *t;

    MRP_UNUSED(data);

    if (mrp_dbus_msg_type(m) != MRP_DBUS_MESSAGE_TYPE_SIGNAL)
        return FALSE;

    if (!mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_STRING, &name) ||
        !mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_STRING, &prev) ||
        !mrp_dbus_msg_read_basic(m, MRP_DBUS_TYPE_STRING, &next))
        return FALSE;

#if 0
    /*
     * Notes: XXX TODO
     *    In principle t->cb could call mrp_dbus_forget for some other D-BUS
     *    address than name. If that happened to be n (== p->hook.next) this
     *    would result in a crash or memory corruption in the next iteration
     *    of this loop (when handling n). We can easily get around this
     *    problem by
     *
     *     1. administering in mrp_dbus_t that we're handing a NameOwnerChange
     *     2. checking for this in mrp_dbus_forget_name and if it is the case
     *        only marking the affected entry for deletion
     *     3. removing entries marked for deletion in this loop (or just
     *        ignoring them and making another pass in the end removing any
     *        such entry).
     */
#endif

    mrp_list_foreach(&dbus->name_trackers, p, n) {
        t = mrp_list_entry(p, name_tracker_t, hook);

        if (!strcmp(name, t->name))
            t->cb(dbus, name, next && *next, next, t->user_data);
    }

    return TRUE;
}


int mrp_dbus_follow_name(mrp_dbus_t *dbus, const char *name,
                         mrp_dbus_name_cb_t cb, void *user_data)
{
    name_tracker_t *t;

    if ((t = mrp_allocz(sizeof(*t))) != NULL) {
        if ((t->name = mrp_strdup(name)) != NULL) {
            t->cb        = cb;
            t->user_data = user_data;

            if (mrp_dbus_install_filter(dbus,
                                        BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                                        BUS_NAME_CHANGED, name,
                                        NULL)) {
                mrp_list_append(&dbus->name_trackers, &t->hook);

                t->qid = mrp_dbus_call(dbus,
                                       BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                                       BUS_GET_OWNER, 5000,
                                       name_owner_query_cb, t,
                                       MRP_DBUS_TYPE_STRING, t->name,
                                       MRP_DBUS_TYPE_INVALID);
                return TRUE;
            }
            else {
                mrp_free(t->name);
                mrp_free(t);
            }
        }
    }

    return FALSE;
}


int mrp_dbus_forget_name(mrp_dbus_t *dbus, const char *name,
                         mrp_dbus_name_cb_t cb, void *user_data)
{
    mrp_list_hook_t *p, *n;
    name_tracker_t  *t;

    mrp_dbus_remove_filter(dbus,
                           BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                           BUS_NAME_CHANGED, name,
                           NULL);

    mrp_list_foreach(&dbus->name_trackers, p, n) {
        t = mrp_list_entry(p, name_tracker_t, hook);

        if (t->cb == cb && t->user_data == user_data && !strcmp(t->name,name)) {
            mrp_list_delete(&t->hook);
            mrp_free(t->name);

            if (!t->qid)
                mrp_free(t);
            else {
                t->cb        = NULL;
                t->user_data = NULL;
                t->name      = NULL;
            }

            return TRUE;
        }
    }

    return FALSE;
}


static void purge_name_trackers(mrp_dbus_t *dbus)
{
    mrp_list_hook_t *p, *n;
    name_tracker_t  *t;

    mrp_list_foreach(&dbus->name_trackers, p, n) {
        t = mrp_list_entry(p, name_tracker_t, hook);

        mrp_list_delete(p);
        mrp_dbus_remove_filter(dbus,
                               BUS_SERVICE, BUS_PATH, BUS_INTERFACE,
                               BUS_NAME_CHANGED, t->name,
                               NULL);
        mrp_free(t->name);
        mrp_free(t);
    }
}


static handler_t *handler_alloc(const char *sender, const char *path,
                                const char *interface, const char *member,
                                mrp_dbus_handler_t handler, void *user_data)
{
    handler_t *h;

    if ((h = mrp_allocz(sizeof(*h))) != NULL) {
        h->sender    = mrp_strdup(sender);
        h->path      = mrp_strdup(path);
        h->interface = mrp_strdup(interface);
        h->member    = mrp_strdup(member);

        if ((path && !h->path) || !h->interface || !h->member) {
            handler_free(h);
            return NULL;
        }

        h->handler   = handler;
        h->user_data = user_data;

        return h;
    }

    return NULL;
}


static void handler_free(handler_t *h)
{
    if (h != NULL) {
        mrp_free(h->sender);
        mrp_free(h->path);
        mrp_free(h->interface);
        mrp_free(h->member);

        mrp_free(h);
    }
}


static handler_list_t *handler_list_alloc(const char *member)
{
    handler_list_t *l;

    if ((l = mrp_allocz(sizeof(*l))) != NULL) {
        if ((l->member = mrp_strdup(member)) != NULL)
            mrp_list_init(&l->handlers);
        else {
            mrp_free(l);
            l = NULL;
        }
    }

    return l;
}


static inline void handler_list_free(handler_list_t *l)
{
    mrp_list_hook_t *p, *n;
    handler_t       *h;

    mrp_list_foreach(&l->handlers, p, n) {
        h = mrp_list_entry(p, handler_t, hook);
        mrp_list_delete(p);
        handler_free(h);
    }

    mrp_free(l->member);
    mrp_free(l);
}


static void handler_list_free_cb(void *key, void *entry)
{
    MRP_UNUSED(key);

    handler_list_free((handler_list_t *)entry);
}


static inline int handler_specificity(handler_t *h)
{
    int score = 0;

    if (h->path && *h->path)
        score |= 0x4;
    if (h->interface && *h->interface)
        score |= 0x2;
    if (h->member && *h->member)
        score |= 0x1;

    return score;
}


static void handler_list_insert(handler_list_t *l, handler_t *handler)
{
    mrp_list_hook_t *p, *n;
    handler_t       *h;
    int              score;

    score = handler_specificity(handler);

    mrp_list_foreach(&l->handlers, p, n) {
        h = mrp_list_entry(p, handler_t, hook);

        if (score >= handler_specificity(h)) {
            mrp_list_append(h->hook.prev, &handler->hook);  /* add before h */
            return;
        }
    }

    mrp_list_append(&l->handlers, &handler->hook);
}


static handler_t *handler_list_lookup(handler_list_t *l, const char *path,
                                      const char *interface, const char *member,
                                      mrp_dbus_handler_t handler,
                                      void *user_data)
{
    mrp_list_hook_t *p, *n;
    handler_t       *h;

    mrp_list_foreach(&l->handlers, p, n) {
        h = mrp_list_entry(p, handler_t, hook);

        if (h->handler == handler && user_data == h->user_data &&
            path      && !strcmp(path, h->path) &&
            interface && !strcmp(interface, h->interface) &&
            member    && !strcmp(member, h->member))
            return h;
    }

    return NULL;
}


static handler_t *handler_list_find(handler_list_t *l, const char *path,
                                    const char *interface, const char *member)
{
#define MATCHES(h, field) (!*field || !*h->field || !strcmp(field, h->field))
    mrp_list_hook_t *p, *n;
    handler_t       *h;

    mrp_list_foreach(&l->handlers, p, n) {
        h = mrp_list_entry(p, handler_t, hook);

        if (MATCHES(h, path) && MATCHES(h, interface) && MATCHES(h, member))
            return h;
    }

    return NULL;
#undef MATCHES
}


static void object_free_cb(void *key, void *entry)
{
    object_t *o = (object_t *)entry;

    MRP_UNUSED(key);

    mrp_free(o->path);
    mrp_free(o);
}


static object_t *object_add(mrp_dbus_t *dbus, const char *path)
{
    object_t *o;

    o = mrp_alloc(sizeof(*o));

    if (o != NULL) {
        o->path = mrp_strdup(path);
        o->cnt  = 1;

        if (o->path == NULL) {
            mrp_free(o);
            return NULL;
        }

        if (sd_bus_add_object(dbus->bus, o->path, dispatch_method, dbus) == 0) {
            if (mrp_htbl_insert(dbus->objects, o->path, o))
                return o;
            else
                sd_bus_remove_object(dbus->bus, o->path, dispatch_method, dbus);
        }

        mrp_free(o->path);
        mrp_free(o);
    }

    return NULL;
}


static object_t *object_lookup(mrp_dbus_t *dbus, const char *path)
{
    return mrp_htbl_lookup(dbus->objects, (void *)path);
}


static int object_ref(mrp_dbus_t *dbus, const char *path)
{
    object_t *o;

    if ((o = object_lookup(dbus, path)) != NULL) {
        o->cnt++;
        return TRUE;
    }

    if (object_add(dbus, path) != NULL)
        return TRUE;
    else
        return FALSE;
}


static void object_unref(mrp_dbus_t *dbus, const char *path)
{
    object_t *o;

    if ((o = object_lookup(dbus, path)) != NULL) {
        o->cnt--;

        if (o->cnt <= 0) {
            mrp_htbl_remove(dbus->objects, (void *)path, FALSE);
            sd_bus_remove_object(dbus->bus, o->path, dispatch_method, dbus);

            mrp_free(o->path);
            mrp_free(o);
        }
    }
}


int mrp_dbus_export_method(mrp_dbus_t *dbus, const char *path,
                           const char *interface, const char *member,
                           mrp_dbus_handler_t handler, void *user_data)
{
    handler_list_t *methods;
    handler_t      *m;

    if (!object_ref(dbus, path))
        return FALSE;

    if ((methods = mrp_htbl_lookup(dbus->methods, (void *)member)) == NULL) {
        if ((methods = handler_list_alloc(member)) == NULL)
            goto fail;

        mrp_htbl_insert(dbus->methods, methods->member, methods);
    }

    m = handler_alloc(NULL, path, interface, member, handler, user_data);

    if (m != NULL) {
        handler_list_insert(methods, m);

        return TRUE;
    }

 fail:
    object_unref(dbus, path);

    return FALSE;
}


int mrp_dbus_remove_method(mrp_dbus_t *dbus, const char *path,
                           const char *interface, const char *member,
                           mrp_dbus_handler_t handler, void *user_data)
{
    handler_list_t *methods;
    handler_t      *m;

    if ((methods = mrp_htbl_lookup(dbus->methods, (void *)member)) == NULL)
        return FALSE;

    m = handler_list_lookup(methods, path, interface, member,
                            handler, user_data);
    if (m != NULL) {
        object_unref(dbus, path);
        mrp_list_delete(&m->hook);
        handler_free(m);

        return TRUE;
    }
    else
        return FALSE;
}


int mrp_dbus_add_signal_handler(mrp_dbus_t *dbus, const char *sender,
                                const char *path, const char *interface,
                                const char *member, mrp_dbus_handler_t handler,
                                void *user_data)
{
    handler_list_t *signals;
    handler_t      *s;

    if ((signals = mrp_htbl_lookup(dbus->signals, (void *)member)) == NULL) {
        if ((signals = handler_list_alloc(member)) == NULL)
            return FALSE;

        if (!mrp_htbl_insert(dbus->signals, signals->member, signals)) {
            handler_list_free(signals);
            return FALSE;
        }
    }

    s = handler_alloc(sender, path, interface, member, handler, user_data);
    if (s != NULL) {
        handler_list_insert(signals, s);
        return TRUE;
    }
    else {
        handler_free(s);
        if (mrp_list_empty(&signals->handlers))
            mrp_htbl_remove(dbus->signals, signals->member, TRUE);
        return FALSE;
    }
}



int mrp_dbus_del_signal_handler(mrp_dbus_t *dbus, const char *sender,
                                const char *path, const char *interface,
                                const char *member, mrp_dbus_handler_t handler,
                                void *user_data)
{
    handler_list_t *signals;
    handler_t      *s;

    MRP_UNUSED(sender);

    if ((signals = mrp_htbl_lookup(dbus->signals, (void *)member)) == NULL)
        return FALSE;

    s = handler_list_lookup(signals, path, interface, member,
                            handler, user_data);
    if (s != NULL) {
        mrp_list_delete(&s->hook);
        handler_free(s);

        if (mrp_list_empty(&signals->handlers))
            mrp_htbl_remove(dbus->signals, (void *)member, TRUE);

        return TRUE;
    }
    else
        return FALSE;
}



int mrp_dbus_subscribe_signal(mrp_dbus_t *dbus,
                              mrp_dbus_handler_t handler, void *user_data,
                              const char *sender, const char *path,
                              const char *interface, const char *member, ...)
{
    va_list ap;
    int     success;


    if (mrp_dbus_add_signal_handler(dbus, sender, path, interface, member,
                                    handler, user_data)) {
        va_start(ap, member);
        success = mrp_dbus_install_filterv(dbus,
                                           sender, path, interface, member, ap);
        va_end(ap);

        if (success)
            return TRUE;
        else
            mrp_dbus_del_signal_handler(dbus, sender, path, interface, member,
                                        handler, user_data);
    }

    return FALSE;
}


int mrp_dbus_unsubscribe_signal(mrp_dbus_t *dbus,
                                mrp_dbus_handler_t handler, void *user_data,
                                const char *sender, const char *path,
                                const char *interface, const char *member, ...)
{
    va_list ap;
    int     status;

    status = mrp_dbus_del_signal_handler(dbus, sender, path, interface, member,
                                         handler, user_data);
    va_start(ap, member);
    status &= mrp_dbus_remove_filterv(dbus,
                                      sender, path, interface, member, ap);
    va_end(ap);

    return status;
}


int mrp_dbus_install_filterv(mrp_dbus_t *dbus, const char *sender,
                             const char *path, const char *interface,
                             const char *member, va_list args)
{
#define ADD_TAG(tag, value) do {                                          \
        if (value != NULL) {                                              \
            l = snprintf(p, n, "%s%s='%s'", p == filter ? "" : ",",       \
                         tag, value);                                     \
            if (l >= n)                                                   \
                return FALSE;                                             \
            n -= l;                                                       \
            p += l;                                                       \
        }                                                                 \
    } while (0)

    va_list   ap;
    char      filter[1024], *p, argn[16], *val;
    int       n, l, i;

    p = filter;
    n = sizeof(filter);

    ADD_TAG("type"     , "signal");
    ADD_TAG("sender"   ,  sender);
    ADD_TAG("path"     ,  path);
    ADD_TAG("interface",  interface);
    ADD_TAG("member"   ,  member);

    va_copy(ap, args);
    i = 0;
    while ((val = va_arg(ap, char *)) != NULL) {
        snprintf(argn, sizeof(argn), "arg%d", i);
        ADD_TAG(argn, val);
        i++;
    }
    va_end(ap);

    if (sd_bus_add_match(dbus->bus, filter, NULL, NULL) != 0) {
        mrp_log_error("Failed to install filter '%s'.", filter);

        return FALSE;
    }
    else
        return TRUE;

}


int mrp_dbus_install_filter(mrp_dbus_t *dbus, const char *sender,
                            const char *path, const char *interface,
                            const char *member, ...)
{
    va_list ap;
    int     status;

    va_start(ap, member);
    status = mrp_dbus_install_filterv(dbus,
                                      sender, path, interface, member, ap);
    va_end(ap);

    return status;
}


int mrp_dbus_remove_filterv(mrp_dbus_t *dbus, const char *sender,
                            const char *path, const char *interface,
                            const char *member, va_list args)
{
    va_list ap;
    char    filter[1024], *p, argn[16], *val;
    int     n, l, i;

    p = filter;
    n = sizeof(filter);

    ADD_TAG("type"     , "signal");
    ADD_TAG("sender"   ,  sender);
    ADD_TAG("path"     ,  path);
    ADD_TAG("interface",  interface);
    ADD_TAG("member"   ,  member);

    va_copy(ap, args);
    i = 0;
    while ((val = va_arg(ap, char *)) != NULL) {
        snprintf(argn, sizeof(argn), "arg%d", i);
        ADD_TAG(argn, val);
        i++;
    }
    va_end(ap);

    sd_bus_remove_match(dbus->bus, filter, NULL, NULL);

    return TRUE;
#undef ADD_TAG
}


int mrp_dbus_remove_filter(mrp_dbus_t *dbus, const char *sender,
                           const char *path, const char *interface,
                           const char *member, ...)
{
    va_list ap;
    int     status;

    va_start(ap, member);
    status = mrp_dbus_remove_filterv(dbus, sender, path, interface, member, ap);
    va_end(ap);

    return status;
}


static int element_size(mrp_dbus_type_t type)
{
    switch (type) {
    case MRP_DBUS_TYPE_BYTE:        return sizeof(char);
    case MRP_DBUS_TYPE_BOOLEAN:     return sizeof(uint32_t);
    case MRP_DBUS_TYPE_INT16:
    case MRP_DBUS_TYPE_UINT16:      return sizeof(uint16_t);
    case MRP_DBUS_TYPE_INT32:
    case MRP_DBUS_TYPE_UINT32:      return sizeof(uint32_t);
    case MRP_DBUS_TYPE_INT64:
    case MRP_DBUS_TYPE_UINT64:      return sizeof(uint64_t);
    case MRP_DBUS_TYPE_DOUBLE:      return sizeof(double);
    case MRP_DBUS_TYPE_STRING:      return sizeof(char *);
    case MRP_DBUS_TYPE_OBJECT_PATH: return sizeof(char *);
    case MRP_DBUS_TYPE_SIGNATURE:   return sizeof(char *);
    default:
        return FALSE;
    }

}


static inline mrp_dbus_msg_t *create_message(sd_bus_message *msg, int ref)
{
    mrp_dbus_msg_t *m;

    if (msg != NULL) {
        if ((m = mrp_allocz(sizeof(*m))) != NULL) {
            mrp_refcnt_init(&m->refcnt);
            mrp_list_init(&m->arrays);
            if (ref)
                m->msg = sd_bus_message_ref(msg);
            else
                m->msg = msg;
        }

        return m;
    }
    else
        return NULL;
}


static void free_msg_array(msg_array_t *a)
{
    void   *ptr;
    size_t  esize, i;
    int     string;

    if (a == NULL)
        return;

    mrp_list_delete(&a->hook);

    if ((esize = element_size(a->type)) != 0) {
        if (a->type == MRP_DBUS_TYPE_STRING ||
            a->type == MRP_DBUS_TYPE_OBJECT_PATH ||
            a->type == MRP_DBUS_TYPE_SIGNATURE)
            string = TRUE;
        else
            string = FALSE;

        if (string)
            for (i = 0, ptr = a->items; i < a->nitem; i++, ptr += esize)
                mrp_free(ptr);

        mrp_free(a->items);
    }
    else
        mrp_log_error("Hmm... looks like we have a corrupted implicit array.");

    mrp_free(a);
}


static void free_message(mrp_dbus_msg_t *m)
{
    mrp_list_hook_t *p, *n;
    msg_array_t     *a;

    mrp_list_foreach(&m->arrays, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);
        free_msg_array(a);
    }

    mrp_free(m);
}


mrp_dbus_msg_t *mrp_dbus_msg_ref(mrp_dbus_msg_t *m)
{
    return mrp_ref_obj(m, refcnt);
}


int mrp_dbus_msg_unref(mrp_dbus_msg_t *m)
{
    if (mrp_unref_obj(m, refcnt)) {
        sd_bus_message_unref(m->msg);
        free_message(m);

        return TRUE;
    }
    else
        return FALSE;
}


static inline int verify_type(sd_bus_message *msg, int expected_type)
{
    uint8_t type;

    if (sd_bus_message_get_type(msg, &type) != 0 || type == expected_type)
        return FALSE;
    else
        return TRUE;
}


static int dispatch_method(sd_bus *bus,int ret, sd_bus_message *msg, void *data)
{
#define SAFESTR(str) (str ? str : "<none>")
    mrp_dbus_t     *dbus      = (mrp_dbus_t *)data;
    mrp_dbus_msg_t *m         = NULL;
    const char     *path      = sd_bus_message_get_path(msg);
    const char     *interface = sd_bus_message_get_interface(msg);
    const char     *member    = sd_bus_message_get_member(msg);
    int             r         = FALSE;
    handler_list_t *l;
    handler_t      *h;

    MRP_UNUSED(bus);
    MRP_UNUSED(ret);

    if (!verify_type(msg, MRP_DBUS_MESSAGE_TYPE_METHOD_CALL) || !member)
        return r;

    mrp_debug("path='%s', interface='%s', member='%s')...",
              SAFESTR(path), SAFESTR(interface), SAFESTR(member));

    if ((l = mrp_htbl_lookup(dbus->methods, (void *)member)) != NULL) {
    retry:
        if ((h = handler_list_find(l, path, interface, member)) != NULL) {
            sd_bus_message_rewind(msg, TRUE);

            if (m == NULL)
                m = create_message(msg, TRUE);

            if (h->handler(dbus, m, h->user_data))
                r = TRUE;

            goto out;
        }
    }
    else {
        if ((l = mrp_htbl_lookup(dbus->methods, "")) != NULL)
            goto retry;
    }

 out:
    if (!r)
        mrp_debug("Unhandled method path=%s, %s.%s.", SAFESTR(path),
                  SAFESTR(interface), SAFESTR(member));

    mrp_dbus_msg_unref(m);

    return r;
}


static int dispatch_signal(sd_bus *bus,int ret, sd_bus_message *msg, void *data)
{
#define MATCHES(h, field) (!*field || !h->field || !*h->field || \
                           !strcmp(field, h->field))
    mrp_dbus_t     *dbus      = (mrp_dbus_t *)data;
    mrp_dbus_msg_t *m         = NULL;
    const char     *path      = sd_bus_message_get_path(msg);
    const char     *interface = sd_bus_message_get_interface(msg);
    const char     *member    = sd_bus_message_get_member(msg);
    mrp_list_hook_t *p, *n;
    handler_list_t  *l;
    handler_t       *h;
    int              retried = FALSE;
    int              handled = FALSE;

    MRP_UNUSED(bus);
    MRP_UNUSED(ret);

    if (!verify_type(msg, MRP_DBUS_MESSAGE_TYPE_SIGNAL) || !member)
        return FALSE;

    mrp_debug("%s(path='%s', interface='%s', member='%s')...",
              __FUNCTION__, SAFESTR(path), SAFESTR(interface), SAFESTR(member));

    if ((l = mrp_htbl_lookup(dbus->signals, (void *)member)) != NULL) {
    retry:
        mrp_list_foreach(&l->handlers, p, n) {
            h = mrp_list_entry(p, handler_t, hook);

            if (MATCHES(h,path) && MATCHES(h,interface) && MATCHES(h,member)) {
                sd_bus_message_rewind(msg, TRUE);

                if (m == NULL)
                    m = create_message(msg, TRUE);

                h->handler(dbus, m, h->user_data);
                handled = TRUE;
            }
        }
    }

    if (!retried) {
        if ((l = mrp_htbl_lookup(dbus->signals, "")) != NULL) {
            retried = TRUE;
            goto retry;
        }
    }

    if (!handled)
        mrp_debug("Unhandled signal path=%s, %s.%s.", SAFESTR(path),
                  SAFESTR(interface), SAFESTR(member));

    mrp_dbus_msg_unref(m);

    return FALSE;
#undef MATCHES
#undef SAFESTR
}


static int append_args_strtype(mrp_dbus_msg_t *msg, const char *types,
                               va_list ap)
{
    MRP_UNUSED(msg);
    MRP_UNUSED(types);
    MRP_UNUSED(ap);

    return FALSE;
}


static int append_args_inttype(sd_bus_message *msg, int type, va_list ap)
{
    void            *vptr;
    int              atype, elen, i;
    void           **aptr;
    int              alen;
    char             stype[2] = { '\0', '\0' };
    int              r        = 0;

    (void)append_args_strtype;

    while (type != MRP_DBUS_TYPE_INVALID) {
        switch (type) {
        case MRP_DBUS_TYPE_BYTE:
        case MRP_DBUS_TYPE_BOOLEAN:
        case MRP_DBUS_TYPE_INT16:
        case MRP_DBUS_TYPE_UINT16:
        case MRP_DBUS_TYPE_INT32:
        case MRP_DBUS_TYPE_UINT32:
        case MRP_DBUS_TYPE_INT64:
        case MRP_DBUS_TYPE_UINT64:
        case MRP_DBUS_TYPE_DOUBLE:
        case MRP_DBUS_TYPE_STRING:
        case MRP_DBUS_TYPE_OBJECT_PATH:
        case MRP_DBUS_TYPE_SIGNATURE:
        case MRP_DBUS_TYPE_UNIX_FD:
            vptr = va_arg(ap, void *);
            r = sd_bus_message_append_basic(msg, type, vptr);
            break;

        case MRP_DBUS_TYPE_ARRAY:
            atype = va_arg(ap, int);
            aptr  = va_arg(ap, void **);
            alen  = va_arg(ap, int);

            switch (atype) {
#define LEN(_type, _size) case MRP_DBUS_TYPE_##_type: elen = _size; break
                LEN(BYTE       , sizeof(uint8_t));
                LEN(BOOLEAN    , sizeof(uint32_t));
                LEN(INT16      , sizeof(int16_t));
                LEN(UINT16     , sizeof(uint16_t));
                LEN(INT32      , sizeof(int32_t));
                LEN(UINT32     , sizeof(uint32_t));
                LEN(INT64      , sizeof(int64_t));
                LEN(UINT64     , sizeof(uint64_t));
                LEN(DOUBLE     , sizeof(double));
                LEN(STRING     , sizeof(const char *));
                LEN(OBJECT_PATH, sizeof(const char *));
                LEN(SIGNATURE  , sizeof(const char *));
                LEN(UNIX_FD    , sizeof(int));
#undef LEN
            default:
                return FALSE;
            }

            stype[0] = atype;
            if (sd_bus_message_open_container(msg, type, stype) != 0)
                return FALSE;
            for (i = 0; i < alen; i++, aptr += elen)
                if (sd_bus_message_append_basic(msg, atype, aptr) != 0)
                    return FALSE;
            if (sd_bus_message_close_container(msg) != 0)
                return FALSE;
            else
                return TRUE;
            break;

        default:
            return FALSE;
        }

        type = va_arg(ap, int);
    }

    return (r == 0 ? TRUE : FALSE);
}


static int call_reply_cb(sd_bus *bus, int ret, sd_bus_message *msg,
                         void *user_data)
{
    call_t         *call  = (call_t *)user_data;
    mrp_dbus_msg_t *reply = create_message(msg, TRUE);
    sd_bus_error    error;

    MRP_UNUSED(bus);

    call->serial = 0;
    mrp_list_delete(&call->hook);

    if (ret == 0) {
        reply = create_message(msg, TRUE);
        sd_bus_message_rewind(reply->msg, TRUE);
    }
    else {
        sd_bus_message *err = NULL;

        if (ret == ETIMEDOUT)
            error = SD_BUS_ERROR_MAKE(MRP_DBUS_ERROR_TIMEOUT,
                                      "D-Bus call timed out");
        else
            error = SD_BUS_ERROR_MAKE(MRP_DBUS_ERROR_FAILED,
                                      "D-Bus call failed");

        if (sd_bus_message_new_method_error(bus, call->msg, &error, &err) == 0)
            reply = create_message(err, FALSE);
        else
            reply = NULL;
    }

    call->cb(call->dbus, reply, call->user_data);
    call_free(call);
    mrp_dbus_msg_unref(reply);

    return TRUE;
}


int32_t mrp_dbus_call(mrp_dbus_t *dbus, const char *dest, const char *path,
                      const char *interface, const char *member, int timeout,
                      mrp_dbus_reply_cb_t cb, void *user_data, int type, ...)
{
    va_list          ap;
    int32_t          id;
    call_t          *call;
    sd_bus_message  *msg;
    int              success;

    call = NULL;

    if (sd_bus_message_new_method_call(dbus->bus, dest, path, interface, member,
                                       &msg) != 0)
        return 0;

    if (cb != NULL) {
        if ((call = mrp_allocz(sizeof(*call))) != NULL) {
            mrp_list_init(&call->hook);

            call->dbus      = dbus;
            call->id        = dbus->call_id++;
            call->cb        = cb;
            call->user_data = user_data;

            id = call->id;
        }
        else
            goto fail;
    }
    else
        id = dbus->call_id++;

    if (type == MRP_DBUS_TYPE_INVALID)
        success = TRUE;
    else {
        va_start(ap, type);
        success = append_args_inttype(msg, type, ap);
        va_end(ap);
    }

    if (!success)
        goto fail;

    if (cb == NULL) {
        sd_bus_message_set_no_reply(msg, TRUE);
        if (sd_bus_send(dbus->bus, msg, NULL) != 0)
            goto fail;
        sd_bus_message_unref(msg);
    }
    else {
        if (sd_bus_send_with_reply(dbus->bus, msg, call_reply_cb, call,
                                   timeout * 1000, &call->serial) != 0)
            goto fail;

        mrp_list_append(&dbus->calls, &call->hook);
        call->msg = msg;
    }

    return id;

 fail:
    sd_bus_message_unref(msg);
    call_free(call);

    return 0;
}


int mrp_dbus_send_msg(mrp_dbus_t *dbus, mrp_dbus_msg_t *m)
{
    /*bus_message_dump(m->msg);*/

    if (sd_bus_send(dbus->bus, m->msg, NULL) == 0)
        return TRUE;
    else
        return FALSE;
}


int mrp_dbus_call_cancel(mrp_dbus_t *dbus, int32_t id)
{
    mrp_list_hook_t *p, *n;
    call_t          *call;

    mrp_list_foreach(&dbus->calls, p, n) {
        call = mrp_list_entry(p, call_t, hook);

        if (call->id == id) {
            mrp_list_delete(p);

            sd_bus_send_with_reply_cancel(dbus->bus, call->serial);
            call->serial = 0;

            call_free(call);
            return TRUE;
        }
    }

    return FALSE;
}


int mrp_dbus_reply(mrp_dbus_t *dbus, mrp_dbus_msg_t *m, int type, ...)
{
    va_list         ap;
    sd_bus_message *rpl;
    int             success;

    if (sd_bus_message_new_method_return(dbus->bus, m->msg, &rpl) != 0)
        return FALSE;

    va_start(ap, type);
    success = append_args_inttype(rpl, type, ap);
    va_end(ap);

    if (!success)
        goto fail;

    if (sd_bus_send(dbus->bus, rpl, NULL) != 0)
        goto fail;

    sd_bus_message_unref(rpl);

    return TRUE;

 fail:
    sd_bus_message_unref(rpl);

    return FALSE;
}


int mrp_dbus_reply_error(mrp_dbus_t *dbus, mrp_dbus_msg_t *m,
                         const char *errname, const char *errmsg, int type, ...)
{
    va_list         ap;
    sd_bus_message *rpl;
    int             success;
    sd_bus_error    err = SD_BUS_ERROR_NULL;;

    sd_bus_error_set_const(&err, errname, errmsg);

    if (sd_bus_message_new_method_error(dbus->bus, m->msg, &err, &rpl) != 0)
        return FALSE;

    va_start(ap, type);
    success = append_args_inttype(rpl, type, ap);
    va_end(ap);

    if (!success)
        goto fail;

    if (sd_bus_send(dbus->bus, rpl, NULL) != 0)
        goto fail;

    sd_bus_message_unref(rpl);

    return TRUE;

 fail:
    sd_bus_message_unref(rpl);

    return FALSE;
}


static void call_free(call_t *call)
{
    if (call != NULL) {
        sd_bus_message_unref(call->msg);
        mrp_free(call);
    }
}


static void purge_calls(mrp_dbus_t *dbus)
{
    mrp_list_hook_t *p, *n;
    call_t          *call;

    mrp_list_foreach(&dbus->calls, p, n) {
        call = mrp_list_entry(p, call_t, hook);

        mrp_list_delete(&call->hook);

        if (call->serial != 0)
            sd_bus_send_with_reply_cancel(dbus->bus, call->serial);

        mrp_free(call);
    }
}


int mrp_dbus_signal(mrp_dbus_t *dbus, const char *dest, const char *path,
                    const char *interface, const char *member, int type, ...)
{
    va_list         ap;
    sd_bus_message *msg;
    int             success;

    if (sd_bus_message_new_signal(dbus->bus, path, interface, member,
                                  &msg) != 0)
        return 0;

    va_start(ap, type);
    success = append_args_inttype(msg, type, ap);
    va_end(ap);

    if (!success)
        goto fail;

    if (dest != NULL)
        if (sd_bus_message_set_destination(msg, dest) != 0)
            goto fail;

    if (sd_bus_send(dbus->bus, msg, NULL) != 0)
        goto fail;

    sd_bus_message_unref(msg);

    return TRUE;

 fail:
    sd_bus_message_unref(msg);

    return 0;
}


mrp_dbus_msg_t *mrp_dbus_msg_method_call(mrp_dbus_t *dbus,
                                         const char *destination,
                                         const char *path,
                                         const char *interface,
                                         const char *member)
{
    sd_bus_message *msg;

    if (sd_bus_message_new_method_call(dbus->bus, destination,
                                       path, interface, member, &msg) == 0)
        return create_message(msg, FALSE);
    else
        return NULL;
}


mrp_dbus_msg_t *mrp_dbus_msg_method_return(mrp_dbus_t *dbus,
                                           mrp_dbus_msg_t *msg)
{
    sd_bus_message *req, *rpl;

    req = (sd_bus_message *)msg;

    if (sd_bus_message_new_method_return(dbus->bus, req, &rpl) == 0)
        return create_message(rpl, FALSE);
    else
        return NULL;
}


mrp_dbus_msg_t *mrp_dbus_msg_error(mrp_dbus_t *dbus, mrp_dbus_msg_t *m,
                                   mrp_dbus_err_t *err)
{
    sd_bus_message *req, *rpl;

    req = m->msg;

    if (sd_bus_message_new_method_error(dbus->bus, req, err, &rpl) == 0)
        return create_message(rpl, FALSE);
    else
        return NULL;
}


mrp_dbus_msg_t *mrp_dbus_msg_signal(mrp_dbus_t *dbus,
                                    const char *destination,
                                    const char *path,
                                    const char *interface,
                                    const char *member)
{
    sd_bus_message *msg = NULL;

    if (sd_bus_message_new_signal(dbus->bus, path, interface, member,
                                  &msg) == 0) {
        if (destination != NULL) {
            if (sd_bus_message_set_destination(msg, destination) != 0) {
                sd_bus_message_unref(msg);
                msg = NULL;
            }
        }
    }

    return create_message(msg, FALSE);
}


mrp_dbus_msg_type_t mrp_dbus_msg_type(mrp_dbus_msg_t *m)
{
    uint8_t type;

    if (sd_bus_message_get_type(m->msg, &type) == 0)
        return (mrp_dbus_msg_type_t)type;
    else
        return MRP_DBUS_MESSAGE_TYPE_INVALID;
}

#define WRAP_GETTER(type, what)                                        \
    type mrp_dbus_msg_##what(mrp_dbus_msg_t *m)                        \
    {                                                                  \
        return sd_bus_message_get_##what((sd_bus_message *)m->msg);    \
    }                                                                  \
    struct __mrp_dbus_allow_trailing_semicolon

WRAP_GETTER(const char *, path);
WRAP_GETTER(const char *, interface);
WRAP_GETTER(const char *, member);
WRAP_GETTER(const char *, destination);
WRAP_GETTER(const char *, sender);

#undef WRAP_GETTER


int mrp_dbus_msg_open_container(mrp_dbus_msg_t *m, char type,
                                const char *contents)
{
    return sd_bus_message_open_container(m->msg, type, contents) == 0;
}


int mrp_dbus_msg_close_container(mrp_dbus_msg_t *m)
{
    return sd_bus_message_close_container(m->msg) == 0;
}


int mrp_dbus_msg_append_basic(mrp_dbus_msg_t *m, char type, void *valuep)
{
    return sd_bus_message_append_basic(m->msg, type, valuep) == 0;
}


int mrp_dbus_msg_enter_container(mrp_dbus_msg_t *m, char type,
                                 const char *contents)
{
    return sd_bus_message_enter_container(m->msg, type, contents) == 1;
}


int mrp_dbus_msg_exit_container(mrp_dbus_msg_t *m)
{
    return sd_bus_message_exit_container(m->msg) == 1;
}


int mrp_dbus_msg_read_basic(mrp_dbus_msg_t *m, char type, void *valuep)
{
    return sd_bus_message_read_basic(m->msg, type, valuep) == 1;
}


int mrp_dbus_msg_read_array(mrp_dbus_msg_t *m, char type,
                            void **itemsp, size_t *nitemp)
{
    char          sub[2] = { (char)type, '\0' };
    msg_array_t  *a;
    int           offs;
    size_t        esize;

    if ((esize = element_size(type)) == 0)
        return FALSE;

    if (!mrp_dbus_msg_enter_container(m, MRP_DBUS_TYPE_ARRAY, sub))
        return FALSE;

    if ((a = mrp_allocz(sizeof(*a))) == NULL)
        goto fail;

    a->type = type;
    mrp_list_init(&a->hook);

    offs = 0;
    while (mrp_dbus_msg_arg_type(m, NULL) != MRP_DBUS_TYPE_INVALID) {
        if (!mrp_realloc(a->items, offs + esize))
            goto fail;

        if (!mrp_dbus_msg_read_basic(m, type, a->items + offs))
            goto fail;
        else
            a->nitem++;

        offs += esize;
    }

    mrp_dbus_msg_exit_container(m);

    mrp_list_append(&m->arrays, &a->hook);
    *itemsp = a->items;
    *nitemp = a->nitem;

    return TRUE;

 fail:
    mrp_dbus_msg_exit_container(m);
    free_msg_array(a);

    return FALSE;
}


mrp_dbus_type_t mrp_dbus_msg_arg_type(mrp_dbus_msg_t *m, const char **contents)
{
    char type;

    if (sd_bus_message_peek_type(m->msg, &type, contents) >= 0)
        return (mrp_dbus_type_t)type;
    else
        return MRP_DBUS_TYPE_INVALID;
}
