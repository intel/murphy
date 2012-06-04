AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

noinst_PROGRAMS  = mm-test hash-test msg-test transport-test
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

# msg test
msg_test_SOURCES = msg-test.c
msg_test_CFLAGS  = $(AM_CFLAGS)
msg_test_LDADD   = ../../libmurphy-common.la

# transport test
transport_test_SOURCES = transport-test.c
transport_test_CFLAGS  = $(AM_CFLAGS)
transport_test_LDADD   = ../../libmurphy-common.la

if DBUS_ENABLED
transport_test_LDADD  += ../../libmurphy-dbus.la

# DBUS test
dbus_test_SOURCES = dbus-test.c
dbus_test_CFLAGS  = $(AM_CFLAGS) $(DBUS_CFLAGS)
dbus_test_LDADD   = ../../libmurphy-dbus.la ../../libmurphy-common.la
endif
