
#define DOTS "........................................................"	\
	     "......................."

#define NPRINT(mc, fmt, args...) fprintf(mc->stdout, fmt , ## args)
#define EPRINT(mc, fmt, args...) fprintf(mc->stderr, fmt , ## args)



/*
 * top-level console commands
 */


static void get_string_lengthes(mrp_console_t *mc,
				size_t *nmaxp, size_t *smaxp, size_t *tmaxp)
{
    mrp_console_group_t *grp;
    mrp_console_cmd_t   *cmd;
    mrp_list_hook_t     *p, *n;
    int                  i;
    size_t               nlen, slen, tmax, nmax, smax;

    tmax = nmax = smax = 0;    

    mrp_list_foreach(&mc->ctx->cmd_groups, p, n) {
	grp = mrp_list_entry(p, typeof(*grp), hook);

	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    nlen = strlen(cmd->name);
	    slen = strlen(cmd->summary);
	    nmax = MRP_MAX(nmax, nlen);
	    smax = MRP_MAX(smax, slen);
	    tmax = MRP_MAX(tmax, nlen + slen);
	}
    }
    
    mrp_list_foreach(&core_groups, p, n) {
	grp = mrp_list_entry(p, typeof(*grp), hook);

	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    nlen = strlen(cmd->name);
	    slen = strlen(cmd->summary);
	    nmax = MRP_MAX(nmax, nlen);
	    smax = MRP_MAX(smax, slen);
	    tmax = MRP_MAX(tmax, nlen + slen);
	}
    }
    
    *nmaxp = nmax;
    *smaxp = smax;
    *tmaxp = tmax;
}


static void help_overview(mrp_console_t *mc)
{
    mrp_console_group_t *grp;
    mrp_console_cmd_t   *cmd;
    mrp_list_hook_t     *p, *n;
    int                  i, l, dend;
    size_t               tmax, nmax, smax;

    get_string_lengthes(mc, &nmax, &smax, &tmax);

    if (4 + 2 + 2 + tmax < 79) {
	dend = 79 - smax - 2;
    }
    else
	dend = tmax + 20;

    NPRINT(mc, "The following commands are available:\n\n");

    mrp_list_foreach(&mc->ctx->cmd_groups, p, n) {
	grp = mrp_list_entry(p, typeof(*grp), hook);

	if (*grp->name)
	    NPRINT(mc, "  commands in group '%s':\n", grp->name);
	else
	    NPRINT(mc, "  general commands:\n");
		
	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    NPRINT(mc, "    %s  %n", cmd->name, &l);
	    NPRINT(mc, "%*.*s %s\n", dend - l, dend - l, DOTS, cmd->summary);
	}
	
	NPRINT(mc, "\n");
    }

    mrp_list_foreach(&core_groups, p, n) {
	grp = mrp_list_entry(p, typeof(*grp), hook);

	if (*grp->name)
	    NPRINT(mc, "  commands in group '%s':\n", grp->name);
	else
	    NPRINT(mc, "  general commands:\n");
		
	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    NPRINT(mc, "    %s  %n", cmd->name, &l);
	    NPRINT(mc, "%*.*s %s\n", dend - l, dend - l, DOTS, cmd->summary);
	}
	
	NPRINT(mc, "\n");
    }
}


static void help_group(mrp_console_t *mc, const char *name)
{
    mrp_console_group_t *grp;
    mrp_console_cmd_t   *cmd;
    mrp_list_hook_t     *p, *n;
    const char          *t;
    int                  i;

    grp = find_group(mc->ctx, name);
    
    if (grp != NULL) {
	if (grp->descr != NULL)
	    NPRINT(mc, "%s\n", grp->descr);
	
	NPRINT(mc, "The following commands are available:\n");
	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    NPRINT(mc, "- %s (syntax: %s%s%s)\n\n", cmd->name,
		   grp->name ? grp->name : "", grp->name ? " " : "",
		   cmd->syntax);
	    NPRINT(mc, "%s\n", cmd->description);
	}
    }	
    else {
	EPRINT(mc, "Command group '%s' does not exist.\n", name);
	EPRINT(mc, "The existing groups are: ");
	t = "";
	mrp_list_foreach(&mc->ctx->cmd_groups, p, n) {
	    grp = mrp_list_entry(p, typeof(*grp), hook);
	    if (*grp->name) {
		EPRINT(mc, "%s'%s'", t, grp->name);
		t = ", ";
	    }
	}
	EPRINT(mc, ".\n");
    }
	

}


