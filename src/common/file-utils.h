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

#ifndef __MURPHY_FILEUTILS_H__
#define __MURPHY_FILEUTILS_H__

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Routines for scanning a directory for matching entries.
 */

/** Directory entry types. */
typedef enum {
    MRP_DIRENT_UNKNOWN = 0,                      /* unknown */
    MRP_DIRENT_NONE = 0x00,                      /* unknown */
    MRP_DIRENT_FIFO = 0x01,                      /* FIFO */
    MRP_DIRENT_CHR  = 0x02,                      /* character device */
    MRP_DIRENT_DIR  = 0x04,                      /* directory */
    MRP_DIRENT_BLK  = 0x08,                      /* block device */
    MRP_DIRENT_REG  = 0x10,                      /* regular file */
    MRP_DIRENT_LNK  = 0x20,                      /* symbolic link */
    MRP_DIRENT_SOCK = 0x40,                      /* socket */
    MRP_DIRENT_ANY  = 0xff,                      /* mask of all types */
} mrp_dirent_type_t;


#define MRP_PATTERN_GLOB  "glob:"                /* a globbing pattern */
#define MRP_PATTERN_REGEX "regex:"               /* a regexp pattern */


/** Directory scanning callback type. */
typedef int (*mrp_scan_dir_cb_t)(const char *entry, mrp_dirent_type_t type,
                                 void *user_data);

/** Scan a directory, calling cb with all matching entries. */
int mrp_scan_dir(const char *path, const char *pattern, mrp_dirent_type_t mask,
                 mrp_scan_dir_cb_t cb, void *user_data);

/** Do an #include-like search for the given file among the given dirs. */
int mrp_find_file(const char *file, const char **dirs, int mode, char *buf,
                  size_t size);

/** Create a directory, creating leading path as necessary. */
int mrp_mkdir(const char *path, mode_t mode);


/** Parse a path into a normalized form, removing ../'s and ./'s. */
char *mrp_normalize_path(char *buf, size_t size, const char *path);

#endif /* __MURPHY_FILEUTILS_H__ */
