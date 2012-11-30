AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

noinst_PROGRAMS  = mm-test hash-test msg-test transport-test internal-transport-test
if DBUS_ENABLED
noinst_PROGRAMS += mainloop-test dbus-test
endif

# memory management test
mm_test_SOURCES = mm-test.c
mm_test_CFLAGS  = $(AM_CFLAGS)
mm_test_LDADD   = ../../libmurphy-common.la

# hash table test
hash_test_SOURCES = hash-test.c
hash_test_CFLAGS  = $(AM_CFLAGS)
hash_test_LDADD   = ../../libmurphy-common.la

# mainloop test
mainloop_test_SOURCES = mainloop-test.c
mainloop_test_CFLAGS  = $(AM_CFLAGS) $(GLIB_CFLAGS) $(DBUS_CFLAGS)
mainloop_test_LDADD   = ../../libmurphy-common.la $(GLIB_LIBS) $(DBUS_LIBS)
if PULSE_ENABLED
mainloop_test_CFLAGS += $(PULSE_CFLAGS)
mainloop_test_LDADD  += ../../libmurphy-pulse.la $(PULSE_LIBS)
endif
if ECORE_ENABLED
mainloop_test_CFLAGS += $(ECORE_CFLAGS)
mainloop_test_LDADD  += ../../libmurphy-ecore.la $(ECORE_LIBS)
endif
if GLIB_ENABLED
mainloop_test_CFLAGS += $(GLIB_CFLAGS)
mainloop_test_LDADD  += ../../libmurphy-glib.la $(GLIB_LIBS)
endif

noinst_LTLIBRARIES = libmainloop-qt-test.la
libmainloop_qt_test_la_SOURCES  = mainloop-qt-test.cpp
libmainloop_qt_test_la_CPPFLAGS = $(AM_CFLAGS) $(QTCORE_CFLAGS)
libmainloop_qt_test_la_LIBADD   = ../../libmurphy-common.la
mainloop_test_LDADD            += libmainloop-qt-test.la

if QT_ENABLED
libmainloop_qt_test_la_LIBADD  += ../../libmurphy-qt.la $(QTCORE_LIBS)
mainloop_test_LDADD            += ../../libmurphy-qt.la $(QTCORE_LIBS)
endif

# msg test
msg_test_SOURCES = msg-test.c
msg_test_CFLAGS  = $(AM_CFLAGS)
msg_test_LDADD   = ../../libmurphy-common.la

# transport test
transport_test_SOURCES = transport-test.c
transport_test_CFLAGS  = $(AM_CFLAGS)
transport_test_LDADD   = ../../libmurphy-common.la

# internal transport test
internal_transport_test_SOURCES = internal-transport-test.c
internal_transport_test_CFLAGS  = $(AM_CFLAGS)
internal_transport_test_LDADD   = ../../libmurphy-common.la

if DBUS_ENABLED
transport_test_LDADD  += ../../libmurphy-dbus.la

# DBUS test
dbus_test_SOURCES = dbus-test.c
dbus_test_CFLAGS  = $(AM_CFLAGS) $(DBUS_CFLAGS)
dbus_test_LDADD   = ../../libmurphy-dbus.la ../../libmurphy-common.la
endif
