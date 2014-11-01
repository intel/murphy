#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/file-utils.h>

int main(int argc, char *argv[])
{
    int   i;
    size_t size;
    char *p, buf[PATH_MAX];
    struct stat ost, nst;

    if (argc > 1) {
        size = strtoul(argv[1], &p, 10);
        if (*p || size > sizeof(buf))
            size = sizeof(buf);
    }
    else
        size = sizeof(buf);

    for (i = 1; i < argc; i++) {
        printf("'%s':\n", argv[i]);
        if ((p = mrp_normalize_path(buf, size, argv[i])) != NULL) {
            printf("    -> '%s'\n", p);

            if (stat(argv[i], &ost) < 0)
                printf("    Non-existing path, can't test in practice...\n");
            else{
                if (stat(buf, &nst) == 0 &&
                    ost.st_dev == nst.st_dev && ost.st_ino == nst.st_ino)
                    printf("    Filesystem-equality check: OK.\n");
                else {
                    printf("    Filesystem-equality check: FAILED\n");
                    exit(1);
                }
            }
        }
        else {
            printf("    failed (%d: %s)\n", errno, strerror(errno));
            exit(1);
        }
    }

    return 0;
}
