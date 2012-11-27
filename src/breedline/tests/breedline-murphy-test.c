#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <murphy/common.h>

#include "breedline/breedline-murphy.h"

static void line_cb(brl_t *brl, const char *line, void *user_data)
{
    mrp_mainloop_t *ml = (mrp_mainloop_t *)user_data;

    MRP_UNUSED(brl);

    printf("got line: '%s'\n", line);

    if (!strcmp(line, "exit") || !strcmp(line, "quit"))
        mrp_mainloop_quit(ml, 0);
    else {
        if (brl_add_history(brl, line) != 0)
            fprintf(stderr, "Failed to save history entry.\n");
    }
}


int main(int argc, char *argv[])
{
    mrp_mainloop_t *ml;
    brl_t          *brl;
    int             fd;
    const char     *prompt;

    ml = mrp_mainloop_create();

    if (ml == NULL) {
        fprintf(stderr, "Failed to create mainloop.\n");
        exit(1);
    }

    fd     = fileno(stdin);
    prompt = argc > 1 ? argv[1] : "breedline-murphy";

    brl = brl_create_with_murphy(fd, prompt, ml, line_cb, ml);

    if (brl == NULL) {
        fprintf(stderr, "Failed to create breedline context (%d: %s).\n",
                errno, strerror(errno));
        exit(1);
    }

    brl_show_prompt(brl);
    mrp_mainloop_run(ml);
    brl_destroy(brl);
    mrp_mainloop_destroy(ml);

    return 0;
}
