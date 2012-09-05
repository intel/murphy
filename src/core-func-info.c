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
    { .line = 248, .func = "mrp_emit_event" },
    { .line = 270, .func = "mrp_set_events" },
    { .line = 287, .func = "mrp_set_named_events" },
    { .line = 308, .func = "event_count" },
    { .line = 316, .func = "lowest_bit" },
    { .line = 324, .func = "single_event" },
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
    { .line = 64, .func = "emit_plugin_event" },
    { .line = 76, .func = "mrp_register_builtin_plugin" },
    { .line = 98, .func = "mrp_plugin_exists" },
    { .line = 116, .func = "check_plugin_version" },
    { .line = 134, .func = "check_plugin_singleton" },
    { .line = 146, .func = "mrp_load_plugin" },
    { .line = 272, .func = "load_plugin_cb" },
    { .line = 294, .func = "mrp_load_all_plugins" },
    { .line = 316, .func = "mrp_request_plugin" },
    { .line = 333, .func = "mrp_unload_plugin" },
    { .line = 384, .func = "mrp_start_plugins" },
    { .line = 420, .func = "mrp_start_plugin" },
    { .line = 429, .func = "mrp_stop_plugin" },
    { .line = 448, .func = "find_plugin_instance" },
    { .line = 465, .func = "find_plugin" },
    { .line = 481, .func = "open_dynamic" },
    { .line = 526, .func = "open_builtin" },
    { .line = 542, .func = "parse_plugin_arg" },
    { .line = 592, .func = "parse_plugin_args" },
    { .line = 649, .func = "export_plugin_methods" },
    { .line = 667, .func = "remove_plugin_methods" },
    { .line = 686, .func = "import_plugin_methods" },
    { .line = 704, .func = "release_plugin_methods" },
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
    { .line = 158, .func = "mrp_execute_script" },
    { .line = 167, .func = "mrp_print_value" },
    { .line = 197, .func = "mrp_create_context_table" },
    { .line = 223, .func = "mrp_destroy_context_table" },
    { .line = 235, .func = "lookup_context_var" },
    { .line = 249, .func = "mrp_declare_context_variable" },
    { .line = 294, .func = "mrp_push_context_frame" },
    { .line = 314, .func = "mrp_pop_context_frame" },
    { .line = 346, .func = "get_context_id" },
    { .line = 352, .func = "get_context_value" },
    { .line = 375, .func = "set_context_value" },
    { .line = 422, .func = "set_context_values" },
    { .line = 436, .func = "mrp_get_context_id" },
    { .line = 448, .func = "mrp_get_context_value" },
    { .line = 455, .func = "mrp_set_context_value" },
    { .line = 462, .func = "mrp_get_context_value_by_name" },
    { .line = 469, .func = "mrp_set_context_value_by_name" },
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
