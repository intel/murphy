CHECK_LIBMDB_LOG = check-libmdb.log
CHECK_LIBMQI_LOG = check-libmqi.log
CHECK_LIBMQL_LOG = check-libmql.log

MDB_LIBS = ../mdb/libmdb.la
MQI_LIBS = ../mqi/libmqi.la
MQL_LIBS = ../mql/libmql.la

if HAVE_CHECK
TESTS = check-libmdb check-libmqi check-libmql
else
TESTS =
endif

noinst_PROGRAMS = $(TESTS)

#
# MDB tests
#
check_libmdb_SOURCES = check-libmdb.c
check_libmdb_CFLAGS  = @CHECK_CFLAGS@ -I../include \
                       -DLOGFILE=\"$(CHECK_LIBMDB_LOG)\"
check_libmdb_LDADD   = @CHECK_LIBS@ $(MDB_LIBS)

AM_CFLAGS = -g3 -O0

#
# MQI tests
#
check_libmqi_SOURCES = check-libmqi.c
check_libmqi_CFLAGS  = @CHECK_CFLAGS@ -I../include \
                       -DLOGFILE=\"$(CHECK_LIBMQI_LOG)\"
check_libmqi_LDADD   = @CHECK_LIBS@ $(MQI_LIBS) $(MDB_LIBS) 


#
# MQL tests
#
check_libmql_SOURCES = check-libmql.c
check_libmql_CFLAGS  = @CHECK_CFLAGS@ -I../include \
                       -DLOGFILE=\"$(CHECK_LIBMQL_LOG)\"
check_libmql_LDADD   = @CHECK_LIBS@ $(MQL_LIBS) $(MQI_LIBS) $(MDB_LIBS) 


clean-local:
	rm -f $(CHECK_LIBMDB_LOG) $(CHECK_LIBMQI_LOG) $(CHECK_LIBMQL_LOG) \
              $(TESTS) *~