#define HELP_SYNTAX      "help [group|command]"
#define HELP_SUMMARY     "print help on a command group or a command"
#define HELP_DESCRIPTION						\
    "Give general help or help on a specific command group or a\n"	\
    "single command.\n"

static void cmd_help(mrp_console_t *mc, void *user_data, int argc, char **argv)
{
    console_t *c = (console_t *)mc;
    char      *ha[2];
    
    MRP_UNUSED(c);

    switch (argc) {
    case 2:
	help_overview(mc);
	break;
	
    case 3:
	help_group(mc, argv[2]);
	break;
	
    case 4:
	fprintf(mc->stdout, "Help for command '%s/%s'.\n", argv[2], argv[3]);
	break;
	
    default:
	ha[0] = "help";
	ha[1] = "help";
	fprintf(mc->stderr, "help: invalid arguments (%d).\n", argc);
	fflush(mc->stderr);
	cmd_help(mc, user_data, 2, ha);
    }
}


#define EXIT_SYNTAX      "exit"
#define EXIT_SUMMARY     "exit from a command group or the console"
#define EXIT_DESCRIPTION					\
    "Exit current console mode, or close the console.\n"


static void cmd_exit(mrp_console_t *mc, void *user_data, int argc, char **argv)
{
    console_t *c = (console_t *)mc;
    char      *ha[2];

    switch (argc) {
    case 2:
	if (c->grp != NULL) {
	    if (c->cmd != NULL)
		c->cmd = NULL;
	    else
		c->grp = NULL;
	}
	else {
	close_console:
	    fprintf(mc->stdout, "Bye.\n");
	    mrp_destroy_console(mc);
	}
	break;
	
    case 3:
	if (!strcmp(argv[2], "console"))
	    goto close_console;
	/* intentional fall-through */

    default:
	ha[0] = "help";
	ha[1] = "exit";
	fprintf(mc->stderr, "exit: invalid arguments\n");
	cmd_help(mc, user_data, 2, ha);
    }
}


MRP_CONSOLE_GROUP(builtin_cmd_group, "", NULL, NULL, {
	MRP_TOKENIZED_CMD("help", cmd_help, FALSE,
			  HELP_SYNTAX, HELP_SUMMARY, HELP_DESCRIPTION),
	MRP_TOKENIZED_CMD("exit", cmd_exit, FALSE,
			  EXIT_SYNTAX, EXIT_SUMMARY, EXIT_DESCRIPTION)
});


/*
 * debug commands
 */

static void debug_enable(mrp_console_t *c, void *user_data,
			 int argc, char **argv)
{
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    MRP_UNUSED(user_data);

    mrp_debug_enable(TRUE);

    mrp_console_printf(c, "Debugging is now enabled.\n");
}


static void debug_disable(mrp_console_t *c, void *user_data,
			  int argc, char **argv)
{
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    MRP_UNUSED(user_data);

    mrp_debug_enable(FALSE);
    
    mrp_console_printf(c, "Debugging is now disabled.\n");
}


static void debug_show(mrp_console_t *c, void *user_data,
		       int argc, char **argv)
{
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_debug_dump_config(c->stdout);
}

#define __GNU_SOURCE
#include <link.h>
#include <elf.h>

typedef struct {
    mrp_console_t *c;
} list_data_t;


#undef __DUMP_ELF_INFO__

#ifdef __DUMP_ELF_IFDO__
static const char *segment_type(uint32_t type)
{
#define T(type) case type: return #type
    switch (type) {
	T(PT_NULL);
	T(PT_LOAD);
	T(PT_DYNAMIC);
	T(PT_INTERP);
	T(PT_NOTE);
	T(PT_SHLIB);
	T(PT_PHDR);
	T(PT_TLS);
	T(PT_NUM);
	T(PT_LOOS);
	T(PT_GNU_EH_FRAME);
	T(PT_GNU_STACK);
	T(PT_GNU_RELRO);
	T(PT_LOPROC);
	T(PT_HIPROC);
    default:
	return "unknown";
    }
}


