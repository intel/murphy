AM_CPPFLAGS = -I$(top_builddir)/src/murphy-db/include -I$(top_builddir)
AM_CFLAGS   = $(WARNING_CFLAGS) $(AM_CPPFLAGS)


noinst_PROGRAMS  = decision-test

# lua decision network test
decision_test_SOURCES = decision-test.c
decision_test_CFLAGS  = $(AM_CFLAGS) $(LUA_CFLAGS)
decision_test_LDADD   =	../../../libmurphy-lua-utils.la		\
			../../../libmurphy-lua-decision.la	\
			../../../libmurphy-resolver.la		\
			../../../libmurphy-core.la   		\
			../../../libmurphy-common.la 		\
			../../../murphy-db/mql/libmql.la        \
			../../../murphy-db/mqi/libmqi.la        \
			../../../murphy-db/mdb/libmdb.la        \
			$(LUA_LIBS)
