#ifndef __MURPHY_CONTEXT_H__
#define __MURPHY_CONTEXT_H__

#include <stdbool.h>

#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>

typedef struct {
    /* logging settings, path configuration, etc. */
    int         log_mask;                  /* what to log */
    const char *log_target;                /* and where to log to */

    const char *config_file;               /* configuration file */
    const char *config_dir;                /* plugin configuration directory */
    const char *plugin_dir;                /* plugin directory */
    bool        foreground;                /* whether to stay in foreground*/

    /* actual runtime context data */
    mrp_mainloop_t  *ml;                   /* mainloop */
    mrp_list_hook_t  plugins;              /* list of loaded plugins */
} mrp_context_t;

/** Create a new murphy context. */
mrp_context_t *mrp_context_create(void);

/** Destroy an existing murphy context. */
void mrp_context_destroy(mrp_context_t *c);


#endif /* __MURPHY_CONTEXT_H__ */