static const char *segment_flags(uint32_t flags)
{
    static char buf[4];
    
    buf[0] = (flags & PF_R) ? 'r' : '-';
    buf[1] = (flags & PF_W) ? 'w' : '-';
    buf[2] = (flags & PF_X) ? 'x' : '-';
    buf[3] = '\0';
    
    return buf;
}

#endif /* __DUMP_ELF_INFO__ */

static int list_cb(struct dl_phdr_info *info, size_t size, void *data)
{
#define P(fmt, args...) fprintf(c->stdout, fmt , ## args)
#define RELOC(addr) (info->dlpi_addr + addr)
    
    mrp_console_t    *c = (mrp_console_t *)data;
    const ElfW(Phdr) *h;
    int               i;
    const char       *beg, *end, *s, *func;
    char              file[512], *p;
    int               line;

    MRP_UNUSED(size);

#ifdef __DUMP_ELF_INFO__
    P("%s (@%p)\n",
      info->dlpi_name && *info->dlpi_name ? info->dlpi_name : "<none>",
      info->dlpi_addr);
    P("  %d segments\n", info->dlpi_phnum);
#endif

    file[sizeof(file) - 1] = '\0';

    for (i = 0; i < info->dlpi_phnum; i++) {
	h = &info->dlpi_phdr[i];
#if __DUMP_ELF_INFO__
	P("  #%d:\n", i);
	P("       type: 0x%x (%s)\n", h->p_type, segment_type(h->p_type));
	P("     offset: 0x%lx\n", h->p_offset);
	P("      vaddr: 0x%lx (0x%lx)\n", h->p_vaddr, RELOC(h->p_vaddr));
	P("      paddr: 0x%lx (0x%lx)\n", h->p_paddr, RELOC(h->p_paddr));
	P("     filesz: 0x%lx\n", h->p_filesz);
	P("      memsz: 0x%lx\n", h->p_memsz);
	P("      flags: 0x%x (%s)\n", h->p_flags, segment_flags(h->p_flags));
	P("      align: 0x%lx\n", h->p_align);
#endif
	if (h->p_flags & PF_W)
	    continue;
	
	beg = (const char *)RELOC(h->p_vaddr);
	end = (const char *)beg + h->p_memsz;

#define PREFIX     "__DEBUG_SITE_"
#define PREFIX_LEN 13
	for (s = beg; s < end - PREFIX_LEN; s++) {
	    if (!strncmp(s, PREFIX, PREFIX_LEN)) {
		s += PREFIX_LEN;
		if (*s != '\0') {
		    strncpy(file, s, sizeof(file) - 1);
		    p = strchr(file, ':');

		    if (p != NULL) {
			*p = '\0';
			line = (int)strtoul(p + 1, NULL, 10);
			func = mrp_debug_site_function(file, line);
		    }
		    else
			func = NULL;
		    
		    if (func != NULL)
			P("  %s@%s\n", func, s);
		    else
			P("  %s\n", s);
		}
	    }
	}
    }

    return 0;
}


static void debug_list(mrp_console_t *c, void *user_data,
		       int argc, char **argv)
{
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    
    fprintf(c->stdout, "Available debug sites:\n");
    dl_iterate_phdr(list_cb, (void *)c);
}


static void debug_set(mrp_console_t *c, void *user_data,
		      int argc, char **argv)
{
    int i;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    for (i = 2; i < argc; i++)
	mrp_debug_set_config(argv[i]);
}


static void debug_reset(mrp_console_t *c, void *user_data,
			int argc, char **argv)
{
    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_debug_reset();
    
    mrp_console_printf(c, "Debugging configuration has been reset to default.");
}


