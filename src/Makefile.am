SUBDIRS         = . common/tests core/tests daemon/tests
AM_CFLAGS       = $(WARNING_CFLAGS) -I$(top_builddir)
MURPHY_CFLAGS   = 
pkgconfigdir    = ${libdir}/pkgconfig

bin_PROGRAMS    =
lib_LTLIBRARIES =
pkgconfig_DATA  =
EXTRA_DIST      =


###################################
# murphy common library
#

lib_LTLIBRARIES += libmurphy-common.la
EXTRA_DIST      += common/murphy-common.pc
#pkgconfig_DATA += common/murphy-common.pc


libmurphy_common_ladir      =		\
		$(includedir)/murphy/common

libmurphy_common_la_HEADERS =		\
		common/macros.h		\
		common/list.h		\
		common/log.h		\
		common/mm.h		\
		common/hashtbl.h	\
		common/mainloop.h

libmurphy_common_la_SOURCES =		\
		common/log.c		\
		common/mm.c		\
		common/hashtbl.c	\
		common/mainloop.c

libmurphy_common_la_CFLAGS  =		\
		$(AM_CFLAGS)

libmurphy_common_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.common \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_common_la_LIBADD  = 		\
		-lrt

libmurphy_common_la_DEPENDENCIES = linker-script.common

libcommonincludedir      = $(includedir)/murphy/common
libcommoninclude_HEADERS = $(libmurphy_common_la_HEADERS)

# linker script generation
linker-script.common: $(libmurphy_common_la_HEADERS)
	$(top_builddir)/build-aux/gen-linker-script -o $@ $^


###################################
# murphy core library
#

lib_LTLIBRARIES += libmurphy-core.la
EXTRA_DIST      += core/murphy-core.pc
#pkgconfig_DATA  += core/murphy-core.pc


libmurphy_core_ladir      =		\
		$(includedir)/murphy/core

libmurphy_core_la_HEADERS =		\
		core/context.h		


libmurphy_core_la_SOURCES =		\
		core/context.c		


libmurphy_core_la_CFLAGS  =		\
		$(AM_CFLAGS)

libmurphy_core_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.core \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_core_la_LIBADD  =	\
		libmurphy-common.la -ldl

libmurphy_core_la_DEPENDENCIES = linker-script.core

# linker script generation
linker-script.core: $(libmurphy_core_la_HEADERS)
	$(top_builddir)/build-aux/gen-linker-script -o $@ $^

