#ifndef __MURPHY_CONFIG_H__
#define __MURPHY_CONFIG_H__

#include <murphy/common/list.h>
#include <murphy/core/context.h>

#ifndef MRP_DEFAULT_CONFIG_DIR
#    define MRP_DEFAULT_CONFIG_DIR  "/etc/murphy"
#endif

#ifndef MRP_DEFAULT_CONFIG_FILE
#    define MRP_DEFAULT_CONFIG_FILE MRP_DEFAULT_CONFIG_DIR"/murphy.conf"
#endif

/*
 * command line processing
 */

/** Parse the command line and update context accordingly. */
int mrp_parse_cmdline(mrp_context_t *ctx, int argc, char **argv);


/*
 * configuration file processing
 */

#define MRP_CFG_MAXLINE 4096             /* input line length limit */
#define MRP_CFG_MAXARGS   64             /* command argument limit */

/* configuration keywords */
#define MRP_KEYWORD_LOAD    "load-plugin"
#define MRP_KEYWORD_TRYLOAD "try-load-plugin"
#define MRP_KEYWORD_AS      "as"
#define MRP_KEYWORD_IF      "if"
#define MRP_KEYWORD_ELSE    "else"
#define MRP_KEYWORD_END     "end"
#define MRP_KEYWORD_EXISTS  "plugin-exists"
#define MRP_KEYWORD_SETCFG  "set"
#define MRP_KEYWORD_ERROR   "error"
#define MRP_KEYWORD_WARNING "warning"
#define MRP_KEYWORD_INFO    "info"
#define MRP_START_COMMENT   '#'

/* known configuration variables for 'set' command */
#define MRP_CFGVAR_RESOLVER "resolver-ruleset"

typedef struct {
    mrp_list_hook_t actions;
} mrp_cfgfile_t;


/** Parse the given configuration file. */
mrp_cfgfile_t *mrp_parse_cfgfile(const char *path);

/** Execute the commands of the given parsed configuration file. */
int mrp_exec_cfgfile(mrp_context_t *ctx, mrp_cfgfile_t *cfg);

/** Free the given parsed configuration file. */
void mrp_free_cfgfile(mrp_cfgfile_t *cfg);

#endif /* __MURPHY_CONFIG_H__ */
