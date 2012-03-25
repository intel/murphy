pkglib_LTLIBRARIES = libmql.la

PARSER_PREFIX   = yy_mql_
AM_YFLAGS       = -p $(PARSER_PREFIX)
LEX_OUTPUT_ROOT = ./lex.$(PARSER_PREFIX)

libmql_la_CFLAGS = -I../include

libmql_la_SOURCES = ../include/murphy-db/mql.h \
                    ../include/murphy-db/statement.h \
                    ../include/murphy-db/result.h \
                    mql-scanner.l mql-parser.y \
                    statement.c result.c

mql-parser.h mql-parser.c: mql-parser.y
	$(YACCCOMPILE) $<
	mv -f y.tab.h mql-parser.h
	mv -f y.tab.c mql-parser.c

mql-scanner.c: mql-scanner.l mql-parser.h
	$(LEXCOMPILE) $<
	mv lex.$(PARSER_PREFIX).c $@


clean-local::
	rm -f *~ mql-scanner.c mql-parser.[hc] *.tab.[hc]
