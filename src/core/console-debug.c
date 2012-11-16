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


/*
 * debug commands
 */

static void debug_enable(mrp_console_t *c, void *user_data,
                         int argc, char **argv)
{
    MRP_UNUSED(c);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    MRP_UNUSED(user_data);

    mrp_debug_enable(TRUE);

    printf("Debugging is now enabled.\n");
}


static void debug_disable(mrp_console_t *c, void *user_data,
                          int argc, char **argv)
{
    MRP_UNUSED(c);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);
    MRP_UNUSED(user_data);

    mrp_debug_enable(FALSE);

    printf("Debugging is now disabled.\n");
}


static void debug_show(mrp_console_t *c, void *user_data,
                       int argc, char **argv)
{
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_debug_dump_config(c->stdout);
}


static void debug_list(mrp_console_t *c, void *user_data,
                       int argc, char **argv)
{
    MRP_UNUSED(user_data);
    MRP_UNUSED(argc);
    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    fprintf(c->stdout, "Available debug sites:\n");
    mrp_debug_dump_sites(c->stdout, 4);
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

    printf("Debugging configuration has been reset to default.");
}


#define DEBUG_GROUP_DESCRIPTION                                           \
    "Debugging commands provide fine-grained control over runtime\n"      \
    "debugging messages produced by the murphy daemon or any of the\n"    \
    "murphy plugins loaded. Each debug message that is generated by\n"    \
    "the standard murphy debug macro declares a debug site that can\n"    \
    "be turned on or off using debug rules. Debug rules come in two\n"    \
    "flavours, enabling and inhibiting. Enabling rules turn matching\n"   \
    "debug messages on, while inhibiting rules turn matching debug\n"     \
    "messages off. Debug rules are in one of the following formats:\n"    \
    "\n"                                                                  \
    "    func[=on|off]:       all messages from <func>\n"                 \
    "    @file[=on|off]:      all messages in <file>\n"                   \
    "    @file:line=[on|off]: messages at <file>:<line>\n"                \
    "    *[=on|off]:          all messages\n"                             \
    "\n"                                                                  \
    "Filenames without a directory can match filenames with one.\n"       \
    "Enabling rules are evaluated before inhibiting rules. All debug\n"   \
    "messages are suppressed if debugging is disabled.\n"

#define ENABLE_SYNTAX       "enable"
#define ENABLE_SUMMARY      "enable debugging"
#define ENABLE_DESCRIPTION                                                \
    "Enable debugging globally. Unless debugging is enabled, all debug\n" \
    "messages are suppressed, even those for which matching enabling\n"   \
    "rules exist.\n"

#define DISABLE_SYNTAX      "disable"
#define DISABLE_SUMMARY     "disable debugging"
#define DISABLE_DESCRIPTION                                               \
    "Disable debugging globally. Unless debugging is enabled all debug\n" \
    "messages are suppressed, even those for which matching enabling\n"   \
    "rules exist.\n"

#define SHOW_SYNTAX         "show"
#define SHOW_SUMMARY        "show debugging configuration"
#define SHOW_DESCRIPTION                                                  \
    "Show the current debugging configuration, and debug rules.\n"

#define SET_SYNTAX          "set [+|-]rule"
#define SET_SUMMARY         "change debugging rules"
#define SET_DESCRIPTION                                                   \
    "Install a new or remove an existing debugging rule. Debug rules\n"   \
    "are in one of the following formats:\n"                              \
    "\n"                                                                  \
    "    func[=on|off]:       all messages from <func>\n"                 \
    "    @file[=on|off]:      all messages in <file>\n"                   \
    "    @file:line=[on|off]: messages at <file>:<line>\n"                \
    "    *[=on|off]:          all messages\n"                             \

#define RESET_SYNTAX        "reset"
#define RESET_SUMMARY       "reset debugging configuration"
#define RESET_DESCRIPTION                                                 \
    "Reset the debugging configuration to the defaults. This will turn"   \
    "disable debugging globally and flush all debugging rules.\n"

#define LIST_SYNTAX         "list"
#define LIST_SUMMARY        "list known debug sites"
#define LIST_DESCRIPTION                                                  \
    "List all known debug sites of the murphy daemon itself as\n"         \
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
