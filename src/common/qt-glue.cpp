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

#include "qt-glue-priv.h"
#include "qt-glue-priv.moc.h"
#include <sys/types.h>
#include <sys/socket.h>

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include <murphy/config.h>
#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/qt-glue.h>

static mrp_mainloop_t *qt_ml;


/*
 * QtGlue
 */

QtGlue::QtGlue(QObject *parent)
    : QObject(parent)
{
}


/*
 * QtIO
 */

QtIO::QtIO (int fd, EventMask events, QObject *parent)
    : QObject (parent)
    , m_events(events), m_fdIn(0), m_fdOut(0), m_fdExcep(0)
    , cb(0), user_data(0)
{
    if (events & Read)  {
        m_fdIn = new QSocketNotifier(fd, QSocketNotifier::Read, this);

        m_fdIn->setEnabled (true);

        QObject::connect (m_fdIn, SIGNAL(activated(int)), this,
                          SLOT(readyRead(int)));
    }
    if (events & Write) {
        m_fdOut = new QSocketNotifier(fd, QSocketNotifier::Write, this);

        m_fdOut->setEnabled (true);

        QObject::connect (m_fdOut, SIGNAL(activated(int)), this,
                          SLOT(readyWrite(int)));
    }
    if (events & Exception) {
        m_fdExcep = new QSocketNotifier(fd, QSocketNotifier::Exception, this);

        m_fdExcep->setEnabled (true);

        QObject::connect (m_fdExcep, SIGNAL(activated(int)), this,
                          SLOT(exception(int)));
    }
}


QtIO::~QtIO() {
    if (m_fdIn)
        delete m_fdIn;
    if (m_fdOut)
        delete m_fdOut;
    if (m_fdExcep)
        delete m_fdExcep;
}


void QtIO::readyRead (int fd)
{
    if(cb)
        cb(parent(), this, fd, MRP_IO_EVENT_IN, user_data);
}


void QtIO::readyWrite (int fd)
{
    if (cb)
        cb(parent(), this, fd, MRP_IO_EVENT_OUT, user_data);
}


static bool check_hup(int fd)
{
    char    buf[1];
    ssize_t n;

    n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK);

    if (n == 0)
        return true;
    else
        return false;
}


void QtIO::exception (int fd)
{
    mrp_io_event_t events;

    if (!check_hup(fd))
        events = MRP_IO_EVENT_HUP;
    else
        events = (mrp_io_event_t)(MRP_IO_EVENT_ERR | MRP_IO_EVENT_HUP);

    if (cb)
        cb(parent(), this, fd, events, user_data);
}


/*
 * QtTimer
 */

QtTimer::QtTimer (int msecs, QObject *parent)
    : QObject (parent)
    , m_timer(new QTimer(this)), m_interval(msecs >= 0 ? msecs : 0)
    , cb(0), user_data(0)
{
    m_timer->setInterval (m_interval);
    m_timer->setSingleShot (false);

    QObject::connect (m_timer, SIGNAL(timeout()), this, SLOT(timedout()));
}


QtTimer::~QtTimer ()
{
    delete m_timer;
}


void QtTimer::setInterval (int msecs)
{
    m_interval = (msecs >= 0 ? msecs : 0);

    m_timer->setInterval (m_interval);

    if (m_timer->isActive()) {
        m_timer->stop ();
        m_timer->start ();
    }
}


void QtTimer::start ()
{
    if (!m_timer->isActive())
        m_timer->start ();
}


void QtTimer::stop ()
{
    if (m_timer->isActive())
        m_timer->stop();
}


void QtTimer::disable()
{
    if (!m_disabled) {
        delete m_timer;

        m_timer = 0;
        m_disabled = true;
    }
}


void QtTimer::enable()
{
    if (m_disabled) {
        m_timer = new QTimer(this);

        setInterval (m_interval);

        connect (m_timer, SIGNAL(timeout()), this, SLOT(timedout()));

        m_timer->start ();
        m_disabled = false;
    }
}


void QtTimer::timedout()
{
    mrp_debug("timer %p latched", this);

    if (cb)
        cb(parent(), this, user_data);
}


