#include <stdlib.h>
#include <murphy/common/debug.h>

/* common/dbus.c */
static mrp_debug_info_t info_0[] = {
    { .line = 147, .func = "purge_filters" },
    { .line = 167, .func = "dbus_disconnect" },
    { .line = 202, .func = "bus_cmp" },
    { .line = 208, .func = "bus_hash" },
    { .line = 219, .func = "find_bus_by_spec" },
    { .line = 233, .func = "dbus_get" },
    { .line = 258, .func = "mrp_dbus_connect" },
    { .line = 378, .func = "mrp_dbus_ref" },
    { .line = 384, .func = "mrp_dbus_unref" },
    { .line = 396, .func = "mrp_dbus_acquire_name" },
    { .line = 418, .func = "mrp_dbus_release_name" },
    { .line = 429, .func = "mrp_dbus_get_unique_name" },
    { .line = 434, .func = "name_owner_query_cb" },
    { .line = 456, .func = "name_owner_change_cb" },
    { .line = 501, .func = "mrp_dbus_follow_name" },
    { .line = 536, .func = "mrp_dbus_forget_name" },
    { .line = 570, .func = "purge_name_trackers" },
    { .line = 588, .func = "handler_alloc" },
    { .line = 615, .func = "handler_free" },
    { .line = 628, .func = "handler_list_alloc" },
    { .line = 645, .func = "handler_list_free" },
    { .line = 661, .func = "handler_list_free_cb" },
    { .line = 669, .func = "handler_specificity" },
    { .line = 684, .func = "handler_list_insert" },
    { .line = 705, .func = "handler_list_lookup" },
    { .line = 727, .func = "handler_list_find" },
    { .line = 746, .func = "mrp_dbus_export_method" },
    { .line = 770, .func = "mrp_dbus_remove_method" },
    { .line = 793, .func = "mrp_dbus_add_signal_handler" },
    { .line = 826, .func = "mrp_dbus_del_signal_handler" },
    { .line = 856, .func = "mrp_dbus_subscribe_signal" },
    { .line = 883, .func = "mrp_dbus_unsubscribe_signal" },
    { .line = 902, .func = "mrp_dbus_install_filterv" },
    { .line = 956, .func = "mrp_dbus_install_filter" },
    { .line = 972, .func = "mrp_dbus_remove_filterv" },
    { .line = 1004, .func = "mrp_dbus_remove_filter" },
    { .line = 1020, .func = "dispatch_method" },
    { .line = 1061, .func = "dispatch_signal" },
    { .line = 1116, .func = "call_reply_cb" },
    { .line = 1135, .func = "mrp_dbus_call" },
    { .line = 1217, .func = "mrp_dbus_send" },
    { .line = 1298, .func = "mrp_dbus_send_msg" },
    { .line = 1304, .func = "mrp_dbus_call_cancel" },
    { .line = 1328, .func = "mrp_dbus_reply" },
    { .line = 1365, .func = "mrp_dbus_reply_error" },
    { .line = 1403, .func = "call_free" },
    { .line = 1410, .func = "purge_calls" },
    { .line = 1428, .func = "mrp_dbus_signal" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "common/dbus.c",
    .info = info_0
};

/* common/dbus-glue.c */
static mrp_debug_info_t info_1[] = {
    { .line = 67, .func = "dispatch_watch" },
    { .line = 92, .func = "watch_freed_cb" },
    { .line = 104, .func = "add_watch" },
    { .line = 153, .func = "del_watch" },
    { .line = 168, .func = "toggle_watch" },
    { .line = 179, .func = "dispatch_timeout" },
    { .line = 191, .func = "timeout_freed_cb" },
    { .line = 204, .func = "add_timeout" },
    { .line = 235, .func = "del_timeout" },
    { .line = 250, .func = "toggle_timeout" },
    { .line = 261, .func = "wakeup_mainloop" },
    { .line = 271, .func = "glue_free_cb" },
    { .line = 300, .func = "pump_cb" },
    { .line = 311, .func = "dispatch_status_cb" },
    { .line = 334, .func = "mrp_dbus_setup_connection" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "common/dbus-glue.c",
    .info = info_1
};

/* common/dbus-transport.c */
static mrp_debug_info_t info_2[] = {
    { .line = 87, .func = "parse_address" },
    { .line = 193, .func = "copy_address" },
    { .line = 246, .func = "check_address" },
    { .line = 254, .func = "peer_address" },
    { .line = 296, .func = "dbus_resolve" },
    { .line = 312, .func = "dbus_open" },
    { .line = 320, .func = "dbus_createfrom" },
    { .line = 334, .func = "dbus_bind" },
    { .line = 415, .func = "dbus_autobind" },
    { .line = 433, .func = "dbus_close" },
    { .line = 469, .func = "dbus_msg_cb" },
    { .line = 512, .func = "dbus_data_cb" },
    { .line = 555, .func = "dbus_raw_cb" },
    { .line = 598, .func = "peer_state_cb" },
    { .line = 632, .func = "dbus_connect" },
    { .line = 669, .func = "dbus_disconnect" },
    { .line = 683, .func = "dbus_sendmsgto" },
    { .line = 721, .func = "dbus_sendmsg" },
    { .line = 731, .func = "dbus_sendrawto" },
    { .line = 776, .func = "dbus_sendraw" },
    { .line = 786, .func = "dbus_senddatato" },
    { .line = 824, .func = "dbus_senddata" },
    { .line = 834, .func = "get_array_signature" },
    { .line = 859, .func = "msg_encode" },
    { .line = 1026, .func = "msg_decode" },
    { .line = 1235, .func = "data_encode" },
    { .line = 1408, .func = "member_type" },
    { .line = 1422, .func = "data_decode" },
    { .line = 1628, .func = "raw_encode" },
    { .line = 1667, .func = "raw_decode" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "common/dbus-transport.c",
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
