#ifndef __MURPHY_LIST_H__
#define __MURPHY_LIST_H__

#include <murphy/common/macros.h>


MRP_CDECL_BEGIN


/** \file
 * A simple doubly-linked circular list implementation, obviously inspired
 * by the linux kernel.
 */


/** A list hook. Used both a list head and to hook up objects to the list. */
typedef struct mrp_list_hook_s mrp_list_hook_t;
struct mrp_list_hook_s {
    mrp_list_hook_t *prev;
    mrp_list_hook_t *next;
};

/** Macro to initialize a list to be empty. */
#define MRP_LIST_INIT(list) { .prev = &(list), .next = &(list) }

/** Macro to define a list and initialize it to be empty. */
#define MRP_LIST_HOOK(list) mrp_list_hook_t list = MRP_LIST_INIT(list)

/** Initialize a list to be empty. */
static inline void mrp_list_init(mrp_list_hook_t *list)
{
    list->prev = list->next = list;
}

/** Check if a list is empty. */
static inline int mrp_list_empty(mrp_list_hook_t *list)
{
    if (list->next == list->prev) {
        if (list->next == list)
            return TRUE;

#ifdef __MURPHY_LIST_ALLOW_NULL
        if (!list->next)
            return TRUE;
#endif
    }

    return FALSE;
}

/** Append a new item to a list (add it after the last item). */
static inline void mrp_list_append(mrp_list_hook_t *list, mrp_list_hook_t *item)
{
    if (mrp_list_empty(list)) {
        list->next = list->prev = item;
        item->next = item->prev = list;
    }
    else {
        mrp_list_hook_t *prev = list->prev;

        prev->next = item;
        item->prev = prev;
        item->next = list;
        list->prev = item;
    }
}

/** Prepend a new item to a list (add it before the first item). */
static inline void mrp_list_prepend(mrp_list_hook_t *list,
                                    mrp_list_hook_t *item)
{
    if (mrp_list_empty(list)) {
        list->next = list->prev = item;
        item->next = item->prev = list;
    }
    else {
        mrp_list_hook_t *next = list->next;

        list->next = item;
        item->prev = list;
        item->next = next;
        next->prev = item;
    }
}

/** Delete the given item from the list. */
static inline void mrp_list_delete(mrp_list_hook_t *item)
{
    mrp_list_hook_t *prev, *next;

    if (!mrp_list_empty(item)) {
        prev = item->prev;
        next = item->next;

        prev->next = next;
        next->prev = prev;

        item->prev = item->next = item;
    }
}

/** Reattach a list to a new hook. Initialize old hook to be empty. */
static inline void mrp_list_move(mrp_list_hook_t *new_hook,
                                 mrp_list_hook_t *old_hook)
{
    *new_hook = *old_hook;

    new_hook->next->prev = new_hook;
    new_hook->prev->next = new_hook;

    mrp_list_init(old_hook);
}


/** Macro to iterate through a list (current item safe to remove). */
#define mrp_list_foreach(list, p, n)                                      \
    if ((list)->next != NULL)                                             \
        for (p = (list)->next, n = p->next; p != (list); p = n, n = n->next)

/** Macro to iterate through a list backwards (current item safe to remove). */
#define mrp_list_foreach_back(list, p, n)                                 \
    if ((list)->prev != NULL)                                             \
        for (p = (list)->prev, n = p->prev; p != (list); p = n, n = n->prev)

/** Macro to get a pointer to a embedding structure from a list pointer. */
#ifndef __cplusplus
#    define PTR_ARITH_TYPE void
#else
#    define PTR_ARITH_TYPE char
#endif

#define mrp_list_entry(ptr, type, member)                                 \
    (type *)(((PTR_ARITH_TYPE *)(ptr)) - MRP_OFFSET(type, member))


MRP_CDECL_END


#endif /* __MURPHY_LIST_H__ */

