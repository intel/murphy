AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir)

noinst_PROGRAMS  = signalling-test

# signalling test
signalling_test_SOURCES = signalling-client.c
signalling_test_CFLAGS  = $(AM_CFLAGS)
signalling_test_LDADD   = ../../libmurphy-common.la
