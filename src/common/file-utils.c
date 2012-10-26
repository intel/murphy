#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/file-utils.h>


static const char *translate_glob(const char *pattern, char *glob, size_t size)
{
    MRP_UNUSED(glob);
    MRP_UNUSED(size);

    /* XXX FIXME: translate pattern to glob-like */

    return pattern;
}


static inline mrp_dirent_type_t dirent_type(mode_t mode)
{
#define MAP_TYPE(x, y) if (S_IS##x(mode)) return MRP_DIRENT_##y

    MAP_TYPE(REG, REG);
    MAP_TYPE(DIR, DIR);
    MAP_TYPE(LNK, LNK);
    MAP_TYPE(CHR, CHR);
    MAP_TYPE(BLK, BLK);
    MAP_TYPE(FIFO, FIFO);
    MAP_TYPE(SOCK, SOCK);

    return MRP_DIRENT_UNKNOWN;

#undef MAP_TYPE
}


int mrp_scan_dir(const char *path, const char *pattern, mrp_dirent_type_t mask,
                 mrp_scan_dir_cb_t cb, void *user_data)
{
    DIR               *dp;
    struct dirent     *de;
    struct stat        st;
    regex_t            regexp;
    const char        *prefix;
    char               glob[1024], file[PATH_MAX];
    size_t             size;
    int                stop;
    mrp_dirent_type_t  type;

    if ((dp = opendir(path)) == NULL)
        return FALSE;

    if (pattern != NULL) {
        prefix = MRP_PATTERN_GLOB;
        size   = sizeof(MRP_PATTERN_GLOB) - 1;

        if (!strncmp(pattern, prefix, size)) {
            pattern = translate_glob(pattern + size, glob, sizeof(glob));

            if (pattern == NULL) {
                closedir(dp);
                return FALSE;
            }
        }
        else {
            prefix = MRP_PATTERN_REGEX;
            size   = sizeof(MRP_PATTERN_REGEX) - 1;

            if (!strncmp(pattern, prefix, size))
                pattern += size;
        }

        if (regcomp(&regexp, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
            closedir(dp);
            return FALSE;
        }
    }

    stop = FALSE;
    while ((de = readdir(dp)) != NULL && !stop) {
        if (pattern != NULL && regexec(&regexp, de->d_name, 0, NULL, 0) != 0)
            continue;

        snprintf(file, sizeof(file), "%s/%s", path, de->d_name);

        if (((mask & MRP_DIRENT_LNK ? lstat : stat))(file, &st) != 0)
            continue;

        type = dirent_type(st.st_mode);
        if (!(type & mask))
            continue;

        stop = !cb(de->d_name, type, user_data);
    }


    closedir(dp);
    if (pattern != NULL)
        regfree(&regexp);

    return TRUE;
}
