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

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/refcnt.h>
#include <murphy/common/utils.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/libdbus.h>


#define DBUS_ADMIN_SERVICE   "org.freedesktop.DBus"
#define DBUS_ADMIN_INTERFACE "org.freedesktop.DBus"
#define DBUS_ADMIN_PATH      "/org/freedesktop/DBus"
#define DBUS_NAME_CHANGED    "NameOwnerChanged"


struct mrp_dbus_s {
    char            *address;            /* bus address */
    DBusConnection  *conn;               /* actual D-BUS connection */
    mrp_mainloop_t  *ml;                 /* murphy mainloop */
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
    DBusPendingCall     *pend;           /* pending DBUS call */
    mrp_list_hook_t      hook;           /* hook to list of pending calls */
} call_t;


typedef struct {
    mrp_mainloop_t *ml;                  /* mainloop for bus connection */
    const char     *address;             /* address of bus */
} bus_spec_t;

static mrp_htbl_t *buses;



static DBusHandlerResult dispatch_signal(DBusConnection *c,
                                         DBusMessage *msg, void *data);
static DBusHandlerResult dispatch_method(DBusConnection *c,
                                         DBusMessage *msg, void *data);
static void purge_name_trackers(mrp_dbus_t *dbus);
static void purge_calls(mrp_dbus_t *dbus);
static void handler_list_free_cb(void *key, void *entry);
static void handler_free(handler_t *h);
static int name_owner_change_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data);
static void call_free(call_t *call);




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


