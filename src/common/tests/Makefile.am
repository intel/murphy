AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

noinst_PROGRAMS  = mm-test hash-test hash12-test msg-test transport-test \
                 internal-transport-test process-watch-test native-test \
		 mkdir-test path-test mask-test

if LIBDBUS_ENABLED
noinst_PROGRAMS += mainloop-test dbus-test
endif

noinst_PROGRAMS += fragbuf-test

# memory management test
mm_test_SOURCES = mm-test.c
mm_test_CFLAGS  = $(AM_CFLAGS)
mm_test_LDADD   = ../../libmurphy-common.la

# hash table test
hash_test_SOURCES = hash-test.c
hash_test_CFLAGS  = $(AM_CFLAGS)
hash_test_LDADD   = ../../libmurphy-common.la

# hash12-test
hash12_test_SOURCES = hash12-test.c
hash12_test_CFLAGS  = $(AM_CFLAGS)
hash12_test_LDADD   = ../../libmurphy-common.la

# mainloop test
mainloop_test_SOURCES = mainloop-test.c
mainloop_test_CFLAGS  = $(AM_CFLAGS) $(GLIB_CFLAGS) $(LIBDBUS_CFLAGS)
mainloop_test_LDADD   = ../../libmurphy-common.la $(GLIB_LIBS) $(LIBDBUS_LIBS)
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

if QT_ENABLED
noinst_LTLIBRARIES = libmainloop-qt-test.la
libmainloop_qt_test_la_SOURCES  = mainloop-qt-test.cpp
libmainloop_qt_test_la_CPPFLAGS = $(AM_CFLAGS) $(QTCORE_CFLAGS)
libmainloop_qt_test_la_LIBADD   = ../../libmurphy-common.la \
				  ../../libmurphy-qt.la $(QTCORE_LIBS)
mainloop_test_LDADD            += libmainloop-qt-test.la $(QTCORE_LIBS) -lstdc++
endif

# msg test
msg_test_SOURCES = msg-test.c
msg_test_CFLAGS  = $(AM_CFLAGS)
msg_test_LDADD   = ../../libmurphy-common.la

# native type test
native_test_SOURCES = native-test.c
native_test_CFLAGS  = $(AM_CFLAGS)
native_test_LDADD   = ../../libmurphy-common.la

# transport test
transport_test_SOURCES = transport-test.c
transport_test_CFLAGS  = $(AM_CFLAGS)
transport_test_LDADD   = ../../libmurphy-common.la

# internal transport test
internal_transport_test_SOURCES = internal-transport-test.c
internal_transport_test_CFLAGS  = $(AM_CFLAGS)
internal_transport_test_LDADD   = ../../libmurphy-common.la

# process watch test
process_watch_test_SOURCES = process-test.c
process_watch_test_CFLAGS  = $(AM_CFLAGS)
process_watch_test_LDADD   = ../../libmurphy-common.la

if LIBDBUS_ENABLED
transport_test_LDADD  += ../../libmurphy-libdbus.la

noinst_PROGRAMS += mainloop-test

# DBUS tests
noinst_PROGRAMS    += dbus-test
dbus_test_SOURCES = dbus-test.c
dbus_test_CFLAGS  = $(AM_CFLAGS) $(LIBDBUS_CFLAGS)
dbus_test_LDADD   = ../../libmurphy-libdbus.la ../../libmurphy-common.la $(LIBDBUS_LIBS)

noinst_PROGRAMS    += libdbus-test libdbus-transport-test
libdbus_test_SOURCES = libdbus-test.c
libdbus_test_CFLAGS  = $(AM_CFLAGS) $(LIBDBUS_CFLAGS)
libdbus_test_LDADD   = ../../libmurphy-dbus-libdbus.la ../../libmurphy-common.la $(LIBDBUS_LIBS)

libdbus_transport_test_SOURCES = libdbus-transport-test.c
libdbus_transport_test_CFLAGS  = $(AM_CFLAGS)
libdbus_transport_test_LDADD   = ../../libmurphy-common.la \
                                 ../../libmurphy-dbus-libdbus.la
endif

if SDBUS_ENABLED
noinst_PROGRAMS    += sdbus-test dbus-sdbus-test sdbus-transport-test sdbus-error-message

sdbus_test_SOURCES = sdbus-test.c
sdbus_test_CFLAGS  = $(AM_CFLAGS) $(SDBUS_CFLAGS)
sdbus_test_LDADD   = ../../libmurphy-common.la $(SDBUS_LIBS)

dbus_sdbus_test_SOURCES = dbus-sdbus-test.c
dbus_sdbus_test_CFLAGS  = $(AM_CFLAGS) $(SDBUS_CFLAGS)
dbus_sdbus_test_LDADD   = 				\
			../../libmurphy-common.la	\
			../../libmurphy-dbus-sdbus.la

sdbus_transport_test_SOURCES = libdbus-transport-test.c
sdbus_transport_test_CFLAGS  = $(AM_CFLAGS)
sdbus_transport_test_LDADD   =				\
			../../libmurphy-common.la	\
			../../libmurphy-dbus-sdbus.la

sdbus_error_message_SOURCES = sdbus-error-message.c
sdbus_error_message_CFLAGS  = $(AM_CFLAGS) $(SDBUS_CFLAGS)
sdbus_error_message_LDADD   =				\
			../../libmurphy-common.la	\
			../../libmurphy-dbus-sdbus.la
endif

# fragbuf test
fragbuf_test_SOURCES = fragbuf-test.c
fragbuf_test_CFLAGS  = $(AM_CFLAGS)
fragbuf_test_LDADD   = ../../libmurphy-common.la

# mkdir-test
mkdir_test_SOURCES = mkdir-test.c
mkdir_test_CFLAGS  = $(AM_CFLAGS) -I.
mkdir_test_LDADD   = ../../libmurphy-common.la

# path-test
path_test_SOURCES = path-test.c
path_test_CFLAGS  = $(AM_CFLAGS) -I.
path_test_LDADD   = ../../libmurphy-common.la

mask_test_SOURCES = mask-test.c
mask_test_CFLAGS  = $(AM_CFLAGS) -I.
mask_test_LDADD   = ../../libmurphy-common.la
