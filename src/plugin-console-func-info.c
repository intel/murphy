#include <stdlib.h>
#include <murphy/common/debug.h>

/* plugins/plugin-console.c */
static mrp_debug_info_t info_0[] = {
    { .line = 52, .func = "write_req" },
    { .line = 75, .func = "tcp_close_req" },
    { .line = 87, .func = "udp_close_req" },
    { .line = 106, .func = "set_prompt_req" },
    { .line = 123, .func = "free_req" },
    { .line = 129, .func = "recv_evt" },
    { .line = 155, .func = "recvfrom_evt" },
    { .line = 204, .func = "closed_evt" },
    { .line = 221, .func = "connection_evt" },
    { .line = 257, .func = "strm_setup" },
    { .line = 304, .func = "dgrm_setup" },
    { .line = 360, .func = "console_init" },
    { .line = 395, .func = "console_exit" },
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
