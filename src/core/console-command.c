

/*
 * top-level console commands
 */


#define NPRINT(mc, fmt, args...) fprintf(mc->stdout, fmt , ## args)
#define EPRINT(mc, fmt, args...) fprintf(mc->stderr, fmt , ## args)


#define DOTS "........................................................"	\
	     "......................."

static void help_overview(mrp_console_t *mc)
{
    mrp_console_group_t *grp;
    mrp_console_cmd_t   *cmd;
    mrp_list_hook_t     *p, *n;
    int                  i, l, dend;
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
	NPRINT(mc, "The following commands are available in group '%s':\n",
	       grp->name);
	NPRINT(mc, "\n");

	for (i = 0, cmd = grp->commands; i < grp->ncommand; i++, cmd++) {
	    NPRINT(mc, "Command %s (%s):\n", cmd->name, cmd->summary);
	    NPRINT(mc, "\n");
	    NPRINT(mc, "%s\n", cmd->description);
	    NPRINT(mc, "\n");
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


#define HELP_HELP        "Give a help on a command group or a command."
#define HELP_DESCRIPTION "Give general help or help on a specific command."

static void cmd_help(mrp_console_t *mc, void *user_data, int argc, char **argv)
{
    console_t *c = (console_t *)mc;
    char      *ha[2];
    
    MRP_UNUSED(c);

    switch (argc) {
    case 1:
	help_overview(mc);
	break;
	
    case 2:
	help_group(mc, argv[1]);
	/*fprintf(mc->stdout, "Help on command '%s'.\n", argv[1]);*/
	break;
	
    case 3:
	/*help_command(mc, argv[1], argv[2]);*/
	fprintf(mc->stdout, "Help for command '%s/%s'.\n", argv[1], argv[2]);
	break;
	
    default:
	ha[0] = "help";
	ha[1] = "help";
	fprintf(mc->stderr, "help: invalid arguments (%d).\n", argc);
	fflush(mc->stderr);
	cmd_help(mc, user_data, 2, ha);
    }
}


#define EXIT_HELP        "Exit from a command group or the console."
#define EXIT_DESCRIPTION "Exit current console mode, or close the console."

static void cmd_exit(mrp_console_t *mc, void *user_data, int argc, char **argv)
{
    console_t *c = (console_t *)mc;
    char      *ha[2];

    switch (argc) {
    case 1:
	if (c->grp != NULL)
	    c->grp = NULL;
	else {
	    fprintf(mc->stdout, "Goodbye....\n");
	    mrp_destroy_console(mc);
	}
	break;
	
    case 2:
	if (!strcmp(argv[1], "console")) {
	    mrp_destroy_console(mc);
	    break;
	}
	/* intentional fall-through */
	
    default:
	ha[0] = "help";
	ha[1] = "exit";
	fprintf(mc->stderr, "exit: invalid arguments\n");
	cmd_help(mc, user_data, 2, ha);
    }
}


MRP_CONSOLE_GROUP(builtin_cmd_group, "", NULL, {
    MRP_PARSED_CMD("help", HELP_HELP, HELP_DESCRIPTION, cmd_help),
    MRP_PARSED_CMD("exit", EXIT_HELP, EXIT_DESCRIPTION, cmd_exit)
});
