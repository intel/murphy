#include <murphy/core/event.h>

#include "resolver.h"
#include "events.h"



MRP_REGISTER_EVENTS(events,
                    { MRP_RESOLVER_EVENT_STARTED, RESOLVER_UPDATE_STARTED },
                    { MRP_RESOLVER_EVENT_FAILED , RESOLVER_UPDATE_FAILED  },
                    { MRP_RESOLVER_EVENT_DONE   , RESOLVER_UPDATE_DONE    });


int emit_resolver_event(int event, const char *target, int level)
{
    uint16_t ttarget = MRP_RESOLVER_TAG_TARGET;
    uint16_t tlevel  = MRP_RESOLVER_TAG_LEVEL;

    return mrp_emit_event(events[event].id,
                          MRP_MSG_TAG_STRING(ttarget, target),
                          MRP_MSG_TAG_UINT32(tlevel , level),
                          MRP_MSG_END);
}
