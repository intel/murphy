#include <stdlib.h>
#include <murphy/common/debug.h>

/* core/console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 77, .func = "console_setup" },
    { .line = 86, .func = "console_cleanup" },
    { .line = 102, .func = "mrp_create_console" },
    { .line = 155, .func = "purge_destroyed" },
    { .line = 177, .func = "mrp_destroy_console" },
    { .line = 199, .func = "check_destroy" },
    { .line = 205, .func = "mrp_console_printf" },
    { .line = 218, .func = "mrp_set_console_prompt" },
    { .line = 245, .func = "find_group" },
    { .line = 271, .func = "find_command" },
    { .line = 288, .func = "mrp_console_add_group" },
    { .line = 299, .func = "mrp_console_del_group" },
    { .line = 310, .func = "mrp_console_add_core_group" },
    { .line = 321, .func = "mrp_console_del_core_group" },
    { .line = 332, .func = "input_evt" },
    { .line = 476, .func = "disconnected_evt" },
    { .line = 482, .func = "complete_evt" },
    { .line = 499, .func = "cookie_write" },
    { .line = 515, .func = "cookie_close" },
    { .line = 523, .func = "console_fopen" },
    { .line = 542, .func = "register_commands" },
    { .line = 548, .func = "unregister_commands" },
    { .line = 564, .func = "get_next_line" },
    { .line = 601, .func = "skip_whitespace" },
    { .line = 608, .func = "get_next_token" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "core/console.c",
    .info = info_0
};

/* core/context.c */
static mrp_debug_info_t info_1[] = {
    { .line = 7, .func = "mrp_context_create" },
    { .line = 26, .func = "mrp_context_destroy" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "core/context.c",
    .info = info_1
};

/* core/plugin.c */
static mrp_debug_info_t info_2[] = {
    { .line = 26, .func = "mrp_register_builtin_plugin" },
    { .line = 48, .func = "mrp_plugin_exists" },
    { .line = 66, .func = "check_plugin_version" },
    { .line = 84, .func = "check_plugin_singleton" },
    { .line = 96, .func = "mrp_load_plugin" },
    { .line = 216, .func = "load_plugin_cb" },
    { .line = 238, .func = "mrp_load_all_plugins" },
    { .line = 260, .func = "mrp_request_plugin" },
    { .line = 277, .func = "mrp_unload_plugin" },
    { .line = 323, .func = "mrp_start_plugins" },
    { .line = 347, .func = "mrp_start_plugin" },
    { .line = 356, .func = "mrp_stop_plugin" },
    { .line = 373, .func = "find_plugin_instance" },
    { .line = 390, .func = "find_plugin" },
    { .line = 406, .func = "open_dynamic" },
    { .line = 451, .func = "open_builtin" },
    { .line = 467, .func = "parse_plugin_arg" },
    { .line = 517, .func = "parse_plugin_args" },
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
