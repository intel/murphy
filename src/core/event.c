/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <stdarg.h>

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/msg.h>

#include <murphy/core/event.h>


/*
 * an event definition
 */

typedef struct {
    char            *name;                    /* event name */
    int              id;                      /* associated event id */
    mrp_list_hook_t  watches;                 /* single-event watches */
} event_def_t;


/*
 * an event watch
 */

struct mrp_event_watch_s {
    mrp_list_hook_t   hook;                   /* hook to event watch list */
    mrp_list_hook_t   purge;                  /* hook to deleted event list */
    mrp_event_cb_t    cb;                     /* notification callback */
    mrp_event_mask_t  events;                 /* events to watch */
    void             *user_data;              /* opaque callback data */
};


static event_def_t     events[MRP_EVENT_MAX]; /* event table */
static int             nevent;                /* number of events */
static int             nemit;                 /* events being emitted */
static mrp_list_hook_t watches;               /* multi-event watches */
static mrp_list_hook_t deleted;               /* events deleted during emit */


static int single_event(mrp_event_mask_t *mask);


MRP_INIT static void init_watch_lists(void)
{
    int i;

    for (i = 0; i < MRP_EVENT_MAX; i++)
        mrp_list_init(&events[i].watches);

    mrp_list_init(&watches);
    mrp_list_init(&deleted);
}


mrp_event_watch_t *mrp_add_event_watch(mrp_event_mask_t *mask,
                                       mrp_event_cb_t cb, void *user_data)
{
    mrp_event_watch_t *w;
    event_def_t       *def;
    int                id;

    if (cb != NULL) {
        w = mrp_allocz(sizeof(*w));

        if (w != NULL) {
            mrp_list_init(&w->hook);
            mrp_list_init(&w->purge);
            w->cb        = cb;
            w->user_data = user_data;
            w->events    = *mask;

            id = single_event(mask);

            if (id != MRP_EVENT_UNKNOWN) {
                if (MRP_EVENT_UNKNOWN < id && id <= nevent) {
                    def = events + id;

                    if (def->name != NULL)
                        mrp_list_append(&def->watches, &w->hook);
                    else {
                        mrp_free(w);
                        w = NULL;
                    }
                }
                else {
                    mrp_free(w);
                    w = NULL;
                }
            }
            else
                mrp_list_append(&watches, &w->hook);
        }
    }
    else
        w = NULL;

    return w;

#if 0
    if (MRP_EVENT_UNKNOWN < id && id <= nevent) {
        def = events + id;

        if (def->name != NULL && cb != NULL) {
            w = mrp_allocz(sizeof(*w));

            if (w != NULL) {
                mrp_list_init(&w->hook);
                mrp_list_init(&w->purge);
                w->cb        = cb;
                w->user_data = user_data;

                if (single_event(mask)) {
                    id = single_event(mask);

                mrp_list_append(&def->watches, &w->hook);

                return w;
            }
        }
    }

    return NULL;
#endif
}


static void delete_watch(mrp_event_watch_t *w)
{
    mrp_list_delete(&w->hook);
    mrp_list_delete(&w->purge);
    mrp_free(w);
}


static void purge_deleted(void)
{
    mrp_event_watch_t *w;
    mrp_list_hook_t   *p, *n;

    mrp_list_foreach(&deleted, p, n) {
        w = mrp_list_entry(p, typeof(*w), purge);
        delete_watch(w);
    }
}


void mrp_del_event_watch(mrp_event_watch_t *w)
{
    if (w != NULL) {
        if (nemit > 0)
            mrp_list_append(&deleted, &w->purge);
        else
            delete_watch(w);
    }
}


int mrp_get_event_id(const char *name, int create)
{
    event_def_t *def;
    int          i;

    for (i = MRP_EVENT_UNKNOWN + 1, def = events + i; i <= nevent; i++, def++) {
        if (!strcmp(name, def->name))
            return i;
    }

    if (create) {
        if (nevent < MRP_EVENT_MAX - 1) {
            def       = events + 1 + nevent;
            def->name = mrp_strdup(name);

            if (def->name != NULL) {
                mrp_list_init(&def->watches);
                def->id = 1 + nevent++;

                return def->id;
            }
        }
    }

    return MRP_EVENT_UNKNOWN;
}


const char *mrp_get_event_name(int id)
{
    event_def_t *def;

    if (MRP_EVENT_UNKNOWN < id && id <= nevent) {
        def = events + id;

        return def->name;
    }

    return "<unknown event>";
}


int mrp_emit_event_msg(int id, mrp_msg_t *event_data)
{
    event_def_t       *def;
    mrp_list_hook_t   *p, *n;
    mrp_event_watch_t *w;

    if (MRP_EVENT_UNKNOWN < id && id <= nevent) {
        def = events + id;

        if (def->name != NULL) {
            nemit++;
            mrp_msg_ref(event_data);

            mrp_debug("emitting event 0x%x (%s)", def->id, def->name);
#if 0
            mrp_msg_dump(event_data, stdout);
#endif

            mrp_list_foreach(&def->watches, p, n) {
                w = mrp_list_entry(p, typeof(*w), hook);
                w->cb(w, def->id, event_data, w->user_data);
            }

            mrp_list_foreach(&watches, p, n) {
                w = mrp_list_entry(p, typeof(*w), hook);
                if (mrp_test_event(&w->events, id))
                    w->cb(w, def->id, event_data, w->user_data);
            }

            nemit--;
            mrp_msg_unref(event_data);
        }

        if (nemit <= 0)
            purge_deleted();

        return TRUE;
    }

    return FALSE;
}


int mrp_emit_event(int id, ...)
{
    mrp_msg_t *msg;
    uint16_t   tag;
    va_list    ap;
    int        success;

    va_start(ap, id);
    tag = va_arg(ap, unsigned int);
    if (tag != MRP_MSG_FIELD_INVALID)
        msg = mrp_msg_createv(tag, ap);
    else
        msg = NULL;
    va_end(ap);

    success = mrp_emit_event_msg(id, msg);
    mrp_msg_unref(msg);

    return success;
}


mrp_event_mask_t *mrp_set_events(mrp_event_mask_t *mask, ...)
{
    va_list ap;
    int     id;

    mrp_reset_event_mask(mask);

    va_start(ap, mask);
    while ((id = va_arg(ap, int)) != MRP_EVENT_UNKNOWN) {
        mrp_add_event(mask, id);
    }
    va_end(ap);

    return mask;
}


mrp_event_mask_t *mrp_set_named_events(mrp_event_mask_t *mask, ...)
{
    va_list     ap;
    const char *name;
    int         id;

    mrp_reset_event_mask(mask);

    va_start(ap, mask);
    while ((name = va_arg(ap, const char *)) != NULL) {
        id = mrp_lookup_event(name);

        if (id != MRP_EVENT_UNKNOWN)
            mrp_add_event(mask, id);
    }
    va_end(ap);

    return mask;
}


static int event_count(mrp_event_mask_t *mask)
{
    uint64_t bits = *mask;

    return __builtin_popcountll(bits);
}


static int lowest_bit(mrp_event_mask_t *mask)
{
    uint64_t bits = *mask;

    return __builtin_ffs(bits);
}


static int single_event(mrp_event_mask_t *mask)
{
    if (event_count(mask) == 1)
        return lowest_bit(mask);
    else
        return MRP_EVENT_UNKNOWN;
}
