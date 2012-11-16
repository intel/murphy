#include <stdlib.h>
#include <murphy/common/debug.h>

/* core/console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 115, .func = "console_setup" },
    { .line = 124, .func = "console_cleanup" },
    { .line = 140, .func = "mrp_create_console" },
    { .line = 196, .func = "purge_destroyed" },
    { .line = 222, .func = "mrp_destroy_console" },
    { .line = 244, .func = "check_destroy" },
    { .line = 250, .func = "mrp_console_printf" },
    { .line = 263, .func = "mrp_set_console_prompt" },
    { .line = 290, .func = "find_group" },
    { .line = 316, .func = "find_command" },
    { .line = 342, .func = "mrp_console_add_group" },
    { .line = 369, .func = "mrp_console_del_group" },
    { .line = 380, .func = "mrp_console_add_core_group" },
    { .line = 407, .func = "mrp_console_del_core_group" },
    { .line = 418, .func = "console_grab_output" },
    { .line = 441, .func = "console_release_output" },
    { .line = 460, .func = "console_read_output" },
    { .line = 466, .func = "raw_argument" },
    { .line = 491, .func = "input_evt" },
    { .line = 661, .func = "disconnected_evt" },
    { .line = 667, .func = "complete_evt" },
    { .line = 684, .func = "cookie_write" },
    { .line = 700, .func = "cookie_close" },
    { .line = 708, .func = "console_fopen" },
    { .line = 727, .func = "register_commands" },
    { .line = 733, .func = "unregister_commands" },
    { .line = 749, .func = "get_next_line" },
    { .line = 786, .func = "skip_whitespace" },
    { .line = 793, .func = "get_next_token" },
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
    { .line = 67, .func = "mrp_context_setstate" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "core/context.c",
    .info = info_1
};

/* core/event.c */
static mrp_debug_info_t info_2[] = {
    { .line = 74, .func = "init_watch_lists" },
    { .line = 86, .func = "mrp_add_event_watch" },
    { .line = 158, .func = "delete_watch" },
    { .line = 166, .func = "purge_deleted" },
    { .line = 178, .func = "mrp_del_event_watch" },
    { .line = 189, .func = "mrp_get_event_id" },
    { .line = 217, .func = "mrp_get_event_name" },
    { .line = 231, .func = "mrp_emit_event_msg" },
    { .line = 276, .func = "mrp_emit_event" },
    { .line = 298, .func = "mrp_set_events" },
    { .line = 315, .func = "mrp_set_named_events" },
    { .line = 336, .func = "event_count" },
    { .line = 344, .func = "lowest_bit" },
    { .line = 352, .func = "single_event" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "core/event.c",
    .info = info_2
};

/* core/lua-bindings/lua-console.c */
static mrp_debug_info_t info_3[] = {
    { .line = 12, .func = "eval_cb" },
    { .line = 38, .func = "source_cb" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_3 = {
    .file = "core/lua-bindings/lua-console.c",
    .info = info_3
};

/* core/lua-bindings/lua-log.c */
static mrp_debug_info_t info_4[] = {
    { .line = 8, .func = "call_function" },
    { .line = 51, .func = "log_msg" },
    { .line = 91, .func = "log_info" },
    { .line = 97, .func = "log_warning" },
    { .line = 104, .func = "log_error" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_4 = {
    .file = "core/lua-bindings/lua-log.c",
    .info = info_4
};

/* core/lua-bindings/lua-murphy.c */
static mrp_debug_info_t info_5[] = {
    { .line = 16, .func = "create_murphy_object" },
    { .line = 31, .func = "register_murphy" },
    { .line = 50, .func = "register_bindings" },
    { .line = 67, .func = "mrp_lua_register_murphy_bindings" },
    { .line = 81, .func = "init_lua_utils" },
    { .line = 87, .func = "init_lua_decision" },
    { .line = 94, .func = "init_lua" },
    { .line = 108, .func = "mrp_lua_set_murphy_context" },
    { .line = 142, .func = "mrp_lua_check_murphy_context" },
    { .line = 156, .func = "mrp_lua_get_murphy_context" },
    { .line = 162, .func = "mrp_lua_get_lua_state" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_5 = {
    .file = "core/lua-bindings/lua-murphy.c",
    .info = info_5
};

/* core/lua-bindings/lua-plugin.c */
static mrp_debug_info_t info_6[] = {
    { .line = 10, .func = "plugin_exists" },
    { .line = 28, .func = "load" },
    { .line = 152, .func = "load_plugin" },
    { .line = 158, .func = "try_load_plugin" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_6 = {
    .file = "core/lua-bindings/lua-plugin.c",
    .info = info_6
};

/* core/method.c */
static mrp_debug_info_t info_7[] = {
    { .line = 61, .func = "create_method_table" },
    { .line = 79, .func = "destroy_method_table" },
    { .line = 86, .func = "free_method" },
    { .line = 96, .func = "alloc_method" },
    { .line = 123, .func = "create_method_list" },
    { .line = 151, .func = "free_method_list" },
    { .line = 170, .func = "purge_method_list" },
    { .line = 178, .func = "lookup_method_list" },
    { .line = 191, .func = "check_signatures" },
    { .line = 208, .func = "lookup_method" },
    { .line = 238, .func = "find_method" },
    { .line = 283, .func = "export_method" },
    { .line = 303, .func = "remove_method" },
    { .line = 328, .func = "mrp_export_method" },
    { .line = 362, .func = "mrp_remove_method" },
    { .line = 370, .func = "mrp_import_method" },
    { .line = 410, .func = "mrp_release_method" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_7 = {
    .file = "core/method.c",
    .info = info_7
};

/* core/plugin.c */
static mrp_debug_info_t info_8[] = {
    { .line = 93, .func = "emit_plugin_event" },
    { .line = 105, .func = "mrp_register_builtin_plugin" },
    { .line = 127, .func = "mrp_plugin_exists" },
    { .line = 145, .func = "check_plugin_version" },
    { .line = 163, .func = "check_plugin_singleton" },
    { .line = 175, .func = "mrp_load_plugin" },
    { .line = 304, .func = "load_plugin_cb" },
    { .line = 326, .func = "mrp_load_all_plugins" },
    { .line = 348, .func = "mrp_request_plugin" },
    { .line = 365, .func = "mrp_unload_plugin" },
    { .line = 416, .func = "mrp_start_plugins" },
    { .line = 449, .func = "mrp_start_plugin" },
    { .line = 473, .func = "mrp_stop_plugin" },
    { .line = 493, .func = "find_plugin_instance" },
    { .line = 512, .func = "find_plugin" },
    { .line = 528, .func = "open_dynamic" },
    { .line = 573, .func = "open_builtin" },
    { .line = 589, .func = "parse_plugin_arg" },
    { .line = 641, .func = "parse_plugin_args" },
    { .line = 698, .func = "export_plugin_methods" },
    { .line = 716, .func = "remove_plugin_methods" },
    { .line = 735, .func = "import_plugin_methods" },
    { .line = 753, .func = "release_plugin_methods" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_8 = {
    .file = "core/plugin.c",
    .info = info_8
};

/* core/scripting.c */
static mrp_debug_info_t info_9[] = {
    { .line = 91, .func = "mrp_register_interpreter" },
    { .line = 102, .func = "unregister_interpreter" },
    { .line = 109, .func = "mrp_unregister_interpreter" },
    { .line = 126, .func = "mrp_lookup_interpreter" },
    { .line = 141, .func = "mrp_create_script" },
    { .line = 171, .func = "mrp_destroy_script" },
    { .line = 183, .func = "mrp_compile_script" },
    { .line = 192, .func = "mrp_prepare_script" },
    { .line = 201, .func = "mrp_execute_script" },
    { .line = 210, .func = "mrp_print_value" },
    { .line = 240, .func = "mrp_create_context_table" },
    { .line = 266, .func = "mrp_destroy_context_table" },
    { .line = 278, .func = "lookup_context_var" },
    { .line = 292, .func = "mrp_declare_context_variable" },
    { .line = 337, .func = "mrp_push_context_frame" },
    { .line = 357, .func = "mrp_pop_context_frame" },
    { .line = 389, .func = "get_context_id" },
    { .line = 395, .func = "get_context_value" },
    { .line = 418, .func = "set_context_value" },
    { .line = 465, .func = "set_context_values" },
    { .line = 479, .func = "mrp_get_context_id" },
    { .line = 491, .func = "mrp_get_context_value" },
    { .line = 498, .func = "mrp_set_context_value" },
    { .line = 505, .func = "mrp_get_context_value_by_name" },
    { .line = 512, .func = "mrp_set_context_value_by_name" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_9 = {
    .file = "core/scripting.c",
    .info = info_9
};

/* table of all files */
static mrp_debug_file_t *debug_files[] = {
    &file_0,
    &file_1,
    &file_2,
    &file_3,
    &file_4,
    &file_5,
    &file_6,
    &file_7,
    &file_8,
    &file_9,
    NULL
};

#include <murphy/common/debug-auto-register.c>
