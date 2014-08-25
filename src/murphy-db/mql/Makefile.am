pkglib_LTLIBRARIES = libmql.la

PARSER_PREFIX   = yy_mql_
AM_YFLAGS       = -p $(PARSER_PREFIX)
LEX_OUTPUT_ROOT = ./lex.$(PARSER_PREFIX)
BUILT_SOURCES   = mql-scanner.c mql-parser.c

LINKER_SCRIPT = linker-script.mql
QUIET_GEN     = $(Q:@=@echo '  GEN   '$@;)

libmql_la_CFLAGS = -I../include

libmql_ladir     = \
		$(includedir)/murphy-db

libmql_la_HEADERS = \
		../include/murphy-db/mql.h \
		../include/murphy-db/mql-statement.h \
		../include/murphy-db/mql-result.h \
		../include/murphy-db/mql-trigger.h

libmql_la_SOURCES = \
		$(libmql_la_HEADERS) \
		mql-scanner.l mql-parser.y \
		statement.c result.c trigger.c transaction.c

libmql_la_LDFLAGS =		\
		-Wl,-version-script=$(LINKER_SCRIPT)
#		-version-info @MURPHYDB_VERSION_INFO@

libmql_la_LIBADD = ../mqi/libmqi.la ../mdb/libmdb.la

libmql_la_DEPENDENCIES = $(LINKER_SCRIPT)


mql-parser.h mql-parser.c: mql-parser.y
	$(YACCCOMPILE) $<
	mv -f y.tab.h mql-parser.h
	mv -f y.tab.c mql-parser.c

mql-scanner.c: mql-scanner.l mql-parser.c
	$(LEXCOMPILE) $<
	mv lex.$(PARSER_PREFIX).c $@

clean-parser:
	-rm -f mql-parser.[hc] *.tab.[hc]

clean-scanner:
	-rm -f mql-scanner.c

# linker script generation
$(LINKER_SCRIPT): $(libmql_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
	    -P "$(CC)" -c "$(libmql_la_CFLAGS)" -p "^mql_" -o $@ $^

clean-$(LINKER_SCRIPT):
	-rm -f $(LINKER_SCRIPT)

# cleanup
clean-local:: clean-parser clean-scanner # clean-$(LINKER_SCRIPT)
	rm -f *~
