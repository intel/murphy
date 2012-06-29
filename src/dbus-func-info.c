#include <stdlib.h>
#include <murphy/common/debug.h>

/* common/dbus.c */
static mrp_debug_info_t info_0[] = {
    { .line = 104, .func = "purge_filters" },
    { .line = 124, .func = "dbus_disconnect" },
    { .line = 153, .func = "mrp_dbus_connect" },
    { .line = 266, .func = "mrp_dbus_ref" },
    { .line = 272, .func = "mrp_dbus_unref" },
    { .line = 284, .func = "mrp_dbus_acquire_name" },
    { .line = 305, .func = "mrp_dbus_release_name" },
    { .line = 316, .func = "mrp_dbus_get_unique_name" },
    { .line = 321, .func = "name_owner_query_cb" },
    { .line = 343, .func = "name_owner_change_cb" },
    { .line = 388, .func = "mrp_dbus_follow_name" },
    { .line = 423, .func = "mrp_dbus_forget_name" },
    { .line = 457, .func = "purge_name_trackers" },
    { .line = 475, .func = "handler_alloc" },
    { .line = 502, .func = "handler_free" },
    { .line = 515, .func = "handler_list_alloc" },
    { .line = 532, .func = "handler_list_free" },
    { .line = 548, .func = "handler_list_free_cb" },
    { .line = 556, .func = "handler_specificity" },
    { .line = 571, .func = "handler_list_insert" },
    { .line = 592, .func = "handler_list_lookup" },
    { .line = 614, .func = "handler_list_find" },
    { .line = 633, .func = "mrp_dbus_export_method" },
    { .line = 657, .func = "mrp_dbus_remove_method" },
    { .line = 680, .func = "mrp_dbus_add_signal_handler" },
    { .line = 713, .func = "mrp_dbus_del_signal_handler" },
    { .line = 743, .func = "mrp_dbus_subscribe_signal" },
    { .line = 770, .func = "mrp_dbus_unsubscribe_signal" },
    { .line = 789, .func = "mrp_dbus_install_filterv" },
    { .line = 843, .func = "mrp_dbus_install_filter" },
    { .line = 859, .func = "mrp_dbus_remove_filterv" },
    { .line = 891, .func = "mrp_dbus_remove_filter" },
    { .line = 907, .func = "dispatch_method" },
    { .line = 948, .func = "dispatch_signal" },
    { .line = 1001, .func = "call_reply_cb" },
    { .line = 1020, .func = "mrp_dbus_call" },
    { .line = 1102, .func = "mrp_dbus_send" },
    { .line = 1183, .func = "mrp_dbus_send_msg" },
    { .line = 1189, .func = "mrp_dbus_call_cancel" },
    { .line = 1213, .func = "mrp_dbus_reply" },
    { .line = 1250, .func = "call_free" },
    { .line = 1257, .func = "purge_calls" },
    { .line = 1275, .func = "mrp_dbus_signal" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "common/dbus.c",
    .info = info_0
};

/* common/dbus-glue.c */
static mrp_debug_info_t info_1[] = {
    { .line = 38, .func = "dispatch_watch" },
    { .line = 64, .func = "watch_freed_cb" },
    { .line = 76, .func = "add_watch" },
    { .line = 125, .func = "del_watch" },
    { .line = 140, .func = "toggle_watch" },
    { .line = 151, .func = "dispatch_timeout" },
    { .line = 165, .func = "timeout_freed_cb" },
    { .line = 178, .func = "add_timeout" },
    { .line = 209, .func = "del_timeout" },
    { .line = 224, .func = "toggle_timeout" },
    { .line = 235, .func = "wakeup_mainloop" },
    { .line = 245, .func = "glue_free_cb" },
    { .line = 274, .func = "pump_cb" },
    { .line = 287, .func = "dispatch_status_cb" },
    { .line = 310, .func = "mrp_dbus_setup_connection" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "common/dbus-glue.c",
    .info = info_1
};

/* common/dbus-transport.c */
static mrp_debug_info_t info_2[] = {
    { .line = 58, .func = "parse_address" },
    { .line = 164, .func = "copy_address" },
    { .line = 217, .func = "check_address" },
    { .line = 225, .func = "peer_address" },
    { .line = 267, .func = "dbus_resolve" },
    { .line = 283, .func = "dbus_open" },
    { .line = 291, .func = "dbus_createfrom" },
    { .line = 305, .func = "dbus_bind" },
    { .line = 381, .func = "dbus_autobind" },
    { .line = 399, .func = "dbus_close" },
    { .line = 433, .func = "dbus_msg_cb" },
    { .line = 476, .func = "dbus_data_cb" },
    { .line = 519, .func = "dbus_raw_cb" },
    { .line = 562, .func = "peer_state_cb" },
    { .line = 596, .func = "dbus_connect" },
    { .line = 633, .func = "dbus_disconnect" },
    { .line = 647, .func = "dbus_sendmsgto" },
    { .line = 685, .func = "dbus_sendmsg" },
    { .line = 695, .func = "dbus_sendrawto" },
    { .line = 740, .func = "dbus_sendraw" },
    { .line = 750, .func = "dbus_senddatato" },
    { .line = 788, .func = "dbus_senddata" },
    { .line = 798, .func = "get_array_signature" },
    { .line = 823, .func = "msg_encode" },
    { .line = 990, .func = "msg_decode" },
    { .line = 1199, .func = "data_encode" },
    { .line = 1372, .func = "member_type" },
    { .line = 1386, .func = "data_decode" },
    { .line = 1591, .func = "raw_encode" },
    { .line = 1630, .func = "raw_decode" },
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
