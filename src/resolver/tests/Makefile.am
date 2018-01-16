noinst_PROGRAMS = parser-test
AM_CFLAGS       = $(WARNING_CFLAGS) $(JSON_CFLAGS) \
       -I$(top_srcdir) \
       -I$(top_builddir) \
       -I$(top_srcdir)/src/murphy-db/include


# parser test
parser_test_SOURCES = parser-test.c
parser_test_CFLAGS  = $(AM_CFLAGS)
parser_test_LDADD   = ../../libmurphy-resolver.la   \
                      ../../libmqi.la               \
                      ../../libmdb.la               \
                      ../../libmurphy-common.la
