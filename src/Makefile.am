SUBDIRS         = . murphy-db common/tests core/tests daemon/tests
AM_CFLAGS       = $(WARNING_CFLAGS) -I$(top_builddir) -DLIBDIR=\"@LIBDIR@\"
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
pkgconfig_DATA  += common/murphy-common.pc


libmurphy_common_ladir      =		\
		$(includedir)/murphy/common

libmurphy_common_la_HEADERS =		\
		common/macros.h		\
		common/list.h		\
		common/log.h		\
		common/mm.h		\
		common/hashtbl.h	\
		common/mainloop.h	\
		common/utils.h		\
		common/file-utils.h

libmurphy_common_la_SOURCES =		\
		common/log.c		\
		common/mm.c		\
		common/hashtbl.c	\
		common/mainloop.c	\
		common/utils.c		\
		common/file-utils.c

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

clean-linker-script::
	-rm -f linker-script.common

###################################
# murphy core library
#

lib_LTLIBRARIES += libmurphy-core.la
EXTRA_DIST      += core/murphy-core.pc
pkgconfig_DATA  += core/murphy-core.pc


libmurphy_core_ladir      =		\
		$(includedir)/murphy/core

libmurphy_core_la_HEADERS =		\
		core/context.h		\
		core/plugin.h

libmurphy_core_la_SOURCES =		\
		core/context.c		\
		core/plugin.c

libmurphy_core_la_CFLAGS  =		\
		$(AM_CFLAGS)

libmurphy_core_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.core \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_core_la_LIBADD  =	\
		libmurphy-common.la -ldl

libmurphy_core_la_DEPENDENCIES = linker-script.core

# core linker script generation
linker-script.core: $(libmurphy_core_la_HEADERS)
	$(top_builddir)/build-aux/gen-linker-script -o $@ $^

clean-linker-script::
	-rm -f linker-script.core

###################################
# murphy plugins
#

BUILTIN_PLUGINS    =
BUILTIN_CFLAGS     = -D__MURPHY_BUILTIN_PLUGIN__ $(AM_CFLAGS)
BUILTIN_LIBS       =

plugin_LTLIBRARIES = 
plugindir          = $(libdir)/murphy/plugins


# test plugin
TEST_PLUGIN_SOURCES = plugins/plugin-test.c
TEST_PLUGIN_CFLAGS  =
TEST_PLUGIN_LIBS    =

if BUILTIN_PLUGIN_TEST
BUILTIN_PLUGINS += $(TEST_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(TEST_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(TEST_PLUGIN_LIBS)
else
plugin_test_la_SOURCES = $(TEST_PLUGIN_SOURCES)
plugin_test_la_CFLAGS  = $(TEST_PLUGIN_CFLAGS) $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_test_la_LDFLAGS = -module -avoid-version
plugin_test_la_LIBADD  = $(TEST_PLUGIN_LIBS)

plugin_LTLIBRARIES    += plugin-test.la
endif

# dbus plugin
DBUS_PLUGIN_SOURCES = plugins/plugin-dbus.c
DBUS_PLUGIN_CFLAGS  = $(DBUS_CFLAGS)
DBUS_PLUGIN_LIBS    = $(DBUS_LIBS)

if BUILTIN_PLUGIN_DBUS
BUILTIN_PLUGINS += $(DBUS_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(DBUS_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(DBUS_PLUGIN_LIBS)
else
plugin_dbus_la_SOURCES = $(DBUS_PLUGIN_SOURCES)
plugin_dbus_la_CFLAGS  = $(DBUS_PLUGIN_CFLAGS) $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_dbus_la_LDFLAGS = -module -avoid-version
plugin_dbus_la_LIBADD  = $(DBUS_PLUGIN_LIBS)

plugin_LTLIBRARIES    += plugin-dbus.la
endif

# glib plugin
GLIB_PLUGIN_SOURCES = plugins/plugin-glib.c
GLIB_PLUGIN_CFLAGS  = $(DBUS_CFLAGS)
GLIB_PLUGIN_LIBS    = $(DBUS_LIBS)

if BUILTIN_PLUGIN_GLIB
BUILTIN_PLUGINS += $(GLIB_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(GLIB_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(GLIB_PLUGIN_LIBS)
else
plugin_glib_la_SOURCES = $(GLIB_PLUGIN_SOURCES)
plugin_glib_la_CFLAGS  = $(GLIB_PLUGIN_CFLAGS) $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_glib_la_LDFLAGS = -module -avoid-version
plugin_glib_la_LIBADD  = $(GLIB_PLUGIN_LIBS)

plugin_LTLIBRARIES    += plugin-glib.la
endif


###################################
# murphy daemon
#

bin_PROGRAMS += murphyd

murphyd_SOURCES =			\
		daemon/daemon.c		\
		daemon/config.c		\
		$(BUILTIN_PLUGINS)

murphyd_CFLAGS  =			\
		$(AM_CFLAGS)		\
		$(BUILTIN_CFLAGS)

murphyd_LDADD  =			\
		libmurphy-core.la	\
		libmurphy-common.la	\
		$(BUILTIN_LIBS)

murphyd_LDFLAGS = -rdynamic


# cleanup
clean-local:: clean-linker-script
	-rm -f *~
