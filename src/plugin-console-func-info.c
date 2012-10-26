#include <stdlib.h>
#include <murphy/common/debug.h>

/* plugins/plugin-console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 81, .func = "write_req" },
    { .line = 104, .func = "tcp_close_req" },
    { .line = 116, .func = "udp_close_req" },
    { .line = 135, .func = "set_prompt_req" },
    { .line = 152, .func = "free_req" },
    { .line = 158, .func = "recv_evt" },
    { .line = 184, .func = "recvfrom_evt" },
    { .line = 233, .func = "closed_evt" },
    { .line = 250, .func = "connection_evt" },
    { .line = 286, .func = "strm_setup" },
    { .line = 333, .func = "dgrm_setup" },
    { .line = 389, .func = "console_init" },
    { .line = 424, .func = "console_exit" },
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
