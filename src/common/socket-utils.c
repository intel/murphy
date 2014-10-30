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

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <murphy/common/macros.h>

static int reject_fd = -1;


static inline int reserve_reject_fd(void)
{
    if (reject_fd < 0)
        reject_fd = open("/dev/null", O_RDONLY);

    return reject_fd;
}


static void MRP_INIT reserve_reject(void)
{
    reserve_reject_fd();
}


int mrp_reject_connection(int sock, struct sockaddr *addr, socklen_t *alen)
{
    struct sockaddr *a, buf;
    socklen_t       *l, len;
    int              fd;

    if (addr != NULL) {
        a = addr;
        l = alen;
    }
    else {
        len = sizeof(buf);
        a   = &buf;
        l   = &len;
    }

    fd = accept(sock, a, l);

    if (fd >= 0) {
        close(fd);

        return 0;
    }

    if (errno != EMFILE)
        return -1;

    if (reject_fd < 0) {
        errno = ENOENT;

        return -1;
    }

    close(reject_fd);
    reject_fd = -1;

    fd = accept(sock, a, l);

    if (fd >= 0)
        close(fd);

    reserve_reject_fd();

    return (fd >= 0 ? 0 : -1);
}
