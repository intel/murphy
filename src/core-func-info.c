#include <stdlib.h>
#include <murphy/common/debug.h>

/* core/console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 106, .func = "console_setup" },
    { .line = 115, .func = "console_cleanup" },
    { .line = 131, .func = "mrp_create_console" },
    { .line = 184, .func = "purge_destroyed" },
    { .line = 206, .func = "mrp_destroy_console" },
    { .line = 228, .func = "check_destroy" },
    { .line = 234, .func = "mrp_console_printf" },
    { .line = 247, .func = "mrp_set_console_prompt" },
    { .line = 274, .func = "find_group" },
    { .line = 300, .func = "find_command" },
    { .line = 317, .func = "mrp_console_add_group" },
    { .line = 328, .func = "mrp_console_del_group" },
    { .line = 339, .func = "mrp_console_add_core_group" },
    { .line = 350, .func = "mrp_console_del_core_group" },
    { .line = 361, .func = "input_evt" },
    { .line = 505, .func = "disconnected_evt" },
    { .line = 511, .func = "complete_evt" },
    { .line = 528, .func = "cookie_write" },
    { .line = 544, .func = "cookie_close" },
    { .line = 552, .func = "console_fopen" },
    { .line = 571, .func = "register_commands" },
    { .line = 577, .func = "unregister_commands" },
    { .line = 593, .func = "get_next_line" },
    { .line = 630, .func = "skip_whitespace" },
    { .line = 637, .func = "get_next_token" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "core/console.c",
    .info = info_0
};

/* core/context.c */
static mrp_debug_info_t info_1[] = {
    { .line = 36, .func = "mrp_context_create" },
    { .line = 55, .func = "mrp_context_destroy" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "core/context.c",
    .info = info_1
};

/* core/plugin.c */
static mrp_debug_info_t info_2[] = {
    { .line = 55, .func = "mrp_register_builtin_plugin" },
    { .line = 77, .func = "mrp_plugin_exists" },
    { .line = 95, .func = "check_plugin_version" },
    { .line = 113, .func = "check_plugin_singleton" },
    { .line = 125, .func = "mrp_load_plugin" },
    { .line = 245, .func = "load_plugin_cb" },
    { .line = 267, .func = "mrp_load_all_plugins" },
    { .line = 289, .func = "mrp_request_plugin" },
    { .line = 306, .func = "mrp_unload_plugin" },
    { .line = 352, .func = "mrp_start_plugins" },
    { .line = 376, .func = "mrp_start_plugin" },
    { .line = 385, .func = "mrp_stop_plugin" },
    { .line = 402, .func = "find_plugin_instance" },
    { .line = 419, .func = "find_plugin" },
    { .line = 435, .func = "open_dynamic" },
    { .line = 480, .func = "open_builtin" },
    { .line = 496, .func = "parse_plugin_arg" },
    { .line = 546, .func = "parse_plugin_args" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "core/plugin.c",
    .info = info_2
};

/* table of all files */
static mrp_debug_file_t *debug_files[] = {
    &file_0,
    &file_1,
    &file_2,
    NULL
};


#include <murphy/common/debug-auto-register.c>