static void *add_io(void *glue_data, int fd, mrp_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               mrp_io_event_t events, void *user_data),
                    void *user_data)
{
    QtGlue          *qt_glue = (QtGlue *)glue_data;
    QtIO            *io;
    QtIO::EventMask  mask;

    mask = 0;
    if (events & MRP_IO_EVENT_IN)
        mask |= QtIO::Read;
    if (events & MRP_IO_EVENT_OUT)
        mask |= QtIO::Write;
    if (events & (MRP_IO_EVENT_ERR | MRP_IO_EVENT_HUP))
        mask |= QtIO::Exception;

    io = new QtIO (fd, mask, qt_glue);

    if (io) {
        mrp_debug("added I/O watch %p (events 0x%x) on fd %d", io, events, fd);

        io->cb        = cb;
        io->user_data = user_data;
    }

    return io;
}


static void del_io(void *glue_data, void *id)
{
    QtIO *io = (QtIO *)id;

    MRP_UNUSED(glue_data);

    mrp_debug("deleting I/O watch %p", io);

    delete io;
}


static void *add_timer(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    QtGlue *qt_glue = (QtGlue *)glue_data;
    QtTimer *t      = new QtTimer(msecs, qt_glue);

    mrp_debug("created timer %p with %d msecs interval", t, msecs);

    if (t) {
        t->cb        = cb;
        t->user_data = user_data;
        t->start();
    }

    return t;
}


static void del_timer(void *glue_data, void *id)
{
    QtTimer *t = (QtTimer *)id;

    MRP_UNUSED(glue_data);

    mrp_debug("deleting timer %p", t);

    delete t;
}


static void mod_timer(void *glue_data, void *id, unsigned int msecs)
{
    QtTimer *t = (QtTimer *)id;

    MRP_UNUSED(glue_data);

    mrp_debug("setting timer %p to %d msecs interval", t, msecs);

    if (t != NULL)
        t->setInterval(msecs);
}


static void *add_defer(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data)
{
    QtGlue  *qt_glue = (QtGlue *)glue_data;
    QtTimer *t       = new QtTimer(0, qt_glue);

    mrp_debug("created timer %p", t);

    if (t) {
        t->cb        = cb;
        t->user_data = user_data;
        t->start();
    }

    return t;
}


static void del_defer(void *glue_data, void *id)
{
    QtTimer *t = (QtTimer *)id;

    MRP_UNUSED(glue_data);

    mrp_debug("deleting timer %p", t);

    delete t;
}


static void mod_defer(void *glue_data, void *id, int enabled)
{
    QtTimer *t = (QtTimer *)id;

    MRP_UNUSED(glue_data);

    mrp_debug("%s timer %p", enabled ? "enabling" : "disabling", t);

    if (enabled)
        t->enable();
    else
        t->disable();
}


static void unregister(void *glue_data)
{
    QtGlue *qt_glue = (QtGlue *)glue_data;

    mrp_debug("unregistering mainloop");

    delete qt_glue;
}


int mrp_mainloop_register_with_qt(mrp_mainloop_t *ml)
{
    static mrp_superloop_ops_t qt_ops;
    QtGlue *qt_glue;

    qt_ops.add_io     = add_io;
    qt_ops.del_io     = del_io;
    qt_ops.add_timer  = add_timer;
    qt_ops.del_timer  = del_timer;
    qt_ops.mod_timer  = mod_timer;
    qt_ops.add_defer  = add_defer;
    qt_ops.del_defer  = del_defer;
    qt_ops.mod_defer  = mod_defer;
    qt_ops.unregister = unregister;

    qt_glue = new QtGlue ();

    return mrp_set_superloop(ml, &qt_ops, (void *)qt_glue);
}


int mrp_mainloop_unregister_from_qt(mrp_mainloop_t *ml)
{
    return mrp_mainloop_unregister(ml);
}


mrp_mainloop_t *mrp_mainloop_qt_get(void)
{
    if (qt_ml == NULL) {
        qt_ml = mrp_mainloop_create();

        if (qt_ml != NULL) {
            if (!mrp_mainloop_register_with_qt(qt_ml)) {
                mrp_mainloop_destroy(qt_ml);
                qt_ml = NULL;
            }
        }
    }

    return qt_ml;
}
