#ifndef __MDB_LIST_H__
#define __MDB_LIST_H__

#include <murphy-db/mqi-types.h>

#define MDB_LIST_RELOCATE(structure, member, ptr)                       \
    ((structure *)((void *)ptr - MQI_OFFSET(structure, member)))


#define MDB_DLIST_HEAD(name)   mdb_dlist_t name = { &(name), &(name) }

#define MDB_DLIST_INIT(self)                                            \
    do {                                                                \
        (&(self))->prev = &(self);                                      \
        (&(self))->next = &(self);                                      \
    } while(0)

#define MDB_DLIST_EMPTY(name)  ((&(name))->next == &(name))

#define MDB_DLIST_FOR_EACH(structure, member, pos, head)                \
    for (pos = MDB_LIST_RELOCATE(structure, member, (head)->next);      \
         &pos->member != (head);                                        \
         pos = MDB_LIST_RELOCATE(structure, member, pos->member.next))

#define MDB_DLIST_FOR_EACH_SAFE(structure, member, pos, n, head)        \
    for (pos = MDB_LIST_RELOCATE(structure, member, (head)->next),      \
           n = MDB_LIST_RELOCATE(structure, member, pos->member.next);  \
         &pos->member != (head);                                        \
         pos = n,                                                       \
           n = MDB_LIST_RELOCATE(structure, member, pos->member.next))

#define MDB_DLIST_FOR_EACH_NOHEAD(structure, member, pos, start)        \
    for (pos = start;                                                   \
         &(pos)->member != &(start)->member;                            \
         pos = MDB_LIST_RELOCATE(structure, member, pos->member.next))

#define MDB_DLIST_FOR_EACH_NOHEAD_SAFE(structure, member, pos,n, start) \
    for (pos = start,                                                   \
           n = MDB_LIST_RELOCATE(structure, member, pos->member.next);  \
         &pos->member != &(start)->member;                              \
         pos = n,                                                       \
           n = MDB_LIST_RELOCATE(structure, member, pos->member.next))

#define MDB_DLIST_INSERT_BEFORE(structure, member, new, before)         \
    do {                                                                \
        mdb_dlist_t *after = (before)->prev;                            \
        after->next = &(new)->member;                                   \
        (new)->member.next = before;                                    \
        (before)->prev = &(new)->member;                                \
        (new)->member.prev = after;                                     \
    } while(0)
#define MDB_DLIST_APPEND(structure, member, new, head)                  \
    MDB_DLIST_INSERT_BEFORE(structure, member, new, head)

#define MDB_DLIST_INSERT_AFTER(structure, member, new, after)           \
    do {                                                                \
        mdb_dlist_t *before = (after)->next;                            \
        (after)->next = &((new)->member);                               \
        (new)->member.next = before;                                    \
        before->prev = &((new)->member);                                \
        (new)->member.prev = after;                                     \
    } while(0)
#define MDB_DLIST_PREPEND(structure, member, new, head)                 \
    MDB_DLIST_INSERT_AFTER(structure, member, new, head)


#define MDB_DLIST_UNLINK(structure, member, elem)                       \
    do {                                                                \
        mdb_dlist_t *after  = (elem)->member.prev;                      \
        mdb_dlist_t *before = (elem)->member.next;                      \
        after->next = before;                                           \
        before->prev = after;                                           \
        (elem)->member.prev = (elem)->member.next = &(elem)->member;    \
    } while(0)


typedef struct mdb_dlist_s {
    struct mdb_dlist_s *prev;
    struct mdb_dlist_s *next;
} mdb_dlist_t;




#endif /* __MDB_LIST_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
