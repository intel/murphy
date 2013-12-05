/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
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


int mrp_find_file(const char *file, const char **dirs, int mode, char *buf,
                  size_t size)
{
    const char *dir;
    char        path[PATH_MAX];
    int         i;

    if (file[0] != '/') {
        if (dirs != NULL) {
            for (dir = dirs[i=0]; dir != NULL; dir = dirs[++i]) {
                if (snprintf(path, sizeof(path), "%s/%s",
                             dir, file) >= (ssize_t)sizeof(path))
                    continue;

                if (access(path, mode) == 0) {
                    file = path;
                    goto found;
                }
            }
        }

        if (snprintf(path, sizeof(path), "./%s", file) < (ssize_t)sizeof(path)) {
            if (access(path, mode) == 0) {
                file = path;
                goto found;
            }
        }
    }
    else {
        if (access(file, mode) == 0)
            goto found;
    }

    errno = ENOENT;
    return -1;

 found:
    if (buf != NULL && size > 0)
        snprintf(buf, size, "%s", file);

    return 0;
}
