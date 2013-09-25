/*
 * Copyright (c) 2013, Intel Corporation
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
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <murphy/common.h>
#include <murphy/core.h>
#include <murphy/common/dbus-sdbus.h>

static int msg_cb(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, void *data)
{
    mrp_dbus_err_t err;
    mrp_dbus_msg_t *reply;
    const char *member = mrp_dbus_msg_member(msg);
    const char *iface = mrp_dbus_msg_interface(msg);
    const char *path = mrp_dbus_msg_path(msg);

    MRP_UNUSED(data);

    printf("Message callback called -- member: '%s', path: '%s',"
            " interface: '%s'\n", member, path, iface);

    mrp_dbus_error_init(&err);
    mrp_dbus_error_set(&err, "org.freedesktop.DBus.Error.Failed", "Error message");

    reply = mrp_dbus_msg_error(dbus, msg, &err);

    if (reply) {
        mrp_dbus_send_msg(dbus, reply);
        mrp_dbus_msg_unref(reply);
    }
    return TRUE;
}

int main()
{
    mrp_dbus_t *dbus;
    mrp_mainloop_t *ml;

    ml = mrp_mainloop_create();

    if (!(dbus = mrp_dbus_connect(ml, "session", NULL))) {
        printf("Failed to connect to D-Bus\n");
    }

    if (!mrp_dbus_acquire_name(dbus, "org.example", NULL)) {
        printf("Failed to acquire name on D-Bus\n");
        goto error;
    }

    if (!mrp_dbus_export_method(dbus, "/example", "org.example", "member",
                msg_cb, NULL)) {
       printf("Failed to register method\n");
       goto error;
    }

    printf("waiting for 'dbus-send --session --print-reply --type=method_call"
            "--dest=org.example /example org.example.member'\n");

    mrp_mainloop_run(ml);

    return 0;

error:
    return 1;
}
