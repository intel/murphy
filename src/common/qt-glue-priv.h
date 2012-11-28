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
#ifndef __MURPHY_QT_GLUE_PRIV_H_
#define __MURPHY_QT_GLUE_PRIV_H_

#include <murphy/config.h>

#ifdef QT_ENABLED

#include <sys/types.h>
#include <sys/socket.h>

#include <QSocketNotifier>
#include <QTimer>
#include <murphy/common/mainloop.h>

class QtGlue : public QObject
{
Q_OBJECT

public:
   QtGlue(QObject *parent = NULL);
};

class QtIO : public QObject
{
    Q_OBJECT

public:
    enum Event {
        Read      = 0x01,
        Write     = 0x02,
        Exception = 0x04
    };
    Q_DECLARE_FLAGS(EventMask, Event)

    QtIO (int fd, EventMask events, QObject *parent = NULL);
    ~QtIO();

public Q_SLOTS:
    void readyRead (int fd);
    void readyWrite (int fd);
    void exception (int fd);

private:
    EventMask        m_events;
    QSocketNotifier *m_fdIn;
    QSocketNotifier *m_fdOut;
    QSocketNotifier *m_fdExcep;

public:
    void (*cb)(void *glue_data,
               void *id, int fd, mrp_io_event_t events,
               void *user_data);
    void *user_data;
};
Q_DECLARE_OPERATORS_FOR_FLAGS (QtIO::EventMask)

class QtTimer : public QObject
{
Q_OBJECT

public:
    QtTimer (int msecs, QObject *parent = NULL);
    ~QtTimer ();

    void setInterval (int msecs);
    void start ();
    void stop ();
    void enable ();
    void disable();

private Q_SLOTS:
    void timedout();

private:
   QTimer *m_timer;
   int     m_interval;
   bool    m_disabled;

public:
    void (*cb)(void *glue_data, void *id, void *user_data);
    void *user_data;
};

#endif

#endif /* __MURPHY_QT_GLUE_PRIV_H_ */
