noinst_PROGRAMS = parser-test
AM_CFLAGS       = $(WARNING_CFLAGS) -I$(top_builddir) $(JSON_CFLAGS)

# parser test
parser_test_SOURCES = parser-test.c
parser_test_CFLAGS  = $(AM_CFLAGS)
parser_test_LDADD   = ../../libmurphy-resolver.la   \
                      ../../murphy-db/mqi/libmqi.la \
                      ../../murphy-db/mdb/libmdb.la \
                      ../../libmurphy-common.la
