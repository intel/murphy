pkglib_LTLIBRARIES = libmqi.la

LINKER_SCRIPT = linker-script.mqi

libmqi_la_CFLAGS = -I../include

libmqi_ladir     = \
		$(includedir)/murphy-db

libmqi_la_HEADERS = \
		../include/murphy-db/mqi.h

libmqi_la_SOURCES = \
		$(libmqi_ls_HEADERS) \
		mqi.c db.h mdb-backend.h mdb-backend.c

libmqi_la_LDFLAGS =		\
		-Wl,-version-script=$(LINKER_SCRIPT)
#		-version-info @MURPHYDB_VERSION_INFO@

libmqi_la_DEPENDENCIES = $(LINKER_SCRIPT)

# linker script generation
$(LINKER_SCRIPT): $(libmqi_la_HEADERS)
	$(top_builddir)/build-aux/gen-linker-script -p "^mqi_" -o $@ $^

clean-$(LINKER_SCRIPT):
	-rm -f $(LINKER_SCRIPT)

# cleanup
clean-local:: clean-$(LINKER_SCRIPT)
	rm -f *~
