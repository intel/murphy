INCLUDES  = -I$(top_builddir)/src/murphy-db/include -I$(top_builddir)
AM_CFLAGS = $(WARNING_CFLAGS) $(INCLUDES)


noinst_PROGRAMS  = decision-test

# lua decision network test
decision_test_SOURCES = decision-test.c
decision_test_CFLAGS  = $(AM_CFLAGS) $(LUA_CFLAGS)
decision_test_LDADD   =	../../../libmurphy-resolver.la		\
			../../../libmurphy-common.la 		\
			../../../libmurphy-core.la   		\
			../../../libmurphy-lua-utils.la		\
			../../../libmurphy-lua-decision.la	\
			$(LUA_LIBS)
