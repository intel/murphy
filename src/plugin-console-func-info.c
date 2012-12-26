#include <stdlib.h>
#include <murphy/common/debug.h>

/* plugins/plugin-console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 83, .func = "write_req" },
    { .line = 106, .func = "logger" },
    { .line = 133, .func = "register_logger" },
    { .line = 145, .func = "unregister_logger" },
    { .line = 157, .func = "tcp_close_req" },
    { .line = 170, .func = "udp_close_req" },
    { .line = 189, .func = "set_prompt_req" },
    { .line = 206, .func = "free_req" },
    { .line = 212, .func = "recv_evt" },
    { .line = 238, .func = "recvfrom_evt" },
    { .line = 287, .func = "closed_evt" },
    { .line = 306, .func = "connection_evt" },
    { .line = 344, .func = "strm_setup" },
    { .line = 391, .func = "dgrm_setup" },
    { .line = 446, .func = "console_init" },
    { .line = 481, .func = "console_exit" },
    { .line = 0, .func = NULL }
};
static mrp_debug_file_t file_0 = {
    .file = "plugins/plugin-console.c",
    .info = info_0
};

/* table of all files */
static mrp_debug_file_t *debug_files[] = {
    &file_0,
    NULL
};

#include <murphy/common/debug-auto-register.c>
