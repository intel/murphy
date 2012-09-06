#include <stdlib.h>
#include <murphy/common/debug.h>

/* common/debug.c */
static mrp_debug_info_t info_0[] = {
    { .line = 57, .func = "free_rule_cb" },
    { .line = 65, .func = "init_rules" },
    { .line = 84, .func = "reset_rules" },
    { .line = 93, .func = "mrp_debug_reset" },
    { .line = 100, .func = "mrp_debug_enable" },
    { .line = 111, .func = "add_rule" },
    { .line = 167, .func = "del_rule" },
    { .line = 209, .func = "mrp_debug_set_config" },
    { .line = 298, .func = "dump_rule_cb" },
    { .line = 312, .func = "mrp_debug_dump_config" },
    { .line = 336, .func = "segment_type" },
    { .line = 361, .func = "segment_flags" },
    { .line = 380, .func = "list_cb" },
    { .line = 453, .func = "mrp_debug_dump_sites" },
    { .line = 461, .func = "mrp_debug_msg" },
    { .line = 474, .func = "mrp_debug_check" },
    { .line = 547, .func = "mrp_debug_register_file" },
    { .line = 555, .func = "mrp_debug_unregister_file" },
    { .line = 566, .func = "mrp_debug_site_function" },
    { .line = 595, .func = "populate_file_table" },
    { .line = 619, .func = "flush_file_table" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "common/debug.c",
    .info = info_0
};

/* common/dgram-transport.c */
static mrp_debug_info_t info_1[] = {
    { .line = 53, .func = "parse_address" },
    { .line = 170, .func = "dgrm_resolve" },
    { .line = 220, .func = "dgrm_open" },
    { .line = 231, .func = "dgrm_createfrom" },
    { .line = 260, .func = "dgrm_bind" },
    { .line = 275, .func = "dgrm_listen" },
    { .line = 284, .func = "dgrm_close" },
    { .line = 303, .func = "dgrm_recv_cb" },
    { .line = 386, .func = "open_socket" },
    { .line = 423, .func = "dgrm_connect" },
    { .line = 451, .func = "dgrm_disconnect" },
    { .line = 467, .func = "dgrm_send" },
    { .line = 504, .func = "dgrm_sendto" },
    { .line = 554, .func = "dgrm_sendraw" },
    { .line = 577, .func = "dgrm_sendrawto" },
    { .line = 603, .func = "senddatato" },
    { .line = 654, .func = "dgrm_senddata" },
    { .line = 663, .func = "dgrm_senddatato" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "common/dgram-transport.c",
    .info = info_1
};

/* common/file-utils.c */
static mrp_debug_info_t info_2[] = {
    { .line = 13, .func = "translate_glob" },
    { .line = 24, .func = "dirent_type" },
    { .line = 42, .func = "mrp_scan_dir" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "common/file-utils.c",
    .info = info_2
};

/* common/hashtbl.c */
static mrp_debug_info_t info_3[] = {
    { .line = 39, .func = "calc_buckets" },
    { .line = 55, .func = "mrp_htbl_create" },
    { .line = 98, .func = "mrp_htbl_destroy" },
    { .line = 110, .func = "free_entry" },
    { .line = 118, .func = "mrp_htbl_reset" },
    { .line = 138, .func = "mrp_htbl_insert" },
    { .line = 159, .func = "lookup" },
    { .line = 180, .func = "mrp_htbl_lookup" },
    { .line = 192, .func = "delete_from_bucket" },
    { .line = 232, .func = "mrp_htbl_remove" },
    { .line = 263, .func = "mrp_htbl_foreach" },
    { .line = 314, .func = "mrp_htbl_find" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_3 = {
    .file = "common/hashtbl.c",
    .info = info_3
};

/* common/log.c */
static mrp_debug_info_t info_4[] = {
    { .line = 14, .func = "mrp_log_parse_levels" },
    { .line = 55, .func = "mrp_log_parse_target" },
    { .line = 68, .func = "mrp_log_enable" },
    { .line = 78, .func = "mrp_log_disable" },
    { .line = 88, .func = "mrp_log_set_mask" },
    { .line = 98, .func = "mrp_log_set_target" },
    { .line = 140, .func = "mrp_log_msgv" },
    { .line = 176, .func = "mrp_log_msg" },
    { .line = 197, .func = "set_default_logging" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_4 = {
    .file = "common/log.c",
    .info = info_4
};

/* common/mainloop.c */
static mrp_debug_info_t info_5[] = {
    { .line = 212, .func = "add_slave_io_watch" },
    { .line = 246, .func = "slave_io_events" },
    { .line = 265, .func = "free_io_watch" },
    { .line = 286, .func = "mrp_add_io_watch" },
    { .line = 325, .func = "mrp_del_io_watch" },
    { .line = 339, .func = "delete_io_watch" },
    { .line = 394, .func = "time_now" },
    { .line = 407, .func = "usecs_to_msecs" },
    { .line = 417, .func = "insert_timer" },
    { .line = 450, .func = "rearm_timer" },
    { .line = 458, .func = "find_next_timer" },
    { .line = 477, .func = "mrp_add_timer" },
    { .line = 500, .func = "mrp_del_timer" },
    { .line = 521, .func = "delete_timer" },
    { .line = 532, .func = "mrp_add_deferred" },
    { .line = 553, .func = "mrp_del_deferred" },
    { .line = 567, .func = "delete_deferred" },
    { .line = 574, .func = "mrp_disable_deferred" },
    { .line = 581, .func = "disable_deferred" },
    { .line = 591, .func = "mrp_enable_deferred" },
    { .line = 607, .func = "delete_sighandler" },
    { .line = 614, .func = "dispatch_signals" },
    { .line = 644, .func = "setup_sighandlers" },
    { .line = 667, .func = "mrp_add_sighandler" },
    { .line = 692, .func = "recalc_sigmask" },
    { .line = 710, .func = "mrp_del_sighandler" },
    { .line = 725, .func = "free_subloop" },
    { .line = 737, .func = "subloop_event_cb" },
    { .line = 751, .func = "mrp_add_subloop" },
    { .line = 787, .func = "mrp_del_subloop" },
    { .line = 824, .func = "super_io_cb" },
    { .line = 840, .func = "super_timer_cb" },
    { .line = 853, .func = "super_work_cb" },
    { .line = 898, .func = "mrp_set_superloop" },
    { .line = 942, .func = "mrp_clear_superloop" },
    { .line = 964, .func = "purge_io_watches" },
    { .line = 984, .func = "purge_timers" },
    { .line = 997, .func = "purge_deferred" },
    { .line = 1016, .func = "purge_sighandlers" },
    { .line = 1029, .func = "purge_deleted" },
    { .line = 1049, .func = "purge_subloops" },
    { .line = 1062, .func = "mrp_mainloop_create" },
    { .line = 1096, .func = "mrp_mainloop_destroy" },
    { .line = 1113, .func = "prepare_subloop" },
    { .line = 1225, .func = "prepare_subloops" },
    { .line = 1246, .func = "mrp_mainloop_prepare" },
    { .line = 1287, .func = "mrp_mainloop_poll" },
    { .line = 1316, .func = "poll_subloop" },
    { .line = 1341, .func = "dispatch_deferred" },
    { .line = 1363, .func = "dispatch_timers" },
    { .line = 1394, .func = "dispatch_subloops" },
    { .line = 1413, .func = "dispatch_slaves" },
    { .line = 1439, .func = "dispatch_poll_events" },
    { .line = 1471, .func = "mrp_mainloop_dispatch" },
    { .line = 1492, .func = "mrp_mainloop_iterate" },
    { .line = 1502, .func = "mrp_mainloop_run" },
    { .line = 1511, .func = "mrp_mainloop_quit" },
    { .line = 1523, .func = "dump_pollfds" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_5 = {
    .file = "common/mainloop.c",
    .info = info_5
};

/* common/mm.c */
static mrp_debug_info_t info_6[] = {
    { .line = 66, .func = "setup" },
    { .line = 93, .func = "cleanup" },
    { .line = 104, .func = "memblk_alloc" },
    { .line = 131, .func = "memblk_resize" },
    { .line = 166, .func = "memblk_free" },
    { .line = 188, .func = "memblk_to_ptr" },
    { .line = 197, .func = "ptr_to_memblk" },
    { .line = 216, .func = "__mm_backtrace" },
    { .line = 228, .func = "__mm_alloc" },
    { .line = 241, .func = "__mm_realloc" },
    { .line = 259, .func = "__mm_memalign" },
    { .line = 276, .func = "__mm_free" },
    { .line = 296, .func = "__passthru_alloc" },
    { .line = 307, .func = "__passthru_realloc" },
    { .line = 318, .func = "__passthru_memalign" },
    { .line = 329, .func = "__passthru_free" },
    { .line = 344, .func = "mrp_mm_alloc" },
    { .line = 350, .func = "mrp_mm_realloc" },
    { .line = 357, .func = "mrp_mm_strdup" },
    { .line = 376, .func = "mrp_mm_memalign" },
    { .line = 383, .func = "mrp_mm_free" },
    { .line = 389, .func = "mrp_mm_config" },
    { .line = 417, .func = "mrp_mm_check" },
    { .line = 502, .func = "mrp_objpool_create" },
    { .line = 542, .func = "free_object" },
    { .line = 551, .func = "mrp_objpool_destroy" },
    { .line = 562, .func = "mrp_objpool_alloc" },
    { .line = 625, .func = "mrp_objpool_free" },
    { .line = 676, .func = "mrp_objpool_grow" },
    { .line = 684, .func = "mrp_objpool_shrink" },
    { .line = 692, .func = "pool_calc_sizes" },
    { .line = 775, .func = "pool_grow" },
    { .line = 796, .func = "pool_shrink" },
    { .line = 821, .func = "pool_foreach_object" },
    { .line = 840, .func = "chunk_foreach_object" },
    { .line = 870, .func = "chunk_empty" },
    { .line = 893, .func = "chunk_init" },
    { .line = 923, .func = "chunk_alloc" },
    { .line = 941, .func = "chunk_free" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_6 = {
    .file = "common/mm.c",
    .info = info_6
};

/* common/msg.c */
static mrp_debug_info_t info_7[] = {
    { .line = 49, .func = "destroy_field" },
    { .line = 83, .func = "create_field" },
    { .line = 257, .func = "msg_destroy" },
    { .line = 273, .func = "mrp_msg_createv" },
    { .line = 306, .func = "mrp_msg_create" },
    { .line = 319, .func = "mrp_msg_ref" },
    { .line = 325, .func = "mrp_msg_unref" },
    { .line = 332, .func = "mrp_msg_append" },
    { .line = 351, .func = "mrp_msg_prepend" },
    { .line = 370, .func = "mrp_msg_find" },
    { .line = 385, .func = "field_type_name" },
    { .line = 438, .func = "mrp_msg_dump" },
    { .line = 573, .func = "mrp_msg_default_encode" },
    { .line = 725, .func = "mrp_msg_default_decode" },
    { .line = 951, .func = "guarded_array_size" },
    { .line = 990, .func = "counted_array_size" },
    { .line = 1007, .func = "get_array_size" },
    { .line = 1028, .func = "mrp_data_get_array_size" },
    { .line = 1034, .func = "get_blob_size" },
    { .line = 1061, .func = "mrp_data_get_blob_size" },
    { .line = 1067, .func = "check_and_init_array_descr" },
    { .line = 1100, .func = "mrp_msg_register_type" },
    { .line = 1157, .func = "mrp_msg_find_type" },
    { .line = 1177, .func = "cleanup_types" },
    { .line = 1185, .func = "mrp_data_encode" },
    { .line = 1350, .func = "member_type" },
    { .line = 1364, .func = "mrp_data_decode" },
    { .line = 1594, .func = "mrp_data_dump" },
    { .line = 1709, .func = "mrp_data_free" },
    { .line = 1744, .func = "mrp_msgbuf_write" },
    { .line = 1762, .func = "mrp_msgbuf_read" },
    { .line = 1769, .func = "mrp_msgbuf_cancel" },
    { .line = 1776, .func = "mrp_msgbuf_ensure" },
    { .line = 1802, .func = "mrp_msgbuf_reserve" },
    { .line = 1834, .func = "mrp_msgbuf_pull" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_7 = {
    .file = "common/msg.c",
    .info = info_7
};

/* common/stream-transport.c */
static mrp_debug_info_t info_8[] = {
    { .line = 44, .func = "parse_address" },
    { .line = 162, .func = "strm_resolve" },
    { .line = 212, .func = "strm_open" },
    { .line = 222, .func = "strm_createfrom" },
    { .line = 254, .func = "strm_bind" },
    { .line = 268, .func = "strm_listen" },
    { .line = 283, .func = "strm_accept" },
    { .line = 327, .func = "strm_close" },
    { .line = 346, .func = "strm_recv_cb" },
    { .line = 449, .func = "open_socket" },
    { .line = 486, .func = "strm_connect" },
    { .line = 522, .func = "strm_disconnect" },
    { .line = 539, .func = "strm_send" },
    { .line = 576, .func = "strm_sendraw" },
    { .line = 599, .func = "strm_senddata" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_8 = {
    .file = "common/stream-transport.c",
    .info = info_8
};

/* common/transport.c */
static mrp_debug_info_t info_9[] = {
    { .line = 18, .func = "check_request_callbacks" },
    { .line = 42, .func = "mrp_transport_register" },
    { .line = 58, .func = "mrp_transport_unregister" },
    { .line = 64, .func = "find_transport" },
    { .line = 79, .func = "check_event_callbacks" },
    { .line = 104, .func = "mrp_transport_create" },
    { .line = 140, .func = "mrp_transport_create_from" },
    { .line = 177, .func = "type_matches" },
    { .line = 186, .func = "mrp_transport_resolve" },
    { .line = 210, .func = "mrp_transport_bind" },
    { .line = 224, .func = "mrp_transport_listen" },
    { .line = 244, .func = "mrp_transport_accept" },
    { .line = 274, .func = "purge_destroyed" },
    { .line = 286, .func = "mrp_transport_destroy" },
    { .line = 301, .func = "check_destroy" },
    { .line = 307, .func = "mrp_transport_connect" },
    { .line = 340, .func = "mrp_transport_disconnect" },
    { .line = 363, .func = "mrp_transport_send" },
    { .line = 381, .func = "mrp_transport_sendto" },
    { .line = 400, .func = "mrp_transport_sendraw" },
    { .line = 419, .func = "mrp_transport_sendrawto" },
    { .line = 438, .func = "mrp_transport_senddata" },
    { .line = 457, .func = "mrp_transport_senddatato" },
    { .line = 476, .func = "recv_data" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_9 = {
    .file = "common/transport.c",
    .info = info_9
};

/* common/utils.c */
static mrp_debug_info_t info_10[] = {
    { .line = 16, .func = "notify_parent" },
    { .line = 29, .func = "mrp_daemonize" },
    { .line = 165, .func = "mrp_string_comp" },
    { .line = 171, .func = "mrp_string_hash" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_10 = {
    .file = "common/utils.c",
    .info = info_10
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
    &file_10,
    NULL
};

#include <murphy/common/debug-auto-register.c>
