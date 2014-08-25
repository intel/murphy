pkglib_LTLIBRARIES = libmdb.la

LINKER_SCRIPT = linker-script.mdb
QUIET_GEN     = $(Q:@=@echo '  GEN   '$@;)

libmdb_la_CFLAGS = -I../include

libmdb_ladir     = \
		$(includedir)/murphy-db

libmdb_la_HEADERS = \
		../include/murphy-db/assert.h \
		../include/murphy-db/list.h \
		../include/murphy-db/handle.h \
		../include/murphy-db/hash.h \
		../include/murphy-db/sequence.h \
		../include/murphy-db/mqi-types.h \
		../include/murphy-db/mdb.h

libmdb_la_SOURCES = \
		$(libmdb_la_HEADERS) \
                list.h handle.c hash.c sequence.c mqi-types.c \
                column.h column.c \
                cond.h cond.c \
                index.h index.c \
                log.h log.c \
                row.h row.c \
                table.h table.c \
                transaction.h transaction.c \
                trigger.h trigger.c

libmdb_la_LDFLAGS =		\
		-Wl,-version-script=$(LINKER_SCRIPT)
#		-version-info @MURPHYDB_VERSION_INFO@

libmdb_la_DEPENDENCIES = $(LINKER_SCRIPT)

# linker script generation
$(LINKER_SCRIPT): $(libmdb_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
	    -P "$(CC)" -c "$(libmdb_la_CFLAGS)" -p "^(mdb_)|(mqi_)" -o $@ $^

clean-$(LINKER_SCRIPT):
	-rm -f $(LINKER_SCRIPT)

# cleanup
clean-local:: # clean-$(LINKER_SCRIPT)
	-rm -f *~