void dbus_disconnect(mrp_dbus_t *dbus)
{
    if (dbus) {
        mrp_htbl_remove(buses, dbus->conn, FALSE);

        if (dbus->signals) {
            mrp_htbl_foreach(dbus->signals, purge_filters, dbus);
            mrp_htbl_destroy(dbus->signals, TRUE);
        }
        if (dbus->methods)
            mrp_htbl_destroy(dbus->methods, TRUE);

        if (dbus->conn != NULL) {
            if (dbus->signal_filter)
                dbus_connection_remove_filter(dbus->conn, dispatch_signal,
                        dbus);
            if (dbus->register_fallback)
                dbus_connection_unregister_object_path(dbus->conn, "/");
            if (dbus->priv)
                dbus_connection_close(dbus->conn);
            dbus_connection_unref(dbus->conn);
        }

        purge_name_trackers(dbus);
        purge_calls(dbus);

        mrp_free(dbus->address);
        dbus->conn = NULL;
        dbus->ml   = NULL;

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
                             DBusError *errp)
{
    static struct DBusObjectPathVTable vtable = {
        .message_function = dispatch_method
    };

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

    if (!strcmp(address, "system"))
        dbus->conn = dbus_bus_get(DBUS_BUS_SYSTEM, errp);
    else if (!strcmp(address, "session"))
        dbus->conn = dbus_bus_get(DBUS_BUS_SESSION, errp);
    else {
        dbus->conn = dbus_connection_open_private(address, errp);
        dbus->priv = TRUE;

        if (dbus->conn == NULL || !dbus_bus_register(dbus->conn, errp))
            goto fail;
    }

    if (dbus->conn == NULL)
        goto fail;

    dbus->address     = mrp_strdup(address);
    dbus->unique_name = dbus_bus_get_unique_name(dbus->conn);

    /*
     * set up with mainloop
     */

    if (!mrp_dbus_setup_connection(ml, dbus->conn))
        goto fail;

    /*
     * set up our message dispatchers and take our name on the bus
     */

    if (!dbus_connection_add_filter(dbus->conn, dispatch_signal, dbus, NULL)) {
        dbus_set_error(errp, DBUS_ERROR_FAILED,
                       "Failed to set up signal dispatching.");
        goto fail;
    }
    dbus->signal_filter = TRUE;

    if (!dbus_connection_register_fallback(dbus->conn, "/", &vtable, dbus)) {
        dbus_set_error(errp, DBUS_ERROR_FAILED,
                       "Failed to set up method dispatching.");
        goto fail;
    }
    dbus->register_fallback = TRUE;

    mrp_clear(&hcfg);
    hcfg.comp = mrp_string_comp;
    hcfg.hash = mrp_string_hash;
    hcfg.free = handler_list_free_cb;

    if ((dbus->methods = mrp_htbl_create(&hcfg)) == NULL) {
        dbus_set_error(errp, DBUS_ERROR_FAILED,
                       "Failed to create DBUS method table.");
        goto fail;
    }

    if ((dbus->signals = mrp_htbl_create(&hcfg)) == NULL) {
        dbus_set_error(errp, DBUS_ERROR_FAILED,
                       "Failed to create DBUS signal table.");
        goto fail;
    }


    /*
     * install handler for NameOwnerChanged for tracking clients/peers
     */

    if (!mrp_dbus_add_signal_handler(dbus, DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                                     DBUS_ADMIN_SERVICE, DBUS_NAME_CHANGED,
                                     name_owner_change_cb, NULL)) {
        dbus_set_error(errp, DBUS_ERROR_FAILED,
                       "Failed to install NameOwnerChanged handler.");
        goto fail;
    }

    /* install a 'safe' filter to avoid receiving all name change signals */
    mrp_dbus_install_filter(dbus,
                            DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                            DBUS_ADMIN_SERVICE, DBUS_NAME_CHANGED,
                            DBUS_ADMIN_SERVICE, NULL);

    mrp_list_init(&dbus->name_trackers);
    dbus->call_id = 1;

    if (mrp_htbl_insert(buses, dbus->conn, dbus))
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


int mrp_dbus_acquire_name(mrp_dbus_t *dbus, const char *name, DBusError *error)
{
    int flags, status;

    mrp_dbus_error_init(error);

    flags  = DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE;
    status = dbus_bus_request_name(dbus->conn, name, flags, error);

    if (status == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        return TRUE;
    else {
        if (status == DBUS_REQUEST_NAME_REPLY_EXISTS) {
            if (error)
                dbus_error_free(error);
            dbus_set_error(error, DBUS_ERROR_FAILED, "name already taken");
        }
        return FALSE;
    }
}


int mrp_dbus_release_name(mrp_dbus_t *dbus, const char *name, DBusError *error)
{
    mrp_dbus_error_init(error);

    if (dbus_bus_release_name(dbus->conn, name, error) != -1)
        return TRUE;
    else
        return FALSE;
}


const char *mrp_dbus_get_unique_name(mrp_dbus_t *dbus)
{
    return dbus->unique_name;
}

static void name_owner_query_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    name_tracker_t *t = (name_tracker_t *)data;
    const char     *owner;
    int             state;

    if (t->cb != NULL) {                /* tracker still active */
        t->qid = 0;
        state  = dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN;

        if (!dbus_message_get_args(msg, NULL,
                                   DBUS_TYPE_STRING, &owner,
                                   DBUS_TYPE_INVALID))
            owner = "<unknown>";

        t->cb(dbus, t->name, state, owner, t->user_data);
    }
    else                                /* already requested to delete */
        mrp_free(t);
}


static int name_owner_change_cb(mrp_dbus_t *dbus, DBusMessage *msg, void *data)
{
    const char      *name, *prev, *next;
    mrp_list_hook_t *p, *n;
    name_tracker_t  *t;

    MRP_UNUSED(data);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
        return FALSE;

    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_STRING, &prev,
                               DBUS_TYPE_STRING, &next,
                               DBUS_TYPE_INVALID))
        return FALSE;

    /*
     * Notes: XXX TODO
     *    In principle t->cb could call mrp_dbus_forget for some other D-BUS
     *    address than name. If that happened to be n (== p->hook.next) this
     *    would result in a crash or memory corruption in the next iteration
     *    of this loop (when handling n). We can easily get around this
     *    problem by
     *
     *     1) adminstering in mrp_dbus_t that we're handing a NameOwnerChange
     *     2) checking for this in mrp_dbus_forget_name and if it is the case
     *        only marking the affected entry for deletion
     *     3) removing entries marked for deletion in this loop (or just
     *        ignoring them and making another pass in the end removing any
     *        such entry).
     */

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
                                        DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                                        DBUS_ADMIN_SERVICE, DBUS_NAME_CHANGED,
                                        name, NULL)) {
                mrp_list_append(&dbus->name_trackers, &t->hook);

                t->qid = mrp_dbus_call(dbus,
                                       DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                                       DBUS_ADMIN_SERVICE, "GetNameOwner", 5000,
                                       name_owner_query_cb, t,
                                       DBUS_TYPE_STRING, &t->name,
                                       DBUS_TYPE_INVALID);
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
                           DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                           DBUS_ADMIN_SERVICE, DBUS_NAME_CHANGED,
                           name, NULL);

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
        mrp_dbus_remove_filter(dbus, DBUS_ADMIN_SERVICE, DBUS_ADMIN_PATH,
                               DBUS_ADMIN_SERVICE, DBUS_NAME_CHANGED,
                               t->name, NULL);
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