#define DEBUG_GROUP_DESCRIPTION						\
    "Debugging commands provide fine-grained control over runtime\n"	\
    "debugging messages produced by the murphy daemon or any of the\n"	\
    "murphy plugins loaded. Each debug message that is generated by\n"	\
    "the standard murphy debug macro declares a debug site that can\n"	\
    "be turned on or off using debug rules. Debug rules come in two\n"	\
    "flavours, enabling and inhibiting. Enabling rules turn matching\n"	\
    "debug messages on, while inhibiting rules turn matching debug\n"	\
    "messages off. Debug rules are in one of the following formats:\n"	\
    "\n"								\
    "    func[=on|off]:       all messages from <func>\n"		\
    "    @file[=on|off]:      all messages in <file>\n"			\
    "    @file:line=[on|off]: messages at <file>:<line>\n"		\
    "    *[=on|off]:          all messages\n"				\
    "\n"								\
    "Filenames without a directory can match filenames with one.\n"	\
    "Enabling rules are evaluated before inhibiting rules. All debug\n"	\
    "messages are suppressed if debugging is disabled.\n"

#define ENABLE_SYNTAX       "enable"
#define ENABLE_SUMMARY      "enable debugging"
#define ENABLE_DESCRIPTION						  \
    "Enable debugging globally. Unless debugging is enabled, all debug\n" \
    "messages are suppressed, even those for which matching enabling\n"	  \
    "rules exist.\n"

#define DISABLE_SYNTAX      "disable"
#define DISABLE_SUMMARY     "disable debugging"
#define DISABLE_DESCRIPTION						   \
    "Disable debugging globally. Unless debugging is enabled, all debug\n" \
    "messages are suppressed, even those for which matching enabling\n"    \
    "rules exist.\n"

#define SHOW_SYNTAX         "show"
#define SHOW_SUMMARY        "show debugging configuration"
#define SHOW_DESCRIPTION						  \
    "Show the current debugging configuration, and debug rules.\n"

#define SET_SYNTAX          "set [+|-]rule"
#define SET_SUMMARY         "change debugging rules"
#define SET_DESCRIPTION							\
    "Install a new or remove an existing debugging rule. Debug rules\n" \
    "are in one of the following formats:\n"				\
    "\n"								\
    "    func[=on|off]:       all messages from <func>\n"		\
    "    @file[=on|off]:      all messages in <file>\n"			\
    "    @file:line=[on|off]: messages at <file>:<line>\n"		\
    "    *[=on|off]:          all messages\n"				\

#define RESET_SYNTAX        "reset"
#define RESET_SUMMARY       "reset debugging configuration"
#define RESET_DESCRIPTION						\
    "Reset the debugging configuration to the defaults. This will turn" \
    "disable debugging globally and flush all debugging rules.\n"

#define LIST_SYNTAX         "list"
#define LIST_SUMMARY        "list known debug sites"
#define LIST_DESCRIPTION						\
    "List all known debug sites of the murphy daemon itself as\n"	\
    "as well as from any loaded murphy plugins.\n"

MRP_CORE_CONSOLE_GROUP(debug_group, "debug", DEBUG_GROUP_DESCRIPTION, NULL, {
	MRP_TOKENIZED_CMD("enable", debug_enable, FALSE,
			  ENABLE_SYNTAX, ENABLE_SUMMARY, ENABLE_DESCRIPTION),
	MRP_TOKENIZED_CMD("disable", debug_disable, FALSE,
			  DISABLE_SYNTAX, DISABLE_SUMMARY, DISABLE_DESCRIPTION),
	MRP_TOKENIZED_CMD("show", debug_show, FALSE,
			  SHOW_SYNTAX, SHOW_SUMMARY, SHOW_DESCRIPTION),
	MRP_TOKENIZED_CMD("set", debug_set, FALSE,
			  SET_SYNTAX, SET_SUMMARY, SET_DESCRIPTION),
	MRP_TOKENIZED_CMD("reset", debug_reset, FALSE,
			  RESET_SYNTAX, RESET_SUMMARY, RESET_DESCRIPTION),
	MRP_TOKENIZED_CMD("list", debug_list, FALSE,
			  LIST_SYNTAX, LIST_SUMMARY, LIST_DESCRIPTION)
});

