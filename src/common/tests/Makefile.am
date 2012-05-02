AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

noinst_PROGRAMS = mm-test hash-test mainloop-test msg-test tcp-test

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

# TCP transport test
tcp_test_SOURCES = tcp-test.c
tcp_test_CFLAGS  = $(AM_CFLAGS)
tcp_test_LDADD   = ../../libmurphy-common.la
