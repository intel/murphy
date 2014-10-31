#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <murphy/common/file-utils.h>

int main(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        printf("Trying to create directory '%s'..\n", argv[i]);
        if (mrp_mkdir(argv[i], 0755) < 0)
            printf("failed (%d: %s)\n", errno, strerror(errno));
        else
            printf("ok\n");
    }

    return 0;
}
