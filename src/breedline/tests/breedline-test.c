#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>

#include <breedline/breedline.h>


int main(int argc, char *argv[])
{
    brl_t *brl;
    char   line[1024];
    int    n;

    brl = brl_create(fileno(stdin), argc > 1 ? argv[1] : "breedline");

    if (brl == NULL) {
        fprintf(stderr, "Failed to create breedline context (%d: %s).\n",
                errno, strerror(errno));
        exit(1);
    }

    while ((n = brl_read_line(brl, line, sizeof(line))) >= 0) {
        printf("got line: '%s'\n", line);

        if (!strcmp(line, "exit") || !strcmp(line, "quit"))
            break;
        else {
            if (brl_add_history(brl, line) != 0)
                fprintf(stderr, "Failed to save history entry.\n");
        }
    }

    brl_destroy(brl);

    return 0;
}