int mrp_dbus_export_method(mrp_dbus_t *dbus, const char *path,
                           const char *interface, const char *member,
                           mrp_dbus_handler_t handler, void *user_data)
{
    handler_list_t *methods;
    handler_t      *m;

    if ((methods = mrp_htbl_lookup(dbus->methods, (void *)member)) == NULL) {
        if ((methods = handler_list_alloc(member)) == NULL)
            return FALSE;

        mrp_htbl_insert(dbus->methods, methods->member, methods);
    }

    m = handler_alloc(NULL, path, interface, member, handler, user_data);
    if (m != NULL) {
        handler_list_insert(methods, m);
        return TRUE;
    }
    else
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
#define ADD_TAG(tag, value, ...) do {                                     \
        if (value != NULL) {                                              \
            l = snprintf(p, n, "%s%s='%s'", p == filter ? "" : ",",       \
                         tag, value);                                     \
            if (l >= n)                                                   \
                do { __VA_ARGS__; } while (0);                            \
            n -= l;                                                       \
            p += l;                                                       \
        }                                                                 \
    } while (0)

    va_list   ap;
    DBusError error;
    char      filter[1024], *p, argn[16], *val;
    int       n, l, i;

    p = filter;
    n = sizeof(filter);

    ADD_TAG("type"     , "signal"  , return FALSE);
    ADD_TAG("sender"   ,  sender   , return FALSE);
    ADD_TAG("path"     ,  path     , return FALSE);
    ADD_TAG("interface",  interface, return FALSE);
    ADD_TAG("member"   ,  member   , return FALSE);

    va_copy(ap, args);
    i = 0;
    while ((val = va_arg(ap, char *)) != NULL) {
        snprintf(argn, sizeof(argn), "arg%d", i);
        ADD_TAG(argn, val, { va_end(ap); return FALSE; });
        i++;
    }
    va_end(ap);

    dbus_error_init(&error);
    dbus_bus_add_match(dbus->conn, filter, &error);

    if (dbus_error_is_set(&error)) {
        mrp_log_error("Failed to install filter '%s' (error: %s).", filter,
                      mrp_dbus_errmsg(&error));
        dbus_error_free(&error);

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

    ADD_TAG("type"     , "signal"  , return FALSE);
    ADD_TAG("sender"   ,  sender   , return FALSE);
    ADD_TAG("path"     ,  path     , return FALSE);
    ADD_TAG("interface",  interface, return FALSE);
    ADD_TAG("member"   ,  member   , return FALSE);

    va_copy(ap, args);
    i = 0;
    while ((val = va_arg(ap, char *)) != NULL) {
        snprintf(argn, sizeof(argn), "arg%d", i);
        ADD_TAG(argn, val, { va_end(ap); return FALSE; });
        i++;
    }
    va_end(ap);

    dbus_bus_remove_match(dbus->conn, filter, NULL);
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



static DBusHandlerResult dispatch_method(DBusConnection *c,
                                         DBusMessage *msg, void *data)
{
#define SAFESTR(str) (str ? str : "<none>")
    const char *path      = dbus_message_get_path(msg);
    const char *interface = dbus_message_get_interface(msg);
    const char *member    = dbus_message_get_member(msg);

    mrp_dbus_t     *dbus = (mrp_dbus_t *)data;
    handler_list_t *l;
    handler_t      *h;

    MRP_UNUSED(c);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL || !member)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    mrp_debug("path='%s', interface='%s', member='%s')...",
              SAFESTR(path), SAFESTR(interface), SAFESTR(member));

    if ((l = mrp_htbl_lookup(dbus->methods, (void *)member)) != NULL) {
    retry:
        if ((h = handler_list_find(l, path, interface, member)) != NULL) {
            if (h->handler(dbus, msg, h->user_data))
                return DBUS_HANDLER_RESULT_HANDLED;
            else
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
    else {
        if ((l = mrp_htbl_lookup(dbus->methods, "")) != NULL)
            goto retry;
    }

    mrp_debug("Unhandled method path=%s, %s.%s.", SAFESTR(path),
              SAFESTR(interface), SAFESTR(member));

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static DBusHandlerResult dispatch_signal(DBusConnection *c,
                                         DBusMessage *msg, void *data)
{
#define MATCHES(h, field) (!*field || !h->field || !*h->field || \
                           !strcmp(field, h->field))

    const char *path      = dbus_message_get_path(msg);
    const char *interface = dbus_message_get_interface(msg);
    const char *member    = dbus_message_get_member(msg);

    mrp_dbus_t      *dbus = (mrp_dbus_t *)data;
    mrp_list_hook_t *p, *n;
    handler_list_t  *l;
    handler_t       *h;
    int              retried = FALSE;
    int              handled = FALSE;

    MRP_UNUSED(c);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL || !member)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    mrp_debug("%s(path='%s', interface='%s', member='%s')...",
              __FUNCTION__,
              SAFESTR(path), SAFESTR(interface), SAFESTR(member));

    if ((l = mrp_htbl_lookup(dbus->signals, (void *)member)) != NULL) {
    retry:
        mrp_list_foreach(&l->handlers, p, n) {
            h = mrp_list_entry(p, handler_t, hook);

            if (MATCHES(h,path) && MATCHES(h,interface) && MATCHES(h,member)) {
                h->handler(dbus, msg, h->user_data);
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

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
#undef MATCHES
#undef SAFESTR
}


static void call_reply_cb(DBusPendingCall *pend, void *user_data)
{
    call_t      *call = (call_t *)user_data;
    DBusMessage *reply;

    reply = dbus_pending_call_steal_reply(pend);

    call->pend = NULL;
    mrp_list_delete(&call->hook);

    call->cb(call->dbus, reply, call->user_data);

    dbus_message_unref(reply);
    dbus_pending_call_unref(pend);

    call_free(call);
}


int32_t mrp_dbus_call(mrp_dbus_t *dbus, const char *dest, const char *path,
                      const char *interface, const char *member, int timeout,
                      mrp_dbus_reply_cb_t cb, void *user_data, int type, ...)
{
    va_list          ap;
    int32_t          id;
    call_t          *call;
    DBusMessage     *msg;
    DBusPendingCall *pend;
    int              success;

    call = NULL;
    pend = NULL;

    msg = dbus_message_new_method_call(dest, path, interface, member);

    if (msg == NULL)
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

    if (type == DBUS_TYPE_INVALID)
        success = TRUE;
    else {
        va_start(ap, type);
        success = dbus_message_append_args_valist(msg, type, ap);
        va_end(ap);
    }

    if (!success)
        goto fail;

    if (cb == NULL) {
        dbus_message_set_no_reply(msg, TRUE);
        if (!dbus_connection_send(dbus->conn, msg, NULL))
            goto fail;
    }
    else {
        if (!dbus_connection_send_with_reply(dbus->conn, msg, &pend, timeout))
            goto fail;

        if (!dbus_pending_call_set_notify(pend, call_reply_cb, call, NULL))
            goto fail;
    }

    if (cb != NULL) {
        mrp_list_append(&dbus->calls, &call->hook);
        call->pend = pend;
    }

    dbus_message_unref(msg);

    return id;

 fail:
    if (pend != NULL)
        dbus_pending_call_unref(pend);

    if(msg != NULL)
        dbus_message_unref(msg);

    call_free(call);

    return 0;
}


int32_t mrp_dbus_send(mrp_dbus_t *dbus, const char *dest, const char *path,
                      const char *interface, const char *member, int timeout,
                      mrp_dbus_reply_cb_t cb, void *user_data, DBusMessage *msg)
{
    int32_t          id;
    call_t          *call;
    DBusPendingCall *pend;
    int              method;

    call = NULL;
    pend = NULL;

    if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL) {
        if (cb != NULL)
            goto fail;
        else
            method = FALSE;
    }
    else
        method = TRUE;

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

    if (!dbus_message_set_destination(msg, dest))
        goto fail;
    if (!dbus_message_set_path(msg, path))
        goto fail;
    if (!dbus_message_set_interface(msg, interface))
        goto fail;
    if (!dbus_message_set_member(msg, member))
        goto fail;

    if (cb == NULL) {
        if (method)
            dbus_message_set_no_reply(msg, TRUE);
        if (!dbus_connection_send(dbus->conn, msg, NULL))
            goto fail;
    }
    else {
        if (!dbus_connection_send_with_reply(dbus->conn, msg, &pend, timeout))
            goto fail;

        if (!dbus_pending_call_set_notify(pend, call_reply_cb, call, NULL))
            goto fail;
    }

    if (cb != NULL) {
        mrp_list_append(&dbus->calls, &call->hook);
        call->pend = pend;
    }

    return id;

 fail:
    if (pend != NULL)
        dbus_pending_call_unref(pend);

    if(msg != NULL)
        dbus_message_unref(msg);

    call_free(call);

    return 0;
}


int mrp_dbus_send_msg(mrp_dbus_t *dbus, DBusMessage *msg)
{
    return dbus_connection_send(dbus->conn, msg, NULL);
}


int mrp_dbus_call_cancel(mrp_dbus_t *dbus, int32_t id)
{
    mrp_list_hook_t *p, *n;
    call_t          *call;

    mrp_list_foreach(&dbus->calls, p, n) {
        call = mrp_list_entry(p, call_t, hook);

        if (call->id == id) {
            mrp_list_delete(p);

            dbus_pending_call_cancel(call->pend);
            dbus_pending_call_unref(call->pend);
            call->pend = NULL;

            call_free(call);
            return TRUE;
        }
    }

    return FALSE;
}


int mrp_dbus_reply(mrp_dbus_t *dbus, DBusMessage *msg, int type, ...)
{
    va_list      ap;
    DBusMessage *rpl;
    int          success;

    rpl = dbus_message_new_method_return(msg);

    if (rpl == NULL)
        return FALSE;

    if (type == DBUS_TYPE_INVALID)
        success = TRUE;
    else {
        va_start(ap, type);
        success = dbus_message_append_args_valist(rpl, type, ap);
        va_end(ap);
    }

    if (!success)
        goto fail;

    if (!dbus_connection_send(dbus->conn, rpl, NULL))
        goto fail;

    dbus_message_unref(rpl);

    return TRUE;

 fail:
    if(rpl != NULL)
        dbus_message_unref(rpl);

    return FALSE;
}


int mrp_dbus_reply_error(mrp_dbus_t *dbus, DBusMessage *msg,
                         const char *errname, const char *errmsg, int type, ...)
{
    va_list      ap;
    DBusMessage *rpl;
    int          success;

    rpl = dbus_message_new_error(msg, errname, errmsg);

    if (rpl == NULL)
        return FALSE;

    if (type == DBUS_TYPE_INVALID)
        success = TRUE;
    else {
        va_start(ap, type);
        success = dbus_message_append_args_valist(rpl, type, ap);
        va_end(ap);
    }

    if (!success)
        goto fail;

    if (!dbus_connection_send(dbus->conn, rpl, NULL))
        goto fail;

    dbus_message_unref(rpl);

    return TRUE;

 fail:
    if(rpl != NULL)
        dbus_message_unref(rpl);

    return FALSE;
}


static void call_free(call_t *call)
{
    if (call != NULL)
        mrp_free(call);
}


static void purge_calls(mrp_dbus_t *dbus)
{
    mrp_list_hook_t *p, *n;
    call_t          *call;

    mrp_list_foreach(&dbus->calls, p, n) {
        call = mrp_list_entry(p, call_t, hook);

        mrp_list_delete(&call->hook);

        if (call->pend != NULL)
            dbus_pending_call_unref(call->pend);

        mrp_free(call);
    }
}


int mrp_dbus_signal(mrp_dbus_t *dbus, const char *dest, const char *path,
                    const char *interface, const char *member, int type, ...)
{
    va_list      ap;
    DBusMessage *msg;
    int          success;

    msg = dbus_message_new_signal(path, interface, member);

    if (msg == NULL)
        return 0;

    if (type == DBUS_TYPE_INVALID)
        success = TRUE;
    else {
        va_start(ap, type);
        success = dbus_message_append_args_valist(msg, type, ap);
        va_end(ap);
    }

    if (!success)
        goto fail;

    if (dest && *dest && !dbus_message_set_destination(msg, dest))
        goto fail;

    if (!dbus_connection_send(dbus->conn, msg, NULL))
        goto fail;

    dbus_message_unref(msg);

    return TRUE;

 fail:
    /*
     * XXX TODO: Hmm... IIRC, libdbus unrefs messages upon failure. If it
     *           was really so, this would corrupt/crash. Check this from
     *           libdbus code.
     */
    if(msg != NULL)
        dbus_message_unref(msg);

    return 0;
}
