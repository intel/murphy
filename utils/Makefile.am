AM_CFLAGS       = $(WARNING_CFLAGS) -I$(top_builddir)
noinst_PROGRAMS = collect-symbols

collect_symbols_SOURCES = collect-symbols.c
collect_symbols_CFLAGS  = $(AM_CFLAGS)
collect_symbols_LDADD   =

collect-symbols$(EXEEXT): $(collect_symbols_SOURCES)
	$(CC_FOR_BUILD) $(collect_symbols_CFLAGS) -o $@ $< \
		$(collect_symbols_LDADD)
