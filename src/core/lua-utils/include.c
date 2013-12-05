/*
 * Copyright (c) 2012, 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/file-utils.h>

#include <murphy/core/lua-utils/include.h>


/*
 * tracking data for include-once files
 */

typedef struct {
    mrp_list_hook_t hook;                /* to list of files included */
    dev_t           dev;                 /* device id */
    ino_t           ino;                 /* file id */
} file_t;


static inline int once_included(mrp_list_hook_t *files, dev_t dev, ino_t ino)
{
    mrp_list_hook_t *p, *n;
    file_t          *f;

    if (files == NULL)
        return FALSE;

    mrp_list_foreach(files, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);

        if (f->dev == dev && f->ino == ino)
            return TRUE;
    }

    return FALSE;
}


static int save_included(mrp_list_hook_t *files, const char *path, dev_t dev,
                         ino_t ino)
{
    file_t *f;

    MRP_UNUSED(path);

    if (files == NULL)
        return -1;

    if ((f = mrp_allocz(sizeof(*f))) == NULL)
        return -1;

    mrp_list_init(&f->hook);
    f->dev = dev;
    f->ino = ino;
    mrp_list_append(files, &f->hook);

    return 0;
}


int mrp_lua_include_file(lua_State *L, const char *file, const char **dirs,
                         mrp_list_hook_t *files)
{
    struct stat st;
    char        path[PATH_MAX];

    if (mrp_find_file(file, dirs, R_OK, path, sizeof(path)) < 0)
        return -1;

    if (files != NULL) {
        if (stat(path, &st) < 0)
            return -1;

        if (once_included(files, st.st_dev, st.st_ino))
            return 0;
    }

    mrp_debug("file '%s' resolved to '%s' for inclusion", file, path);

    if (!luaL_loadfile(L, path) && !lua_pcall(L, 0, 0, 0)) {
        if (files != NULL)
            save_included(files, path, st.st_dev, st.st_ino);

        return 0;
    }

    errno = EINVAL;
    return -1;
}
