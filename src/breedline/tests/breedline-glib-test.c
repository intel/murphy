#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>

#include <glib.h>

#include "breedline/breedline-glib.h"

static void line_cb(brl_t *brl, const char *line, void *user_data)
{
    GMainLoop *ml = (GMainLoop *)user_data;

    (void)brl;

    printf("got line: '%s'\n", line);

    if (!strcmp(line, "exit") || !strcmp(line, "quit"))
        g_main_loop_quit(ml);
    else {
        if (brl_add_history(brl, line) != 0)
            fprintf(stderr, "Failed to save history entry.\n");
    }
}


int main(int argc, char *argv[])
{
    GMainLoop  *ml;
    brl_t      *brl;
    int         fd;
    const char *prompt;

    ml = g_main_loop_new(NULL, FALSE);

    if (ml == NULL) {
        fprintf(stderr, "Failed to create mainloop.\n");
        exit(1);
    }

    fd     = fileno(stdin);
    prompt = argc > 1 ? argv[1] : "breedline-glib";

    brl = brl_create_with_glib(fd, prompt, ml, line_cb, ml);

    if (brl == NULL) {
        fprintf(stderr, "Failed to create breedline context (%d: %s).\n",
                errno, strerror(errno));
        exit(1);
    }

    brl_show_prompt(brl);
    g_main_loop_run(ml);
    brl_destroy(brl);
    g_main_loop_unref(ml);

    return 0;
}
