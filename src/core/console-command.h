#ifndef __MURPHY_CONSOLE_COMMAND_H__
#define __MURPHY_CONSOLE_COMMAND_H__


/**
 * Convenience macro for declaring console commands.
 *
 * Here is how you can use these macros to declare a group of commands
 * for instance from/for your plugin:
 *
 * #define DESCRIPTION1 "Test command 1 (description of command 1)..."
 * #define HELP1        "Help for test command1...
 * #define DESCRIPTION2 "Test command 2 (description of command 2)..."
 * #define HELP2        "Help for test command2...
 * #define DESCRIPTION3 "Test command 3 (description of command 3)..."
 * #define HELP3        "Help for test command3...
 * #define DESCRIPTION4 "Test command 4 (description of command 4)..."
 * #define HELP4        "Help for test command4...
 *
 * static void cmd1_cb(mrp_console_t *c, void *user_data, int argc, char **argv)
 * {
 *     int i;
 *
 *     for (i = 0; i < argc, i++) {
 *         mrp_console_printf(c, "%s(): arg #%d: '%s'\n", __FUNCTION__,
 *                            i, argv[i]);
 *     }
 * }
 *
 * ...
 *
 * MRP_CONSOLE_GROUP(test_cmd_group, "test", NULL, {
 *     MRP_PARSED_CMD("cmd1", CMD1_HELP, CMD1_DESCRIPTION, cmd1_cb),
 *     MRP_PARSED_CMD("cmd2", CMD2_HELP, CMD2_DESCRIPTION, cmd2_cb),
 *     MRP_PARSED_CMD("cmd3", CMD3_HELP, CMD3_DESCRIPTION, cmd3_cb),
 *     MRP_PARSED_CMD("cmd4", CMD4_HELP, CMD4_DESCRIPTION, cmd4_cb)
 * });
 *
 * ...
 *
 * static int plugin_init(mrp_plugin_t *plugin)
 * {
 *     ...
 *     mrp_add_console_group(plugin->ctx, &test_cmd_group);
 *     ...
 * }
 *
 *
 * static int plugin_exit(mrp_plugin_t *plugin)
 * {
 *     ...
 *     mrp_del_console_group(plugin->ctx, &test_cmd_group);
 *     ...
 * }
 * 
 */

#define MRP_CONSOLE_COMMANDS(_var, ...)			\
    static mrp_console_cmd_t _var[] = __VA_ARGS__

#define MRP_CONSOLE_GROUP(_var, _name, _data, ...)	   \
    MRP_CONSOLE_COMMANDS(_var##_cmds, __VA_ARGS__);	   \
    static mrp_console_group_t _var = {			   \
        .name      = _name,                                \
        .user_data = _data,                                \
        .commands  = _var##_cmds,                          \
        .ncommand  = MRP_ARRAY_SIZE(_var##_cmds),          \
        .hook      = MRP_LIST_INIT(_var.hook),		   \
    };

#define MRP_PARSED_CMD(_name, _summ, _descr, _cb) {			\
	.name = _name,							\
	.summary = _summ,						\
	.description = _descr,					        \
	.tok = _cb							\
    }

#define MRP_RAW_CMD(_name, _summ, _descr, _cb) {			\
	.name = _name,							\
	.summary = _summ,						\
	.description = _descr,					        \
        .flags = MRP_CONSOLE_RAWINPUT,				        \
	.raw = _cb							\
    }

typedef struct mrp_console_s mrp_console_t;


/*
 * console/command flags
 */

typedef enum {
    MRP_CONSOLE_TOKENIZE = 0x0,          /* callback wants tokenized input */
    MRP_CONSOLE_RAWINPUT = 0x1,          /* callback wants 'raw' input */
} mrp_console_flag_t;


/*
 * a console command
 */

typedef struct {
    const char         *name;            /* command name */
    const char         *summary;         /* short help */
    const char         *description;     /* long command description */
    mrp_console_flag_t  flags;           /* command flags */
    union {                              /* tokenized or raw input cb */
	int    (*raw)(mrp_console_t *c, void *user_data, char *input);
	void   (*tok)(mrp_console_t *c, void *user_data, int argc, char **argv);
    };
} mrp_console_cmd_t;


/*
 * a console command group
 */

typedef struct {
    const char        *name;             /* command group name/prefix */
    void              *user_data;        /* opaque callback data */
    mrp_console_cmd_t *commands;         /* commands in this group */
    int                ncommand;         /* number of commands */
    mrp_list_hook_t    hook;             /* to list of command groups */
} mrp_console_group_t;


/** Register a console command group. */
int mrp_add_console_group(mrp_context_t *ctx, mrp_console_group_t *group);

/** Unregister a console command group. */
int mrp_del_console_group(mrp_context_t *ctx, mrp_console_group_t *group);


#endif /* __MURPHY_CONSOLE_COMMAND_H__ */
