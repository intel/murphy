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
#include <stdbool.h>
#include <dirent.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
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


int mrp_mkdir(const char *path, mode_t mode)
{
    const char *p;
    char       *q, buf[PATH_MAX];
    int         n, undo[PATH_MAX / 2];
    struct stat st;

    if (path == NULL || path[0] == '\0') {
        errno = path ? EINVAL : EFAULT;
        return -1;
    }

    /*
     * Notes:
     *     Our directory creation algorithm logic closely resembles what
     *     'mkdir -p' does. We simply walk the given path component by
     *     component, testing if each one exist. If an existing one is
     *     not a directory we bail out. Missing ones we try to create with
     *     the given mode, bailing out if we fail.
     *
     *     Unlike 'mkdir -p' whenever we fail we clean up by removing
     *     all directories we have created (or at least we try).
     *
     *     Similarly to 'mkdir -p' we don't try to be overly 'smart' about
     *     the path we're handling. Especially we never try to treat '..'
     *     in any special way. This is very much intentional and the idea
     *     is to let the caller try to create a full directory hierarchy
     *     atomically, either succeeeding creating the full hierarchy, or
     *     none of it. To see the consequences of these design choices,
     *     consider what are the possible outcomes of a call like
     *
     *       mrp_mkdir("/home/kli/bin/../sbin/../scripts/../etc/../doc", 0755);
     */

    p = path;
    q = buf;
    n = 0;
    while (1) {
        if (q - buf >= (ptrdiff_t)sizeof(buf) - 1) {
            errno = ENAMETOOLONG;
            goto cleanup;
        }

        if (*p && *p != '/') {
            *q++ = *p++;
            continue;
        }

        *q = '\0';

        mrp_debug("checking/creating '%s'...", buf);

        if (q != buf) {
            if (stat(buf, &st) < 0) {
                if (errno != ENOENT)
                    goto cleanup;

                if (mkdir(buf, mode) < 0)
                    goto cleanup;

                undo[n++] = q - buf;
            }
            else {
                if (!S_ISDIR(st.st_mode)) {
                    errno = ENOTDIR;
                    goto cleanup;
                }
            }
        }

        while (*p == '/')
            p++;

        if (!*p)
            break;

        *q++ = '/';
    }

    return 0;

 cleanup:
    while (--n >= 0) {
        buf[undo[n]] = '\0';
        mrp_debug("cleaning up '%s'...", buf);
        rmdir(buf);
    }

    return -1;
}


char *mrp_normalize_path(char *buf, size_t size, const char *path)
{
    const char *p;
    char       *q;
    int         n, back[PATH_MAX / 2];

    if (path == NULL)
        return NULL;

    if (*path == '\0') {
        if (size > 0) {
            *buf = '\0';
            return buf;
        }
        else {
        overflow:
            errno = ENAMETOOLONG;
            return NULL;
        }
    }

    p   = path;
    q   = buf;
    n   = 0;

    while (*p) {
        if (q - buf + 1 >= (ptrdiff_t)size)
            goto overflow;

        if (*p == '/') {
            back[n++] = q - buf;
            *q++ = *p++;

        skip_slashes:
            while (*p == '/')
                p++;

            /*
             * '.'
             *
             * We skip './' including all trailing slashes. Note that
             * the code is arranged so that whenever we skip trailing
             * slashes, we automatically check and skip also trailing
             * './'s too...
             */

            if (p[0] == '.' && (p[1] == '/' || p[1] == '\0')) {
                p++;
                goto skip_slashes;
            }

            /*
             * '..'
             *
             * We throw away the last incorrectly saved backtracking
             * point (we saved it for this '../'). Then if we can still
             * backtrack, we do so. Otherwise (we're at the beginning
             * already), if the path is absolute, we just ignore the
             * current '../' (can't go above '/'), otherwise we keep it
             * for relative pathes.
             */

            if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) {
                n--;                                /* throw away */
                if (n > 0) {                        /* can still backtrack */
                    if (back[n - 1] >= 0)           /* previous not a '..' */
                        q = buf + back[n - 1] + 1;
                }
                else {
                    if (q > buf && buf[0] == '/')   /* for absolute pathes */
                        q = buf + 1;                /*     reset to start */
                    else {                          /* for relative pathes */
                        if (q - buf + 4 >= (ptrdiff_t)size)
                            goto overflow;

                        q[0] = '.';                 /*     append this '..' */
                        q[1] = '.';
                        q[2] = '/';
                        q += 3;
                        back[n] = -1;               /*     block backtracking */
                    }
                }

                p += 2;
                goto skip_slashes;
            }
        }
        else
            *q++ = *p++;
    }

    /*
     * finally for other than '/' align trailing slashes
     */

    if (p > path + 1 && p[-1] != '/')
        if (q > buf + 1 && q[-1] == '/')
            q--;

    *q = '\0';

    return buf;
}
