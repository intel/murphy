/*
 * Copyright (c) 2012, Intel Corporation
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

#include <murphy/config.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/mainloop.h>

#include "mainloop-qt-test.h"

#include <QCoreApplication>
#include <murphy/common/qt-glue.h>


typedef struct qt_config_s {
    QCoreApplication *app;
    mrp_mainloop_t   *ml;
} qt_config_t ;

static qt_config_t *qt;

mrp_mainloop_t *qt_mainloop_create()
{
    mrp_mainloop_t *ml = NULL;

    if (qt == NULL) {
        int argc = 0;
        char **argv = 0;

        qt = (qt_config_t *)mrp_allocz(sizeof(*qt));
        if (!qt) return NULL;

        qt->app = new QCoreApplication(argc, argv);
        ml      = mrp_mainloop_qt_get();

        if (ml == NULL) {
            delete qt->app;
            mrp_free(qt);
            qt = NULL;
        }
    }
    else {
        return qt->ml;
    }

    return ml;
}

int qt_mainloop_run()
{
    if (qt != NULL) {
        QCoreApplication::exec();
        return TRUE;
    }
    else
        return FALSE;
}

int qt_mainloop_quit()
{
    if (qt != NULL) {
        QCoreApplication::quit();
        return TRUE;
    }
    else
        return FALSE;
}

int qt_mainloop_cleanup(mrp_mainloop_t *ml)
{
    if (qt != NULL) {
        mrp_mainloop_unregister(ml);
        mrp_mainloop_destroy(ml);

        delete qt->app;
        mrp_free(qt);
        qt = NULL;

        return TRUE;
    }
    else
        return FALSE;
}
