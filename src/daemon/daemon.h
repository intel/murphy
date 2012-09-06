#ifndef __MURPHY_DAEMON_H__
#define __MURPHY_DAEMON_H__

#include <murphy/core/event.h>

/*
 * names of daemon-related events we emit
 */

#define MRP_DAEMON_LOADING  "daemon-loading"    /* loading configuration */
#define MRP_DAEMON_STARTING "daemon-starting"   /* starting up (plugins) */
#define MRP_DAEMON_RUNNING  "daemon-running"    /* about to run mainloop */
#define MRP_DAEMON_STOPPING "daemon-stopping"   /* shutting down */

#endif /* __MURPHY_DAEMON_H__ */
