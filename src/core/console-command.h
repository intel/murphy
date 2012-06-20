#ifndef __MURPHY_CONSOLE_COMMAND_H__
#define __MURPHY_CONSOLE_COMMAND_H__


/** Macro to declare an array of console commands. */
#define MRP_CONSOLE_COMMANDS(_var, ...)			\
    static mrp_console_cmd_t _var[] = __VA_ARGS__

/** Macro to declare a console command group. */
#define MRP_CONSOLE_GROUP(_var, _name, _descr, _data, ...)  \
    MRP_CONSOLE_COMMANDS(_var##_cmds, __VA_ARGS__);	    \
    static mrp_console_group_t _var = {			    \
        .name      = (char *)_name,			    \
	.descr     = _descr,				    \
        .user_data = _data,				    \
        .commands  = _var##_cmds,			    \
        .ncommand  = MRP_ARRAY_SIZE(_var##_cmds),	    \
        .hook      = MRP_LIST_INIT(_var.hook),		    \
    };

/** Macro to declare a console command that wants tokenized input. */
#define MRP_TOKENIZED_CMD(_name, _cb, _selectable, _syntax, _summ, _descr) { \
	.name        = _name,						\
	.syntax      = _syntax,					        \
	.summary     = _summ,						\
	.description = _descr,					        \
        .tok         = _cb,						\
        .flags       =							\
	              (_selectable ? MRP_CONSOLE_SELECTABLE : 0),	\
    }

#if 0 /* XXX TODO: implement handling of raw input mode commands */
/** Macro to declare a console command that wants a raw input. */
#define MRP_RAWINPUT_CMD(_name, _cb, _selectable, _syntax, _summ, _descr) { \
	.name        = _name,						\
	.syntax      = _syntax,					        \
	.summary     = _summ,						\
	.description = _descr,					        \
        .flags       = MRP_CONSOLE_RAWINPUT |				\
	              (_selectable ? MRP_CONSOLE_SELECTABLE : 0),	\
	.raw         = _cb						\
    }
#endif

typedef struct mrp_console_s mrp_console_t;


/*
 * console command flags
 */

typedef enum {
    MRP_CONSOLE_TOKENIZE   = 0x0,        /* wants tokenized input */
    MRP_CONSOLE_RAWINPUT   = 0x1,        /* wants raw input */
    MRP_CONSOLE_SELECTABLE = 0x2,        /* selectable as command mode */
} mrp_console_flag_t;


/*
 * a console command
 */

typedef struct {
    const char         *name;            /* command name */
    const char         *syntax;          /* command syntax */
    const char         *summary;         /* short help */
    const char         *description;     /* long command description */
    mrp_console_flag_t  flags;           /* command flags */
    union {                              /* tokenized or raw input cb */
	int    (*raw)(mrp_console_t *c, void *user_data, char *input);
	void   (*tok)(mrp_console_t *c, void *user_data, int argc, char **argv);
    };
} mrp_console_cmd_t;


/*
 * a group of console commands
 */

typedef struct {
    char              *name;             /* command group name/prefix */
    char              *descr;            /* group description */
    void              *user_data;        /* opaque callback data */
    mrp_console_cmd_t *commands;         /* commands in this group */
    int                ncommand;         /* number of commands */
    mrp_list_hook_t    hook;             /* to list of command groups */
} mrp_console_group_t;


/** Register a console command group. */
int mrp_console_add_group(mrp_context_t *ctx, mrp_console_group_t *group);

/** Unregister a console command group. */
int mrp_console_del_group(mrp_context_t *ctx, mrp_console_group_t *group);

/** Convenience macro to register a group of core commands. */
#define MRP_CORE_CONSOLE_GROUP(_var, _name, _descr, _data, ...)	\
    MRP_CONSOLE_GROUP(_var, _name, _descr, _data, __VA_ARGS__);	\
    								\
    static void _var##_register_core_group(void)		\
	__attribute__((constructor));				\
    								\
    static void __attribute__((constructor))			\
    _var##_register_core_group(void) {				\
	mrp_console_add_core_group(&_var);			\
    }								\
    								\
    static void __attribute__((destructor))			\
    _var##_unregister_core_group(void) {			\
	mrp_console_del_core_group(&_var);			\
    }								\
    struct mrp_allow_trailing_semicolon

/** Pre-register a group of core commands to register later to any context. */
int mrp_console_add_core_group(mrp_console_group_t *group);

/** Unregister a pre-registered group of core commands. */
int mrp_console_del_core_group(mrp_console_group_t *group);

#endif /* __MURPHY_CONSOLE_COMMAND_H__ */
