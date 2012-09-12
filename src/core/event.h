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

#ifndef __MURPHY_EVENT_H__
#define __MURPHY_EVENT_H__

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/msg.h>

MRP_CDECL_BEGIN

#define MRP_EVENT_UNKNOWN   0
#define MRP_EVENT_MAX      64


/*
 * event declarations
 */

typedef struct {
    const char *name;                    /* event name */
    int         id;                      /* associated event id */
} mrp_event_t;


/*
 * Convenience macro for autoregistering a table of events on startup.
 */

#define MRP_REGISTER_EVENTS(_event_table, ...)                          \
    static mrp_event_t _event_table[] = {                               \
        __VA_ARGS__,                                                    \
        { .name = NULL }                                                \
    };                                                                  \
    MRP_INIT static void register_events(void)                          \
    {                                                                   \
        mrp_event_t *e;                                                 \
        int          i;                                                 \
                                                                        \
        for (i = 0, e = _event_table; e->name != NULL; i++, e++) {      \
            if (e->id != i) {                                           \
                mrp_log_error("%s:%d: misinitialized event table %s.",  \
                              __FILE__, __LINE__, #_event_table);       \
                mrp_log_error("This can result from passing a mis"      \
                              "initialized event table to the macro");  \
                mrp_log_error("MRP_REGISTER_EVENT, ie. a table where "  \
                              "id != index for some element).");        \
                return;                                                 \
            }                                                           \
                                                                        \
            e->id = mrp_register_event(e->name);                        \
                                                                        \
            if (e->id != MRP_EVENT_UNKNOWN)                             \
                mrp_log_info("Event '%s' registered as 0x%x.",          \
                             e->name, e->id);                           \
            else                                                        \
                mrp_log_error("Failed to register event '%s'.",         \
                              e->name);                                 \
        }                                                               \
    }                                                                   \
    struct __mrp_allow_trailing_semicolon


typedef uint64_t mrp_event_mask_t;


typedef struct mrp_event_watch_s mrp_event_watch_t;


/** Event watch notification callback type. */
typedef void (*mrp_event_cb_t)(mrp_event_watch_t *w, int id,
                               mrp_msg_t *event_data, void *user_data);

/** Subscribe for an event. */
mrp_event_watch_t *mrp_add_event_watch(mrp_event_mask_t *events,
                                       mrp_event_cb_t cb,
                                       void *user_data);

/** Unsubscribe from an event. */
void mrp_del_event_watch(mrp_event_watch_t *w);

/** Get the id of the given event, create missing events is asked for. */
int mrp_get_event_id(const char *name, int create);

/** Get the name of the event corresponding to the given id. */
const char *mrp_get_event_name(int id);

/** Register a new event. */
#define mrp_register_event(name) mrp_get_event_id(name, TRUE)

/** Look up the id of an existing event. */
#define mrp_lookup_event(name) mrp_get_event_id(name, FALSE)

/** Emit an event with the given message. */
int mrp_emit_event_msg(int id, mrp_msg_t *event_data);

/** Emit an event with a message constructed from the given parameters. */
int mrp_emit_event(int id, ...) MRP_NULLTERM;

/** Initialize an event mask to be empty. */
static inline void mrp_reset_event_mask(mrp_event_mask_t *mask)
{
    *mask = 0;
}

/** Turn on the bit corresponding to id in mask. */
static inline void mrp_add_event(mrp_event_mask_t *mask, int id)
{
    *mask |= (1 << (id - 1));
}

/** Turn off the bit corresponding to id in mask. */
static inline void mrp_del_event(mrp_event_mask_t *mask, int id)
{
    *mask &= ~(1 << (id - 1));
}

/** Test if the bit corresponding to id in mask is on. */
static inline int mrp_test_event(mrp_event_mask_t *mask, int id)
{
    return *mask & (1 << (id - 1));
}

/** Turn on the bit corresponding to the named event in mask. */
static inline int mrp_add_named_event(mrp_event_mask_t *mask, const char *name)
{
    int id = mrp_lookup_event(name);

    if (id != 0) {
        mrp_add_event(mask, id);
        return TRUE;
    }
    else
        return FALSE;
}

/** Turn off the bit corresponding to the named event in mask. */
static inline int mrp_del_named_event(mrp_event_mask_t *mask, const char *name)
{
    int id = mrp_lookup_event(name);

    if (id != 0) {
        mrp_del_event(mask, id);
        return TRUE;
    }
    else
        return FALSE;
}

/** Test the bit corresponding to the named event in mask. */
static inline int mrp_test_named_event(mrp_event_mask_t *mask, const char *name)
{
    int id = mrp_lookup_event(name);

    if (id != 0)
        return mrp_test_event(mask, id);
    else
        return FALSE;
}

/** Set up the given mask with the given event ids (terminated by
    MRP_EVENT_UNKNOWN). */
mrp_event_mask_t *mrp_set_events(mrp_event_mask_t *mask, ...);

/** Set up the given mask with the given named events (terminated by
    NULL). */
mrp_event_mask_t *mrp_set_named_events(mrp_event_mask_t *mask, ...);

MRP_CDECL_END

#endif /* __MURPHY_EVENT_H__ */
