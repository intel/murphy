/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#define DOTS "........................................................" \
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
#define HELP_DESCRIPTION                                                  \
    "Give general help or help on a specific command group or a\n"        \
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
#define EXIT_DESCRIPTION                                                  \
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
                          EXIT_SYNTAX, EXIT_SUMMARY, EXIT_DESCRIPTION),
});
