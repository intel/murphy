#include <stdlib.h>
#include <murphy/common/debug.h>

/* common/debug.c */
static mrp_debug_info_t info_0[] = {
    { .line = 58, .func = "free_rule_cb" },
    { .line = 66, .func = "init_rules" },
    { .line = 87, .func = "reset_rules" },
    { .line = 96, .func = "mrp_debug_reset" },
    { .line = 103, .func = "mrp_debug_enable" },
    { .line = 114, .func = "add_rule" },
    { .line = 170, .func = "del_rule" },
    { .line = 212, .func = "mrp_debug_set_config" },
    { .line = 301, .func = "dump_rule_cb" },
    { .line = 315, .func = "mrp_debug_dump_config" },
    { .line = 339, .func = "segment_type" },
    { .line = 364, .func = "segment_flags" },
    { .line = 383, .func = "list_cb" },
    { .line = 456, .func = "mrp_debug_dump_sites" },
    { .line = 464, .func = "mrp_debug_msg" },
    { .line = 477, .func = "mrp_debug_check" },
    { .line = 550, .func = "mrp_debug_register_file" },
    { .line = 558, .func = "mrp_debug_unregister_file" },
    { .line = 569, .func = "mrp_debug_site_function" },
    { .line = 598, .func = "populate_file_table" },
    { .line = 622, .func = "flush_file_table" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "common/debug.c",
    .info = info_0
};

/* common/dgram-transport.c */
static mrp_debug_info_t info_1[] = {
    { .line = 82, .func = "parse_address" },
    { .line = 199, .func = "dgrm_resolve" },
    { .line = 249, .func = "dgrm_open" },
    { .line = 260, .func = "dgrm_createfrom" },
    { .line = 289, .func = "dgrm_bind" },
    { .line = 304, .func = "dgrm_listen" },
    { .line = 313, .func = "dgrm_close" },
    { .line = 332, .func = "dgrm_recv_cb" },
    { .line = 415, .func = "open_socket" },
    { .line = 452, .func = "dgrm_connect" },
    { .line = 480, .func = "dgrm_disconnect" },
    { .line = 496, .func = "dgrm_send" },
    { .line = 533, .func = "dgrm_sendto" },
    { .line = 583, .func = "dgrm_sendraw" },
    { .line = 606, .func = "dgrm_sendrawto" },
    { .line = 632, .func = "senddatato" },
    { .line = 683, .func = "dgrm_senddata" },
    { .line = 692, .func = "dgrm_senddatato" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_1 = {
    .file = "common/dgram-transport.c",
    .info = info_1
};

/* common/file-utils.c */
static mrp_debug_info_t info_2[] = {
    { .line = 42, .func = "translate_glob" },
    { .line = 53, .func = "dirent_type" },
    { .line = 71, .func = "mrp_scan_dir" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_2 = {
    .file = "common/file-utils.c",
    .info = info_2
};

/* common/hashtbl.c */
static mrp_debug_info_t info_3[] = {
    { .line = 68, .func = "calc_buckets" },
    { .line = 84, .func = "mrp_htbl_create" },
    { .line = 127, .func = "mrp_htbl_destroy" },
    { .line = 139, .func = "free_entry" },
    { .line = 147, .func = "mrp_htbl_reset" },
    { .line = 167, .func = "mrp_htbl_insert" },
    { .line = 188, .func = "lookup" },
    { .line = 209, .func = "mrp_htbl_lookup" },
    { .line = 221, .func = "delete_from_bucket" },
    { .line = 261, .func = "mrp_htbl_remove" },
    { .line = 292, .func = "mrp_htbl_foreach" },
    { .line = 343, .func = "mrp_htbl_find" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_3 = {
    .file = "common/hashtbl.c",
    .info = info_3
};

/* common/internal-transport.c */
static mrp_debug_info_t info_4[] = {
    { .line = 75, .func = "process_queue" },
    { .line = 144, .func = "internal_initialize_table" },
    { .line = 205, .func = "internal_resolve" },
    { .line = 232, .func = "internal_open" },
    { .line = 252, .func = "internal_bind" },
    { .line = 271, .func = "internal_listen" },
    { .line = 286, .func = "internal_accept" },
    { .line = 301, .func = "remove_messages" },
    { .line = 328, .func = "internal_close" },
    { .line = 349, .func = "internal_connect" },
    { .line = 382, .func = "internal_disconnect" },
    { .line = 400, .func = "internal_sendto" },
    { .line = 437, .func = "internal_send" },
    { .line = 447, .func = "internal_sendrawto" },
    { .line = 476, .func = "internal_sendraw" },
    { .line = 486, .func = "encode_custom_data" },
    { .line = 522, .func = "internal_senddatato" },
    { .line = 566, .func = "internal_senddata" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_4 = {
    .file = "common/internal-transport.c",
    .info = info_4
};

/* common/log.c */
static mrp_debug_info_t info_5[] = {
    { .line = 58, .func = "mrp_log_parse_levels" },
    { .line = 99, .func = "mrp_log_parse_target" },
    { .line = 105, .func = "mrp_log_enable" },
    { .line = 115, .func = "mrp_log_disable" },
    { .line = 125, .func = "mrp_log_set_mask" },
    { .line = 135, .func = "find_target" },
    { .line = 151, .func = "mrp_log_set_target" },
    { .line = 191, .func = "mrp_log_register_target" },
    { .line = 218, .func = "mrp_log_unregister_target" },
    { .line = 238, .func = "log_msgv" },
    { .line = 276, .func = "mrp_log_msgv" },
    { .line = 290, .func = "mrp_log_msg" },
    { .line = 308, .func = "set_default_logging" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_5 = {
    .file = "common/log.c",
    .info = info_5
};

/* common/mainloop.c */
static mrp_debug_info_t info_6[] = {
    { .line = 214, .func = "add_slave_io_watch" },
    { .line = 248, .func = "slave_io_events" },
    { .line = 267, .func = "free_io_watch" },
    { .line = 288, .func = "mrp_add_io_watch" },
    { .line = 327, .func = "mrp_del_io_watch" },
    { .line = 341, .func = "delete_io_watch" },
    { .line = 396, .func = "time_now" },
    { .line = 409, .func = "usecs_to_msecs" },
    { .line = 419, .func = "insert_timer" },
    { .line = 452, .func = "rearm_timer" },
    { .line = 460, .func = "find_next_timer" },
    { .line = 479, .func = "mrp_add_timer" },
    { .line = 502, .func = "mrp_del_timer" },
    { .line = 523, .func = "delete_timer" },
    { .line = 534, .func = "mrp_add_deferred" },
    { .line = 555, .func = "mrp_del_deferred" },
    { .line = 569, .func = "delete_deferred" },
    { .line = 576, .func = "mrp_disable_deferred" },
    { .line = 583, .func = "disable_deferred" },
    { .line = 593, .func = "mrp_enable_deferred" },
    { .line = 609, .func = "delete_sighandler" },
    { .line = 616, .func = "dispatch_signals" },
    { .line = 646, .func = "setup_sighandlers" },
    { .line = 669, .func = "mrp_add_sighandler" },
    { .line = 694, .func = "recalc_sigmask" },
    { .line = 712, .func = "mrp_del_sighandler" },
    { .line = 727, .func = "free_subloop" },
    { .line = 739, .func = "subloop_event_cb" },
    { .line = 753, .func = "mrp_add_subloop" },
    { .line = 789, .func = "mrp_del_subloop" },
    { .line = 827, .func = "super_io_cb" },
    { .line = 843, .func = "super_timer_cb" },
    { .line = 856, .func = "super_work_cb" },
    { .line = 901, .func = "mrp_set_superloop" },
    { .line = 945, .func = "mrp_clear_superloop" },
    { .line = 978, .func = "mrp_mainloop_unregister" },
    { .line = 988, .func = "purge_io_watches" },
    { .line = 1008, .func = "purge_timers" },
    { .line = 1021, .func = "purge_deferred" },
    { .line = 1040, .func = "purge_sighandlers" },
    { .line = 1053, .func = "purge_deleted" },
    { .line = 1073, .func = "purge_subloops" },
    { .line = 1086, .func = "mrp_mainloop_create" },
    { .line = 1120, .func = "mrp_mainloop_destroy" },
    { .line = 1137, .func = "prepare_subloop" },
    { .line = 1251, .func = "prepare_subloops" },
    { .line = 1272, .func = "mrp_mainloop_prepare" },
    { .line = 1313, .func = "mrp_mainloop_poll" },
    { .line = 1342, .func = "poll_subloop" },
    { .line = 1367, .func = "dispatch_deferred" },
    { .line = 1389, .func = "dispatch_timers" },
    { .line = 1420, .func = "dispatch_subloops" },
    { .line = 1439, .func = "dispatch_slaves" },
    { .line = 1465, .func = "dispatch_poll_events" },
    { .line = 1512, .func = "mrp_mainloop_dispatch" },
    { .line = 1533, .func = "mrp_mainloop_iterate" },
    { .line = 1543, .func = "mrp_mainloop_run" },
    { .line = 1552, .func = "mrp_mainloop_quit" },
    { .line = 1564, .func = "dump_pollfds" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_6 = {
    .file = "common/mainloop.c",
    .info = info_6
};

/* common/mm.c */
static mrp_debug_info_t info_7[] = {
    { .line = 95, .func = "setup" },
    { .line = 122, .func = "cleanup" },
    { .line = 133, .func = "memblk_alloc" },
    { .line = 160, .func = "memblk_resize" },
    { .line = 195, .func = "memblk_free" },
    { .line = 217, .func = "memblk_to_ptr" },
    { .line = 226, .func = "ptr_to_memblk" },
    { .line = 245, .func = "__mm_backtrace" },
    { .line = 257, .func = "__mm_alloc" },
    { .line = 270, .func = "__mm_realloc" },
    { .line = 288, .func = "__mm_memalign" },
    { .line = 305, .func = "__mm_free" },
    { .line = 325, .func = "__passthru_alloc" },
    { .line = 336, .func = "__passthru_realloc" },
    { .line = 347, .func = "__passthru_memalign" },
    { .line = 358, .func = "__passthru_free" },
    { .line = 373, .func = "mrp_mm_alloc" },
    { .line = 379, .func = "mrp_mm_realloc" },
    { .line = 386, .func = "mrp_mm_strdup" },
    { .line = 405, .func = "mrp_mm_memalign" },
    { .line = 412, .func = "mrp_mm_free" },
    { .line = 418, .func = "mrp_mm_config" },
    { .line = 446, .func = "mrp_mm_check" },
    { .line = 531, .func = "mrp_objpool_create" },
    { .line = 571, .func = "free_object" },
    { .line = 580, .func = "mrp_objpool_destroy" },
    { .line = 591, .func = "mrp_objpool_alloc" },
    { .line = 654, .func = "mrp_objpool_free" },
    { .line = 705, .func = "mrp_objpool_grow" },
    { .line = 713, .func = "mrp_objpool_shrink" },
    { .line = 721, .func = "pool_calc_sizes" },
    { .line = 804, .func = "pool_grow" },
    { .line = 825, .func = "pool_shrink" },
    { .line = 850, .func = "pool_foreach_object" },
    { .line = 869, .func = "chunk_foreach_object" },
    { .line = 899, .func = "chunk_empty" },
    { .line = 922, .func = "chunk_init" },
    { .line = 952, .func = "chunk_alloc" },
    { .line = 970, .func = "chunk_free" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_7 = {
    .file = "common/mm.c",
    .info = info_7
};

/* common/msg.c */
static mrp_debug_info_t info_8[] = {
    { .line = 49, .func = "destroy_field" },
    { .line = 83, .func = "create_field" },
    { .line = 257, .func = "msg_destroy" },
    { .line = 273, .func = "mrp_msg_createv" },
    { .line = 306, .func = "mrp_msg_create" },
    { .line = 319, .func = "mrp_msg_ref" },
    { .line = 325, .func = "mrp_msg_unref" },
    { .line = 332, .func = "mrp_msg_append" },
    { .line = 351, .func = "mrp_msg_prepend" },
    { .line = 370, .func = "mrp_msg_set" },
    { .line = 394, .func = "mrp_msg_iterate" },
    { .line = 459, .func = "mrp_msg_find" },
    { .line = 474, .func = "mrp_msg_get" },
    { .line = 589, .func = "mrp_msg_iterate_get" },
    { .line = 723, .func = "field_type_name" },
    { .line = 776, .func = "mrp_msg_dump" },
    { .line = 911, .func = "mrp_msg_default_encode" },
    { .line = 1063, .func = "mrp_msg_default_decode" },
    { .line = 1289, .func = "guarded_array_size" },
    { .line = 1328, .func = "counted_array_size" },
    { .line = 1345, .func = "get_array_size" },
    { .line = 1366, .func = "mrp_data_get_array_size" },
    { .line = 1372, .func = "get_blob_size" },
    { .line = 1399, .func = "mrp_data_get_blob_size" },
    { .line = 1405, .func = "check_and_init_array_descr" },
    { .line = 1438, .func = "mrp_msg_register_type" },
    { .line = 1495, .func = "mrp_msg_find_type" },
    { .line = 1515, .func = "cleanup_types" },
    { .line = 1523, .func = "mrp_data_encode" },
    { .line = 1688, .func = "member_type" },
    { .line = 1702, .func = "mrp_data_decode" },
    { .line = 1932, .func = "mrp_data_dump" },
    { .line = 2047, .func = "mrp_data_free" },
    { .line = 2082, .func = "mrp_msgbuf_write" },
    { .line = 2100, .func = "mrp_msgbuf_read" },
    { .line = 2107, .func = "mrp_msgbuf_cancel" },
    { .line = 2114, .func = "mrp_msgbuf_ensure" },
    { .line = 2140, .func = "mrp_msgbuf_reserve" },
    { .line = 2172, .func = "mrp_msgbuf_pull" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_8 = {
    .file = "common/msg.c",
    .info = info_8
};

/* common/stream-transport.c */
static mrp_debug_info_t info_9[] = {
    { .line = 73, .func = "parse_address" },
    { .line = 191, .func = "strm_resolve" },
    { .line = 241, .func = "strm_open" },
    { .line = 251, .func = "strm_createfrom" },
    { .line = 283, .func = "strm_bind" },
    { .line = 297, .func = "strm_listen" },
    { .line = 312, .func = "strm_accept" },
    { .line = 356, .func = "strm_close" },
    { .line = 375, .func = "strm_recv_cb" },
    { .line = 484, .func = "open_socket" },
    { .line = 521, .func = "strm_connect" },
    { .line = 557, .func = "strm_disconnect" },
    { .line = 574, .func = "strm_send" },
    { .line = 611, .func = "strm_sendraw" },
    { .line = 634, .func = "strm_senddata" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_9 = {
    .file = "common/stream-transport.c",
    .info = info_9
};

/* common/transport.c */
static mrp_debug_info_t info_10[] = {
    { .line = 47, .func = "check_request_callbacks" },
    { .line = 71, .func = "mrp_transport_register" },
    { .line = 87, .func = "mrp_transport_unregister" },
    { .line = 93, .func = "find_transport" },
    { .line = 108, .func = "check_event_callbacks" },
    { .line = 133, .func = "mrp_transport_create" },
    { .line = 169, .func = "mrp_transport_create_from" },
    { .line = 206, .func = "type_matches" },
    { .line = 215, .func = "mrp_transport_resolve" },
    { .line = 239, .func = "mrp_transport_bind" },
    { .line = 253, .func = "mrp_transport_listen" },
    { .line = 273, .func = "mrp_transport_accept" },
    { .line = 308, .func = "purge_destroyed" },
    { .line = 320, .func = "mrp_transport_destroy" },
    { .line = 335, .func = "check_destroy" },
    { .line = 341, .func = "mrp_transport_connect" },
    { .line = 374, .func = "mrp_transport_disconnect" },
    { .line = 397, .func = "mrp_transport_send" },
    { .line = 415, .func = "mrp_transport_sendto" },
    { .line = 434, .func = "mrp_transport_sendraw" },
    { .line = 453, .func = "mrp_transport_sendrawto" },
    { .line = 472, .func = "mrp_transport_senddata" },
    { .line = 491, .func = "mrp_transport_senddatato" },
    { .line = 510, .func = "recv_data" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_10 = {
    .file = "common/transport.c",
    .info = info_10
};

/* common/utils.c */
static mrp_debug_info_t info_11[] = {
    { .line = 45, .func = "notify_parent" },
    { .line = 58, .func = "mrp_daemonize" },
    { .line = 194, .func = "mrp_string_comp" },
    { .line = 200, .func = "mrp_string_hash" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_11 = {
    .file = "common/utils.c",
    .info = info_11
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
    &file_11,
    NULL
};

#include <murphy/common/debug-auto-register.c>
