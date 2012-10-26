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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/log.h>
#include <murphy/common/utils.h>

#define MSG_OK "OK"

static int notify_parent(int fd, const char *fmt, ...)
{
    va_list ap;
    int     len;

    va_start(ap, fmt);
    len = vdprintf(fd, fmt, ap);
    va_end(ap);

    return (len > 0);
}


int mrp_daemonize(const char *dir, const char *new_out, const char *new_err)
{
    pid_t pid;
    int   in, out, err;
    char  msg[1024];
    int   chnl[2], len;

    /*
     * create a pipe for communicating back the child status
     */

    if (pipe(chnl) == -1) {
        mrp_log_error("Failed to create pipe to get child status (%d: %s).",
                      errno, strerror(errno));
        return FALSE;
    }


    /*
     * fork, change to our new working directory and create a new session
     */

    switch ((pid = fork())) {
    case -1: /* failed */
        mrp_log_error("Could not daemonize, fork failed (%d: %s).",
                      errno, strerror(errno));
        return FALSE;

    case 0:  /* child */
        close(chnl[0]);
        break;

    default: /* parent */
        close(chnl[1]);

        /*
         * wait for and check the status report from the child
         */

        len = read(chnl[0], msg, sizeof(msg) - 1);

        if (len > 0) {
            msg[len] = '\0';

            if (!strcmp(msg, MSG_OK)) {
                mrp_log_info("Successfully daemonized.");
                exit(0);
            }
            else
                mrp_log_error("Daemonizing failed after fork: %s.", msg);
        }
        else
            mrp_log_error("Daemonizing failed in forked child.");

        return FALSE;
    }


    if (chdir(dir) != 0) {
        mrp_log_error("Could not daemonize, failed to chdir to %s (%d: %s).",
                      dir, errno, strerror(errno));
        return FALSE;
    }

    if (setsid() < 0) {
        notify_parent(chnl[1], "Failed to create new session (%d: %s).",
                      errno, strerror(errno));
        exit(1);
    }


    /*
     * fork again and redirect our stdin, stdout, and stderr
     */

    switch ((pid = fork())) {
    case -1: /* failed */
        notify_parent(chnl[1], "Could not daemonize, fork failed (%d: %s).",
                      errno, strerror(errno));
        exit(1);

    case 0: /* child */
        break;

    default: /* parent */
        close(chnl[1]);
        exit(0);
    }


    if ((in = open("/dev/null", O_RDONLY)) < 0) {
        notify_parent(chnl[1], "Failed to open /dev/null (%d: %s).", errno,
                      strerror(errno));
        exit(1);
    }

    if ((out = open(new_out, O_WRONLY)) < 0) {
        notify_parent(chnl[1], "Failed to open %s (%d: %s).", new_out, errno,
                      strerror(errno));
        exit(1);
    }

    if ((err = open(new_err, O_WRONLY)) < 0) {
        notify_parent(chnl[1], "Failed to open %s (%d: %s).", new_err, errno,
                      strerror(errno));
        exit(1);
    }

    if (dup2(in, fileno(stdin)) < 0) {
        notify_parent(chnl[1], "Failed to redirect stdin (%d: %s).", errno,
                     strerror(errno));
        exit(1);
    }

    if (dup2(out, fileno(stdout)) < 0) {
        notify_parent(chnl[1], "Failed to redirect stdout (%d: %s).", errno,
                     strerror(errno));
        exit(1);
    }

    if (dup2(err, fileno(stderr)) < 0) {
        notify_parent(chnl[1], "Failed to redirect stderr (%d: %s).", errno,
                     strerror(errno));
        exit(1);
    }

    close(in);
    close(out);
    close(err);

    notify_parent(chnl[1], "%s", MSG_OK);

    return TRUE;
}


int mrp_string_comp(const void *key1, const void *key2)
{
    return strcmp(key1, key2);
}


uint32_t mrp_string_hash(const void *key)
{
    uint32_t    h;
    const char *p;

    for (h = 0, p = key; *p; p++) {
        h <<= 1;
        h  ^= *p;
    }

    return h;
}
