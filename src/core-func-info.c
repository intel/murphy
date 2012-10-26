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

/* core/event.c */
static mrp_debug_info_t info_2[] = {
    { .line = 45, .func = "init_watch_lists" },
    { .line = 57, .func = "mrp_add_event_watch" },
    { .line = 129, .func = "delete_watch" },
    { .line = 137, .func = "purge_deleted" },
    { .line = 149, .func = "mrp_del_event_watch" },
    { .line = 160, .func = "mrp_get_event_id" },
    { .line = 188, .func = "mrp_get_event_name" },
    { .line = 202, .func = "mrp_emit_event_msg" },
    { .line = 247, .func = "mrp_emit_event" },
    { .line = 269, .func = "mrp_set_events" },
    { .line = 286, .func = "mrp_set_named_events" },
    { .line = 307, .func = "event_count" },
    { .line = 315, .func = "lowest_bit" },
    { .line = 323, .func = "single_event" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "core/event.c",
    .info = info_2
};

/* core/method.c */
static mrp_debug_info_t info_3[] = {
    { .line = 32, .func = "create_method_table" },
    { .line = 50, .func = "destroy_method_table" },
    { .line = 57, .func = "free_method" },
    { .line = 67, .func = "alloc_method" },
    { .line = 94, .func = "create_method_list" },
    { .line = 122, .func = "free_method_list" },
    { .line = 141, .func = "purge_method_list" },
    { .line = 149, .func = "lookup_method_list" },
    { .line = 162, .func = "check_signatures" },
    { .line = 173, .func = "lookup_method" },
    { .line = 203, .func = "find_method" },
    { .line = 248, .func = "export_method" },
    { .line = 268, .func = "remove_method" },
    { .line = 293, .func = "mrp_export_method" },
    { .line = 327, .func = "mrp_remove_method" },
    { .line = 335, .func = "mrp_import_method" },
    { .line = 375, .func = "mrp_release_method" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_3 = {
    .file = "core/method.c",
    .info = info_3
};

/* core/plugin.c */
static mrp_debug_info_t info_4[] = {
    { .line = 93, .func = "emit_plugin_event" },
    { .line = 105, .func = "mrp_register_builtin_plugin" },
    { .line = 127, .func = "mrp_plugin_exists" },
    { .line = 145, .func = "check_plugin_version" },
    { .line = 163, .func = "check_plugin_singleton" },
    { .line = 175, .func = "mrp_load_plugin" },
    { .line = 301, .func = "load_plugin_cb" },
    { .line = 323, .func = "mrp_load_all_plugins" },
    { .line = 345, .func = "mrp_request_plugin" },
    { .line = 362, .func = "mrp_unload_plugin" },
    { .line = 413, .func = "mrp_start_plugins" },
    { .line = 449, .func = "mrp_start_plugin" },
    { .line = 458, .func = "mrp_stop_plugin" },
    { .line = 477, .func = "find_plugin_instance" },
    { .line = 494, .func = "find_plugin" },
    { .line = 510, .func = "open_dynamic" },
    { .line = 555, .func = "open_builtin" },
    { .line = 571, .func = "parse_plugin_arg" },
    { .line = 621, .func = "parse_plugin_args" },
    { .line = 678, .func = "export_plugin_methods" },
    { .line = 696, .func = "remove_plugin_methods" },
    { .line = 715, .func = "import_plugin_methods" },
    { .line = 733, .func = "release_plugin_methods" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_4 = {
    .file = "core/plugin.c",
    .info = info_4
};

/* core/scripting.c */
static mrp_debug_info_t info_5[] = {
    { .line = 62, .func = "mrp_register_interpreter" },
    { .line = 70, .func = "unregister_interpreter" },
    { .line = 76, .func = "mrp_unregister_interpreter" },
    { .line = 93, .func = "mrp_lookup_interpreter" },
    { .line = 108, .func = "mrp_create_script" },
    { .line = 138, .func = "mrp_destroy_script" },
    { .line = 149, .func = "mrp_compile_script" },
    { .line = 158, .func = "mrp_prepare_script" },
    { .line = 167, .func = "mrp_execute_script" },
    { .line = 176, .func = "mrp_print_value" },
    { .line = 206, .func = "mrp_create_context_table" },
    { .line = 232, .func = "mrp_destroy_context_table" },
    { .line = 244, .func = "lookup_context_var" },
    { .line = 258, .func = "mrp_declare_context_variable" },
    { .line = 303, .func = "mrp_push_context_frame" },
    { .line = 323, .func = "mrp_pop_context_frame" },
    { .line = 355, .func = "get_context_id" },
    { .line = 361, .func = "get_context_value" },
    { .line = 384, .func = "set_context_value" },
    { .line = 431, .func = "set_context_values" },
    { .line = 445, .func = "mrp_get_context_id" },
    { .line = 457, .func = "mrp_get_context_value" },
    { .line = 464, .func = "mrp_set_context_value" },
    { .line = 471, .func = "mrp_get_context_value_by_name" },
    { .line = 478, .func = "mrp_set_context_value_by_name" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_5 = {
    .file = "core/scripting.c",
    .info = info_5
};

/* table of all files */
static mrp_debug_file_t *debug_files[] = {
    &file_0,
    &file_1,
    &file_2,
    &file_3,
    &file_4,
    &file_5,
    NULL
};

#include <murphy/common/debug-auto-register.c>
