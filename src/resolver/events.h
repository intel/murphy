#ifndef __MURPHY_RESOLVER_EVENTS_H__
#define __MURPHY_RESOLVER_EVENTS_H__

/*
 * resolver-related events
 */

enum {
    RESOLVER_UPDATE_STARTED = 0,
    RESOLVER_UPDATE_FAILED,
    RESOLVER_UPDATE_DONE
};


int emit_resolver_event(int event, const char *target, int level);


#endif /* __MURPHY_RESOLVER_EVENTS_H__ */
