SUBDIRS         = murphy-db . \
		  common/tests breedline/tests core/tests \
		  core/lua-decision/tests resolver/tests \
		  daemon/tests  plugins/tests

AM_CFLAGS       = $(WARNING_CFLAGS) $(AM_CPPFLAGS) \
		  -DSYSCONFDIR=\"@SYSCONFDIR@\" -DLIBDIR=\"@LIBDIR@\"
MURPHY_CFLAGS   =
pkgconfigdir    = ${libdir}/pkgconfig

bin_PROGRAMS    =
lib_LTLIBRARIES =
pkgconfig_DATA  =
EXTRA_DIST      =

QUIET_GEN      = $(Q:@=@echo '  GEN   '$@;)

LEXCOMPILE      = $(LEX) $(LFLAGS) $(AM_LFLAGS)
YACCCOMPILE     = $(YACC) $(YFLAGS) $(AM_YFLAGS)

AM_CPPFLAGS     = -I$(top_builddir) -I$(top_builddir)/src \
		  -I$(top_builddir)/src/murphy-db/include
BUILT_SOURCES   =

###################################
# murphy common library
#

lib_LTLIBRARIES += libmurphy-common.la
EXTRA_DIST      += common/murphy-common.pc
pkgconfig_DATA  += common/murphy-common.pc


libmurphy_commonh_ladir      =		\
		$(includedir)/murphy

libmurphy_commonh_la_HEADERS =		\
		common.h		\
		config.h

libmurphy_common_ladir      =		\
		$(includedir)/murphy/common

libmurphy_common_la_HEADERS =		\
		common/macros.h		\
		common/list.h		\
		common/log.h		\
		common/debug.h 		\
		common/debug-info.h	\
		common/env.h		\
		common/mm.h		\
		common/hashtbl.h	\
		common/process.h	\
		common/mainloop.h	\
		common/utils.h		\
		common/file-utils.h	\
		common/socket-utils.h	\
		common/msg.h		\
		common/refcnt.h		\
		common/fragbuf.h	\
		common/json.h		\
		common/transport.h	\
		common/tlv.h		\
		common/native-types.h	\
		common/mask.h

libmurphy_common_la_REGULAR_SOURCES =		\
		common/log.c			\
		common/debug.c			\
		common/env.c			\
		common/mm.c			\
		common/hashtbl.c		\
		common/mainloop.c		\
		common/utils.c			\
		common/file-utils.c		\
		common/socket-utils.c		\
		common/process.c 		\
		common/msg.c			\
		common/fragbuf.c		\
		common/json.c			\
		common/transport.c		\
		common/stream-transport.c	\
		common/internal-transport.c	\
		common/dgram-transport.c	\
		common/tlv.c			\
		common/native-types.c

libmurphy_common_la_SOURCES =				\
		$(libmurphy_common_la_REGULAR_SOURCES)

libmurphy_common_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(JSON_CFLAGS)

libmurphy_common_la_LDFLAGS =					\
		-Wl,-version-script=linker-script.common	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_common_la_LIBADD  = 		\
		$(JSON_LIBS)		\
		-lrt

libmurphy_common_la_DEPENDENCIES =	\
		linker-script.common	\
		$(filter %.la, $(libmurphy_common_la_LIBADD))

libcommonincludedir      = $(includedir)/murphy/common
libcommoninclude_HEADERS = $(libmurphy_common_la_HEADERS)

if WEBSOCKETS_ENABLED
libmurphy_common_la_HEADERS +=			\
		common/websocklib.h		\
		common/websocket.h

libmurphy_common_la_REGULAR_SOURCES +=		\
		common/websocklib.c		\
		common/websocket.c		\
		common/wsck-transport.c

libmurphy_common_la_CFLAGS +=			\
		$(WEBSOCKETS_CFLAGS)

libmurphy_common_la_LIBADD +=			\
		$(WEBSOCKETS_LIBS)

EXTRA_DIST     += $(webconsole_DATA);
webconsoledir   = $(datadir)/murphy/webconsole
webconsole_DATA = 				\
		plugins/console/console.js	\
		plugins/console/console.html
endif


# linker script generation
linker-script.common: $(libmurphy_common_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_common_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.common

###################################
# murphy lua utilities
#

lib_LTLIBRARIES += libmurphy-lua-utils.la
EXTRA_DIST      += core/lua-utils/murphy-lua-utils.pc
pkgconfig_DATA  += core/lua-utils/murphy-lua-utils.pc

libmurphy_lua_utils_ladir =					\
		$(includedir)/murphy/core/lua-utils

libmurphy_lua_utils_la_HEADERS =				\
		core/lua-utils/lua-utils.h			\
		core/lua-utils/strarray.h			\
		core/lua-utils/funcbridge.h			\
		core/lua-utils/object.h				\
		core/lua-utils/error.h				\
		core/lua-utils/include.h

libmurphy_lua_utils_la_REGULAR_SOURCES =			\
		core/lua-utils/lua-utils.c			\
		core/lua-utils/strarray.c			\
		core/lua-utils/funcbridge.c			\
		core/lua-utils/object.c				\
		core/lua-utils/error.c				\
		core/lua-utils/include.c

libmurphy_lua_utils_la_SOURCES =				\
		$(libmurphy_lua_utils_la_REGULAR_SOURCES)

libmurphy_lua_utils_la_CFLAGS =					\
		$(AM_CFLAGS)					\
		$(LUA_CFLAGS)

libmurphy_lua_utils_la_LDFLAGS =				\
		-Wl,-version-script=linker-script.lua-utils	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_lua_utils_la_LIBADD =	    				\
		libmurphy-common.la				\
		$(LUA_LIBS)

libmurphy_lua_utils_la_DEPENDENCIES =				\
		linker-script.lua-utils				\
		$(filter %.la, $(libmurphy_lua_utils_la_LIBADD))

# lua-utils linker script generation
linker-script.lua-utils: $(libmurphy_lua_utils_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_lua_utils_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.lua-utils

###################################
# murphy lua decision network
#

lib_LTLIBRARIES += libmurphy-lua-decision.la
EXTRA_DIST      += core/lua-decision/murphy-lua-decision.pc
pkgconfig_DATA  += core/lua-decision/murphy-lua-decision.pc

libmurphy_lua_decision_ladir =					\
		$(includedir)/murphy/core/lua-decision

libmurphy_lua_decision_la_HEADERS =				\
		core/lua-decision/mdb.h				\
		core/lua-decision/element.h

libmurphy_lua_decision_la_REGULAR_SOURCES =			\
		core/lua-decision/mdb.c				\
		core/lua-decision/element.c

libmurphy_lua_decision_la_SOURCES =				\
		$(libmurphy_lua_decision_la_REGULAR_SOURCES)

libmurphy_lua_decision_la_CFLAGS =				\
		$(AM_CFLAGS)					\
		$(LUA_CFLAGS)

libmurphy_lua_decision_la_LDFLAGS =				\
		-Wl,-version-script=linker-script.lua-decision	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_lua_decision_la_LIBADD =	\
		libmurphy-common.la	\
		libmurphy-lua-utils.la  \
		murphy-db/mql/libmql.la \
		murphy-db/mqi/libmqi.la \
		murphy-db/mdb/libmdb.la \
		$(LUA_LIBS)

libmurphy_lua_decision_la_DEPENDENCIES =	\
		linker-script.lua-decision	\
		$(filter %.la, $(libmurphy_lua_decision_la_LIBADD))

# lua-decision linker script generation
linker-script.lua-decision: $(libmurphy_lua_decision_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_lua_decision_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.lua-decision

###################################
# murphy core library
#

lib_LTLIBRARIES += libmurphy-core.la
EXTRA_DIST      += core/murphy-core.pc
pkgconfig_DATA  += core/murphy-core.pc

LUA_BINDINGS_SOURCES = 					\
		core/lua-bindings/lua-murphy.c		\
		core/lua-bindings/lua-lua.c		\
		core/lua-bindings/lua-plugin.c		\
		core/lua-bindings/lua-log.c		\
		core/lua-bindings/lua-console.c 	\
		core/lua-bindings/lua-bitwise.c   	\
		core/lua-bindings/lua-json.c		\
		core/lua-bindings/lua-timer.c   	\
		core/lua-bindings/lua-event.c   	\
		core/lua-bindings/lua-deferred.c	\
		core/lua-bindings/lua-sighandler.c	\
		core/lua-bindings/lua-transport.c	\
		core/lua-bindings/lua-env.c

libmurphy_core_lua_bindings_ladir =			\
		$(includedir)/murphy/core/lua-bindings

libmurphy_core_lua_bindings_la_HEADERS =		\
		core/lua-bindings/murphy.h 		\
		core/lua-bindings/lua-json.h

libmurphy_coreh_ladir      =		\
		$(includedir)/murphy

libmurphy_coreh_la_HEADERS =		\
		core.h

libmurphy_core_ladir =			\
		$(includedir)/murphy/core

libmurphy_core_la_HEADERS =		\
		core/context.h		\
		core/plugin.h		\
		core/console-command.h	\
		core/console.h		\
		core/scripting.h	\
		core/method.h		\
		core/auth.h		\
		core/domain.h		\
		core/domain-types.h

libmurphy_core_la_REGULAR_SOURCES =	\
		core/context.c		\
		core/plugin.c		\
		core/console.c		\
		core/scripting.c	\
		core/method.c		\
		core/auth.c		\
		core/auth-deny.c	\
		core/domain.c		\
		$(LUA_BINDINGS_SOURCES)

if SMACK_ENABLED
libmurphy_core_la_REGULAR_SOURCES +=	\
		core/auth-smack.c
endif

libmurphy_core_la_SOURCES =		\
		$(libmurphy_core_la_REGULAR_SOURCES)

libmurphy_core_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(LUA_CFLAGS)		\
		$(JSON_CFLAGS)		\
		$(SMACK_CFLAGS)

libmurphy_core_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.core \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_core_la_LIBADD  =			\
		libmurphy-common.la 		\
		libmurphy-lua-decision.la	\
		libmurphy-lua-utils.la		\
		murphy-db/mql/libmql.la 	\
		murphy-db/mqi/libmqi.la 	\
		murphy-db/mdb/libmdb.la		\
		$(LUA_LIBS) 			\
		$(SMACK_LIBS)			\
		-ldl

libmurphy_core_la_DEPENDENCIES =		\
		linker-script.core        	\
		$(filter %.la, $(libmurphy_core_la_LIBADD))

# core linker script generation
linker-script.core: $(libmurphy_core_la_HEADERS) \
		    $(libmurphy_core_lua_bindings_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_core_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.core

###################################
# murphy dbus library
#
# Right now we have three variants of the murphy D-Bus library. The
# original with leaky libdbus abstraction, a revised one that plugs
# the most gaping abstraction holes, and one which uses systemd-bus,
# the licensing-wise much easier/liberal D-Bus implementation from
# systemd.
#
# All original code uses/used to use the libdbus-based one with leaky
# abstraction. This would make it very difficult to move away from
# the license-troubled libdbus even if/when a viable alternative (eg.
# from the kdbus effort) became available. To get rid of this problem,
# we need to get rid of the direct dependency on libdbus. Once that is
# done, we can (hopefully) switch easily and transparently between
# D-Bus libraries, if needed by touching only one component, the
# murphy-dbus library.
#
# To recap these are the different library variants:
#
# 1) murphy-libdbus
#    This is the original murphy-dbus library. It uses libdbus as the
#    underlying D-Bus client library implementation. It does not provide
#    a proper abstraction on top of libdbus. Messages, message building,
#    and parsing used DBusMessage.
#
# 2) murphy-dbus-libdbus
#    This is a modified version of murphy-libdbus which provides an
#    additional abstraction for messages, message building and parsing.
#
# 3) murphy-dbus-systemdbus
#    This library provides an identical API to murphy-dbus-libdbus but
#    it uses the systemd dbus library implementation (systemd-bus) as
#    the underlying D-Bus client library implementation.
#
# So all current code still using libdbus for message building/parsing
# needs to get rid of it and changed to use murphy-dbus-libdbus.
#

# original libdbus-based library
murphy_libdbus_headers =		\
		common/libdbus.h

if LIBDBUS_ENABLED
lib_LTLIBRARIES += libmurphy-libdbus.la
EXTRA_DIST      += common/murphy-libdbus.pc
pkgconfig_DATA  += common/murphy-libdbus.pc

libmurphy_libdbus_ladir =		\
		$(includedir)/murphy/common

libmurphy_libdbus_la_HEADERS = $(murphy_libdbus_headers)

libmurphy_libdbus_la_REGULAR_SOURCES =		\
		common/libdbus.c		\
		common/libdbus-glue.c

libmurphy_libdbus_la_SOURCES =		\
		$(libmurphy_libdbus_la_REGULAR_SOURCES)

libmurphy_libdbus_la_CFLAGS = 		\
		$(AM_CFLAGS)		\
		$(LIBDBUS_CFLAGS)

libmurphy_libdbus_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.libdbus \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_libdbus_la_LIBADD  = 		\
		libmurphy-common.la	\
		$(LIBDBUS_LIBS)		\
		-lrt

libmurphy_libdbus_la_DEPENDENCIES =	\
		linker-script.libdbus	\
		$(filter %.la, $(libmurphy_libdbus_la_LIBADD))

libmurphy_libdbusdir      = $(includedir)/murphy/dbus
libmurphy_libdbus_HEADERS = $(libmurphy_libdbus_la_HEADERS)
endif

# linker script generation
linker-script.libdbus: $(murphy_libdbus_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_libdbus_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.libdbus

# libdbus-based library with full abstraction
murphy_dbus_libdbus_headers =		\
		common/dbus-libdbus.h	\
		common/dbus-error.h

if LIBDBUS_ENABLED
lib_LTLIBRARIES += libmurphy-dbus-libdbus.la
EXTRA_DIST      += common/murphy-dbus-libdbus.pc
pkgconfig_DATA  += common/murphy-dbus-libdbus.pc

libmurphy_dbus_libdbus_ladir =		\
		$(includedir)/murphy/common

libmurphy_dbus_libdbus_la_HEADERS = $(murphy_dbus_libdbus_headers)

libmurphy_dbus_libdbus_la_REGULAR_SOURCES =	\
		common/dbus-libdbus.c		\
		common/dbus-libdbus-glue.c	\
		common/dbus-libdbus-transport.c

libmurphy_dbus_libdbus_la_SOURCES =		\
		$(libmurphy_dbus_libdbus_la_REGULAR_SOURCES)

libmurphy_dbus_libdbus_la_CFLAGS = 	\
		$(AM_CFLAGS)		\
		$(LIBDBUS_CFLAGS)

libmurphy_dbus_libdbus_la_LDFLAGS =	\
		-Wl,-version-script=linker-script.dbus-libdbus \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_dbus_libdbus_la_LIBADD  = 	\
		libmurphy-common.la	\
		-lrt $(LIBDBUS_LIBS)

libmurphy_dbus_libdbus_la_DEPENDENCIES =	\
		linker-script.dbus-libdbus	\
		$(filter %.la, $(libmurphy_dbus_libdbus_la_LIBADD))

libmurphy_dbus_libdbusdir      = $(includedir)/murphy/dbus
libmurphy_dbus_libdbus_HEADERS = $(libmurphy_dbus_libdbus_la_HEADERS)
endif

# linker script generation
linker-script.dbus-libdbus: $(murphy_dbus_libdbus_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_dbus_libdbus_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.dbus-libdbus

# systemd-bus based library
murphy_dbus_sdbus_headers = 			\
		common/dbus-sdbus.h		\
		common/dbus-error.h

if SDBUS_ENABLED
lib_LTLIBRARIES += libmurphy-dbus-sdbus.la
EXTRA_DIST      += common/murphy-dbus-sdbus.pc
pkgconfig_DATA  += common/murphy-dbus-sdbus.pc

libmurphy_dbus_sdbus_ladir      =		\
		$(includedir)/murphy/common

libmurphy_dbus_sdbus_la_HEADERS = $(murphy_dbus_sdbus_headers)

libmurphy_dbus_sdbus_la_REGULAR_SOURCES =	\
		common/dbus-sdbus.c		\
		common/dbus-sdbus-glue.c	\
		common/dbus-sdbus-transport.c

libmurphy_dbus_sdbus_la_SOURCES =		\
		$(libmurphy_dbus_sdbus_la_REGULAR_SOURCES)

libmurphy_dbus_sdbus_la_CFLAGS  =	\
		$(AM_CFLAGS)		\
		$(SDBUS_CFLAGS)

libmurphy_dbus_sdbus_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.dbus-sdbus \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_dbus_sdbus_la_LIBADD  = 		\
		-lrt $(SDBUS_LIBS)

libmurphy_dbus_sdbus_la_DEPENDENCIES =		\
		linker-script.dbus-sdbus	\
		$(filter %.la, $(libmurphy_dbus_sdbus_la_LIBADD))

libmurphy_dbus_sdbusdir      = $(includedir)/murphy/dbus
libmurphy_dbus_sdbus_HEADERS = $(libmurphy_dbus_sdbus_la_HEADERS)
endif

# linker script generation
linker-script.dbus-sdbus: $(murphy_dbus_sdbus_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_dbus_sdbus_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.sd-bus

###################################
# murphy pulse glue library
#

murphy_pulse_headers =			\
		common/pulse-glue.h	\
		common/pulse-subloop.h

if PULSE_ENABLED
lib_LTLIBRARIES += libmurphy-pulse.la
EXTRA_DIST      += common/murphy-pulse.pc
pkgconfig_DATA  += common/murphy-pulse.pc

libmurphy_pulse_ladir      =		\
		$(includedir)/murphy/common

libmurphy_pulse_la_HEADERS = $(murphy_pulse_headers)

libmurphy_pulse_la_SOURCES =		\
		common/pulse-glue.c	\
		common/pulse-subloop.c

libmurphy_pulse_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(PULSE_CFLAGS)

libmurphy_pulse_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.pulse \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_pulse_la_LIBADD  =		\
		libmurphy-common.la	\
		$(PULSE_LIBS)

libmurphy_pulse_la_DEPENDENCIES =	\
		linker-script.pulse	\
		$(filter %.la, $(libmurphy_pulse_la_LIBADD))

libpulseincludedir      = $(includedir)/murphy/pulse
libpulseinclude_HEADERS = $(libmurphy_pulse_la_HEADERS)
endif

# linker script generation
linker-script.pulse: $(murphy_pulse_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_pulse_la_CFLAGS)" \
                -p '^mrp_|^_mrp|^pa_' -o $@ $^

clean-linker-script::
	-rm -f linker-script.pulse

###################################
# murphy ecore glue library
#

murphy_ecore_headers =			\
		common/ecore-glue.h

if ECORE_ENABLED
lib_LTLIBRARIES += libmurphy-ecore.la
EXTRA_DIST      += common/murphy-ecore.pc
pkgconfig_DATA  += common/murphy-ecore.pc

libmurphy_ecore_ladir      =		\
		$(includedir)/murphy/common

libmurphy_ecore_la_HEADERS = $(murphy_ecore_headers)

libmurphy_ecore_la_SOURCES =		\
		common/ecore-glue.c

libmurphy_ecore_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(ECORE_CFLAGS)

libmurphy_ecore_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.ecore \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_ecore_la_LIBADD  =		\
		libmurphy-common.la	\
		$(ECORE_LIBS)

libmurphy_ecore_la_DEPENDENCIES =	\
		linker-script.ecore	\
		$(filter %.la, $(libmurphy_ecore_la_LIBADD))

libecoreincludedir      = $(includedir)/murphy/ecore
libecoreinclude_HEADERS = $(libmurphy_ecore_la_HEADERS)
endif

# linker script generation
linker-script.ecore: $(murphy_ecore_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_ecore_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.ecore

###################################
# murphy glib glue library
#

murphy_glib_headers =			\
		common/glib-glue.h

if GLIB_ENABLED
lib_LTLIBRARIES += libmurphy-glib.la
EXTRA_DIST      += common/murphy-glib.pc
pkgconfig_DATA  += common/murphy-glib.pc

libmurphy_glib_ladir      =		\
		$(includedir)/murphy/common

libmurphy_glib_la_HEADERS = $(murphy_glib_headers)

libmurphy_glib_la_SOURCES =		\
		common/glib-glue.c

libmurphy_glib_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(GLIB_CFLAGS)

libmurphy_glib_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.glib \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_glib_la_LIBADD  =		\
		libmurphy-common.la	\
		$(GLIB_LIBS)

libmurphy_glib_la_DEPENDENCIES =	\
		linker-script.glib	\
		$(filter %.la, $(libmurphy_glib_la_LIBADD))

libglibincludedir      = $(includedir)/murphy/glib
libglibinclude_HEADERS = $(libmurphy_glib_la_HEADERS)
endif

# linker script generation
linker-script.glib: $(murphy_glib_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_glib_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.glib

###################################
# murphy qt glue library
#

murphy_qt_headers	=		\
		common/qt-glue.h

if QT_ENABLED
lib_LTLIBRARIES += libmurphy-qt.la
EXTRA_DIST      += common/murphy-qt.pc
pkgconfig_DATA  += common/murphy-qt.pc

BUILT_SOURCES   += common/qt-glue-priv.moc.h

libmurphy_qt_ladir      =		\
		$(includedir)/murphy/common

libmurphy_qt_la_HEADERS = $(murphy_qt_headers)

libmurphy_qt_la_SOURCES =		\
		common/qt-glue.cpp

libmurphy_qt_la_CPPFLAGS  =		\
		$(AM_CFLAGS)		\
		$(QTCORE_CFLAGS)

libmurphy_qt_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.qt \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_qt_la_LIBADD  = $(QTCORE_LIBS)

libmurphy_qt_la_DEPENDENCIES =		\
		linker-script.qt	\
		$(filter %.la, $(libmurphy_qt_la_LIBADD))

libqtincludedir      = $(includedir)/murphy/qt
libqtinclude_HEADERS = $(libmurphy_qt_la_HEADERS)

# run moc to generate Qt meta objects
common/qt-glue-priv.moc.h: common/qt-glue-priv.h
	$(QUIET_GEN)$(QT_MOC) -DQT_ENABLED=1 $< -o$@
endif

# linker script generation
linker-script.qt: $(murphy_qt_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_qt_la_CPPFLAGS) -DQT_ENABLED=1" \
		-o $@ $^

clean-linker-script::
	-rm -f linker-script.qt

###################################
# murphy resolver library
#

lib_LTLIBRARIES += libmurphy-resolver.la
EXTRA_DIST      += resolver/murphy-resolver.pc
pkgconfig_DATA  += resolver/murphy-resolver.pc

BUILT_SOURCES   += resolver/scanner.c resolver/parser.c

libmurphy_resolver_ladir =			\
		$(includedir)/murphy/resolver

libmurphy_resolver_la_HEADERS =			\
		resolver/resolver.h

SIMPLE_SCRIPT_SOURCES = 					\
		resolver/scripting/simple/simple-script.c	\
		resolver/scripting/simple/simple-scanner.c	\
		resolver/scripting/simple/simple-parser.c	\
		resolver/scripting/simple/call.c                \
		resolver/scripting/simple/builtins.c

libmurphy_resolver_la_REGULAR_SOURCES =				\
		resolver/resolver.c				\
		resolver/parser.c				\
		resolver/scanner.c      			\
		resolver/target.c				\
		resolver/target-sorter.c			\
		resolver/fact.c					\
		resolver/events.c				\
		resolver/console.c				\
		$(SIMPLE_SCRIPT_SOURCES)

libmurphy_resolver_la_SOURCES =			\
		$(libmurphy_resolver_la_REGULAR_SOURCES)

libmurphy_resolver_la_CFLAGS =			\
		$(AM_CFLAGS)			\
		$(JSON_CFLAGS)

libmurphy_resolver_la_LDFLAGS =			\
		-Wl,-version-script=linker-script.resolver \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_resolver_la_LIBADD =	    	\
		libmurphy-core.la   	\
		libmurphy-common.la 	\
		murphy-db/mql/libmql.la \
		murphy-db/mqi/libmqi.la \
		murphy-db/mdb/libmdb.la

libmurphy_resolver_la_DEPENDENCIES =	\
		linker-script.resolver  \
		$(filter %.la, $(libmurphy_resolver_la_LIBADD))

# resolver lexical analyser and parser generation
RESOLVER_PARSER_PREFIX = yy_res_

resolver/scanner.c: resolver/scanner.l resolver/parser.c
	$(LEXCOMPILE) -P $(RESOLVER_PARSER_PREFIX) $<
	mv lex.$(RESOLVER_PARSER_PREFIX).c $@

resolver/parser.h resolver/parser.c: resolver/parser.y
	$(YACCCOMPILE) -p $(RESOLVER_PARSER_PREFIX) $< -o resolver/parser.c

# resolver linker script generation
linker-script.resolver: $(libmurphy_resolver_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_resolver_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.resolver

# simple interpreter lexical analyser and parser generation
SIMPLE_PARSER_PREFIX  = yy_smpl_
BUILT_SOURCES        += resolver/scripting/simple/simple-scanner.c \
                        resolver/scripting/simple/simple-parser.c

resolver/scripting/simple/simple-scanner.c:		\
    resolver/scripting/simple/simple-scanner.l		\
    resolver/scripting/simple/simple-parser.c
	$(LEXCOMPILE) -P $(SIMPLE_PARSER_PREFIX) $<
	mv lex.$(SIMPLE_PARSER_PREFIX).c $@

# simple interpreter parser generation
resolver/scripting/simple/simple-parser.h 	\
resolver/scripting/simple/simple-parser.c:	\
    resolver/scripting/simple/simple-parser.y
	$(YACCCOMPILE) -p $(SIMPLE_PARSER_PREFIX) $< -o \
	    resolver/scripting/simple/simple-parser.c

###################################
# murphy backend resource library
#

if BUILD_RESOURCES
RESOURCE_LIBRARY = libmurphy-resource-backend.la
else
RESOURCE_LIBRARY =
endif

if BUILD_RESOURCES
lib_LTLIBRARIES += $(RESOURCE_LIBRARY)

libmurphy_resource_backend_ladir =                     \
		$(includedir)/murphy/resource

libmurphy_resource_backend_la_HEADERS =                        \
		resource/data-types.h           \
		resource/common-api.h           \
		resource/client-api.h           \
		resource/manager-api.h          \
		resource/config-api.h           \
		resource/protocol.h

libmurphy_resource_backend_la_REGULAR_SOURCES =                \
		resource/attribute.c		\
		resource/resource.c		\
		resource/resource-set.c		\
		resource/application-class.c	\
		resource/resource-owner.c	\
		resource/resource-client.c	\
		resource/zone.c			\
		resource/config-lua.c		\
		resource/resource-lua.c 	\
		resource/lua-resource.c

libmurphy_resource_backend_la_SOURCES =                        \
		$(libmurphy_resource_backend_la_REGULAR_SOURCES)

libmurphy_resource_backend_la_CFLAGS =		\
		$(AM_CFLAGS)			\
		$(LUA_CFLAGS)

libmurphy_resource_backend_la_LDFLAGS =                        \
		-Wl,-version-script=linker-script.resource_backend \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_resource_backend_la_LIBADD =     \
		libmurphy-core.la          \
		libmurphy-common.la        \
		libmurphy-lua-utils.la     \
		murphy-db/mql/libmql.la    \
		murphy-db/mqi/libmqi.la    \
		murphy-db/mdb/libmdb.la    \
		$(LUA_LIBS)

libmurphy_resource_backend_la_DEPENDENCIES =	\
		linker-script.resource_backend	\
		$(filter %.la, $(libmurphy_resource_backend_la_LIBADD))
endif

# resource linker script generation
linker-script.resource_backend: $(libmurphy_resource_backend_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_resource_backend_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.resource_backend

##########################
# readline replacement
#
lib_LTLIBRARIES += libbreedline.la
EXTRA_DIST      += breedline/breedline.pc
pkgconfig_DATA  += breedline/breedline.pc

libbreedline_ladir      =		\
		$(includedir)/breedline

libbreedline_la_HEADERS =		\
		breedline/breedline.h	\
		breedline/macros.h

libbreedline_la_SOURCES =		\
		breedline/breedline.c

libbreedline_la_CFLAGS  =		\
		$(AM_CFLAGS)

libbreedline_la_LDFLAGS =					\
		-Wl,-version-script=linker-script.breedline	\
		-version-info @MURPHY_VERSION_INFO@

libbreedline_la_LIBADD =

libbreedline_la_DEPENDENCIES =			\
		linker-script.breedline		\
		$(filter %.la, $(libbreedline_la_LIBADD))

libbreedlineincludedir      = $(includedir)/breedline
libbreedlineinclude_HEADERS = $(libbreedline_la_HEADERS)

# linker script generation
linker-script.breedline: $(libbreedline_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libbreedline_la_CFLAGS)" -p '^brl_' -o $@ $^

clean-linker-script::
	-rm -f linker-script.breedline

##########################
# breedline murphy glue library
#
lib_LTLIBRARIES += libbreedline-murphy.la
EXTRA_DIST      += breedline/breedline-murphy.pc
pkgconfig_DATA  += breedline/breedline-murphy.pc

libbreedline_murphy_ladir      = $(includedir)/breedline

libbreedline_murphy_la_HEADERS = breedline/breedline-murphy.h
libbreedline_murphy_la_SOURCES = breedline/breedline-murphy.c
libbreedline_murphy_la_CFLAGS  = $(AM_CFLAGS)

libbreedline_murphy_la_LDFLAGS =	\
		-Wl,-version-script=linker-script.breedline-murphy	\
		-version-info @MURPHY_VERSION_INFO@

libbreedline_murphy_la_LIBADD = 	\
		libmurphy-common.la 	\
		libbreedline.la

libbreedline_murphy_la_DEPENDENCIES =		\
		linker-script.breedline-murphy	\
		$(filter %.la,$(libbreedline_murphy_la_LIBADD))

libbreedline_murphyincludedir      = $(includedir)/breedline
libbreedline_murphyinclude_HEADERS = $(libbreedline_murphy_la_HEADERS)

# linker script generation
linker-script.breedline-murphy: $(libbreedline_murphy_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
	    -P "$(CC)" -c "$(libbreedline_murphy_la_CFLAGS)" -p '^brl_' -o $@ $^

clean-linker-script::
	-rm -f linker-script.breedline-murphy

##########################
# breedline glib glue library
#

breedline_glib_headers =	\
		breedline/breedline-glib.h

if GLIB_ENABLED
lib_LTLIBRARIES += libbreedline-glib.la
EXTRA_DIST      += breedline/breedline-glib.pc
pkgconfig_DATA  += breedline/breedline-glib.pc

libbreedline_glib_ladir      = $(includedir)/breedline

libbreedline_glib_la_HEADERS = $(breedline_glib_headers)
libbreedline_glib_la_SOURCES = breedline/breedline-glib.c
libbreedline_glib_la_CFLAGS  = $(AM_CFLAGS) $(GLIB_CFLAGS)

libbreedline_glib_la_LDFLAGS = \
		-Wl,-version-script=linker-script.breedline-glib	\
		-version-info @MURPHY_VERSION_INFO@

libbreedline_glib_la_LIBADD =		\
		libbreedline.la 	\
		$(GLIB_LIBS)

libbreedline_glib_la_DEPENDENCIES =		\
		linker-script.breedline-glib	\
		$(filter %.la, $(libbreedline_glib_la_LIBADD))

libbreedline_glibincludedir      = $(includedir)/breedline
libbreedline_glibinclude_HEADERS = $(libbreedline_glib_la_HEADERS)
endif

# linker script generation
linker-script.breedline-glib: $(breedline_glib_headers)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libbreedline_glib_la_CFLAGS)" -p '^brl_' \
		-o $@ $^

clean-linker-script::
	-rm -f linker-script.breedline-glib

###################################
# murphy resource external library

# TODO: put this under conditional compilation

lib_LTLIBRARIES += libmurphy-resource.la
EXTRA_DIST      += plugins/resource-native/libmurphy-resource/murphy-resource.pc
pkgconfig_DATA  += plugins/resource-native/libmurphy-resource/murphy-resource.pc

libmurphy_resource_ladir =          \
        $(includedir)/murphy/plugins/resource-native/libmurphy-resource

libmurphy_resource_la_HEADERS =         \
        plugins/resource-native/libmurphy-resource/resource-api.h     \
        plugins/resource-native/libmurphy-resource/resource-private.h

libmurphy_resource_la_SOURCES =         \
        plugins/resource-native/libmurphy-resource/resource.c \
        plugins/resource-native/libmurphy-resource/attribute.c \
        plugins/resource-native/libmurphy-resource/rset.c \
        plugins/resource-native/libmurphy-resource/string_array.c \
        plugins/resource-native/libmurphy-resource/message.c \
	plugins/resource-native/libmurphy-resource/resource-log.c

libmurphy_resource_la_CFLAGS =          \
        $(AM_CFLAGS)

libmurphy_resource_la_LDFLAGS =	\
		-Wl,-version-script=linker-script.resource	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_resource_la_LIBADD =      \
		libmurphy-common.la

libmurphy_resource_la_DEPENDENCIES =	\
		libmurphy-common.la	\
		linker-script.resource	\
		$(filter %.la, $(libmurphy_resource_la_LIBADD))

# linker script generation
linker-script.resource: $(libmurphy_resource_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_resource_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.resource

# resource-api-test
bin_PROGRAMS += resource-api-test

resource_api_test_SOURCES = plugins/resource-native/libmurphy-resource/api_test.c
resource_api_test_CFLAGS  = $(AM_CFLAGS)
resource_api_test_LDADD   = libmurphy-common.la libmurphy-resource.la

# resource-api-fuzz
bin_PROGRAMS += resource-api-fuzz

resource_api_fuzz_SOURCES = plugins/resource-native/libmurphy-resource/resource-fuzz.c
resource_api_fuzz_CFLAGS  = $(AM_CFLAGS)
resource_api_fuzz_LDADD   = libmurphy-common.la libmurphy-resource.la

# context-create
bin_PROGRAMS += resource-context-create

resource_context_create_SOURCES = plugins/resource-native/libmurphy-resource/context-create.c
resource_context_create_CFLAGS  = $(AM_CFLAGS)
resource_context_create_LDADD   = libmurphy-common.la libmurphy-resource.la

###################################
# murphy plugins
#

BUILTIN_PLUGINS    =
BUILTIN_CFLAGS     = -D__MURPHY_BUILTIN_PLUGIN__ $(AM_CFLAGS)
BUILTIN_LIBS       =

LINKEDIN_PLUGINS   =

plugin_LTLIBRARIES =
plugindir          = $(libdir)/murphy/plugins


# test plugin
TEST_PLUGIN_SOURCES = plugins/plugin-test.c
TEST_PLUGIN_CFLAGS  =
TEST_PLUGIN_LIBS    =

if !DISABLED_PLUGIN_TEST
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
endif

# dbus plugin
if LIBDBUS_ENABLED
DBUS_PLUGIN_SOURCES = plugins/plugin-dbus.c
DBUS_PLUGIN_CFLAGS  = $(LIBDBUS_CFLAGS)
DBUS_PLUGIN_LIBS    = libmurphy-dbus-libdbus.la $(LIBDBUS_LIBS)

if !DISABLED_PLUGIN_DBUS
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
endif
endif

if GLIB_ENABLED
# glib plugin
GLIB_PLUGIN_SOURCES = plugins/plugin-glib.c
GLIB_PLUGIN_CFLAGS  = $(GLIB_CFLAGS)
GLIB_PLUGIN_LIBS    = $(GLIB_LIBS)

if !DISABLED_PLUGIN_GLIB
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
endif
endif

# resource-dbus plugin
if BUILD_RESOURCES
if LIBDBUS_ENABLED
RESOURCE_DBUS_PLUGIN_SOURCES = plugins/plugin-resource-dbus.c
RESOURCE_DBUS_PLUGIN_CFLAGS  = $(LIBDBUS_CFLAGS)
RESOURCE_DBUS_PLUGIN_LIBS    =					\
				libmurphy-dbus-libdbus.la	\
				libmurphy-core.la		\
				libmurphy-common.la		\
				$(RESOURCE_LIBRARY)

if !DISABLED_PLUGIN_RESOURCE_DBUS
if BUILTIN_PLUGIN_RESOURCE_DBUS
BUILTIN_PLUGINS += $(RESOURCE_DBUS_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(RESOURCE_DBUS_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(RESOURCE_DBUS_PLUGIN_LIBS)
else
plugin_resource_dbus_la_SOURCES = $(RESOURCE_DBUS_PLUGIN_SOURCES)
plugin_resource_dbus_la_CFLAGS  = $(RESOURCE_DBUS_PLUGIN_CFLAGS) $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_resource_dbus_la_LDFLAGS = -module -avoid-version
plugin_resource_dbus_la_LIBADD  = $(RESOURCE_DBUS_PLUGIN_LIBS)
plugin_LTLIBRARIES    += plugin-resource-dbus.la
endif
endif
endif
endif

# resource-wrt plugin
if BUILD_RESOURCES
if WEBSOCKETS_ENABLED
RESOURCE_WRT_PLUGIN_SOURCES = plugins/resource-wrt/plugin-resource-wrt.c
RESOURCE_WRT_PLUGIN_CFLAGS  = $(WEBSOCKETS_CFLAGS)
RESOURCE_WRT_PLUGIN_LIBS    =

if !DISABLED_PLUGIN_RESOURCE_WRT
if BUILTIN_PLUGIN_RESOURCE_WRT
BUILTIN_PLUGINS += $(RESOURCE_WRT_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(RESOURCE_WRT_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(RESOURCE_WRT_PLUGIN_LIBS)
else
plugin_resource_wrt_la_SOURCES = $(RESOURCE_WRT_PLUGIN_SOURCES)
plugin_resource_wrt_la_CFLAGS  = $(RESOURCE_WRT_PLUGIN_CFLAGS) \
				 $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_resource_wrt_la_LDFLAGS = -module -avoid-version
plugin_resource_wrt_la_LIBADD  = $(RESOURCE_WRT_PLUGIN_LIBS)
plugin_LTLIBRARIES    += plugin-resource-wrt.la

EXTRA_DIST       += $(resource_wrt_DATA);
resource_wrtdir   = $(datadir)/murphy/resource-wrt
resource_wrt_DATA = 					\
		plugins/resource-wrt/resource-api.js	\
		plugins/resource-wrt/resource-test.html

endif
endif
endif
endif

# console plugin
CONSOLE_PLUGIN_REGULAR_SOURCES = plugins/console/plugin-console.c
CONSOLE_PLUGIN_SOURCES         = $(CONSOLE_PLUGIN_REGULAR_SOURCES)
CONSOLE_PLUGIN_CFLAGS  	       =
CONSOLE_PLUGIN_LIBS    	       =

if CONSOLE_ENABLED
if !DISABLED_PLUGIN_CONSOLE
if BUILTIN_PLUGIN_CONSOLE
BUILTIN_PLUGINS += $(CONSOLE_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(CONSOLE_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(CONSOLE_PLUGIN_LIBS)
else
plugin_console_la_SOURCES = $(CONSOLE_PLUGIN_SOURCES)
plugin_console_la_CFLAGS  = $(CONSOLE_PLUGIN_CFLAGS) \
			    $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_console_la_LDFLAGS = -module -avoid-version
plugin_console_la_LIBADD  = $(CONSOLE_PLUGIN_LIBS)

plugin_LTLIBRARIES    += plugin-console.la
endif
endif
endif

# native resource plugin
if BUILD_RESOURCES
PLUGIN_RESOURCE_NATIVE_REGULAR_SOURCES = 				\
		plugins/resource-native/plugin-resource-native.c
PLUGIN_RESOURCE_NATIVE_SOURCES = 					\
		$(PLUGIN_RESOURCE_NATIVE_REGULAR_SOURCES)
PLUGIN_RESOURCE_NATIVE_CFLAGS =						\
		$(LUA_CFLAGS)

PLUGIN_RESOURCE_NATIVE_LIBS =       					\
		libmurphy-common.la					\
		$(RESOURCE_LIBRARY)                   			\
		$(LUA_LIBS)

plugin_resource_native_la_SOURCES = $(PLUGIN_RESOURCE_NATIVE_SOURCES)
plugin_resource_native_la_CFLAGS  = $(PLUGIN_RESOURCE_NATIVE_CFLAGS)	\
				    $(MURPHY_CFLAGS) $(AM_CFLAGS)	\
				    $(JSON_CFLAGS)
plugin_resource_native_la_LDFLAGS = -module -avoid-version
plugin_resource_native_la_LIBADD  = $(PLUGIN_RESOURCE_NATIVE_LIBS)

plugin_LTLIBRARIES += plugin-resource-native.la

# resource-client
bin_PROGRAMS += resource-client

resource_client_SOURCES = plugins/resource-native/resource-client.c
resource_client_CFLAGS  = $(AM_CFLAGS)
resource_client_LDADD   =  libmurphy-common.la
endif

# domain control plugin
DOMAIN_CONTROL_PLUGIN_SOURCES = plugins/domain-control/plugin-domain-control.c \
				plugins/domain-control/domain-control.c        \
				plugins/domain-control/proxy.c		       \
				plugins/domain-control/table.c                 \
				plugins/domain-control/notify.c 	       \
				plugins/domain-control/message.c
DOMAIN_CONTROL_PLUGIN_LOADER  = linkedin-domain-control-loader.c


DOMAIN_CONTROL_PLUGIN_CFLAGS  =
DOMAIN_CONTROL_PLUGIN_LIBS    =

if !DISABLED_PLUGIN_DOMAIN_CONTROL
if BUILTIN_PLUGIN_DOMAIN_CONTROL
LINKEDIN_PLUGINS             += libmurphy-plugin-domain-control.la
lib_LTLIBRARIES              += libmurphy-plugin-domain-control.la
DOMAIN_CONTROL_PLUGIN_CFLAGS += $(BUILTIN_CFLAGS)


libmurphy_plugin_domain_control_ladir =				 \
		$(includedir)/murphy/domain-control

libmurphy_plugin_domain_control_la_SOURCES =			 \
		$(DOMAIN_CONTROL_PLUGIN_SOURCES)		 \
		$(DOMAIN_CONTROL_PLUGIN_LOADER)

libmurphy_plugin_domain_control_la_CFLAGS =			 \
		$(DOMAIN_CONTROL_PLUGIN_CFLAGS)			 \
		$(AM_CFLAGS)					 \
		$(JSON_CFLAGS)

libmurphy_plugin_domain_control_la_LDFLAGS =			 \
		-Wl,-version-script=linker-script.domain-control \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_plugin_domain_control_la_LIBADD =		\
		libmurphy-core.la   			\
		libmurphy-common.la 			\
		murphy-db/mql/libmql.la			\
		murphy-db/mqi/libmqi.la			\
		murphy-db/mdb/libmdb.la

libmurphy_plugin_domain_control_la_DEPENDENCIES =	\
		linker-script.domain-control		\
	$(filter %.la, $(libmurphy_plugin_domain_control_la_LIBADD))
else
plugin_domain_control_la_SOURCES = $(DOMAIN_CONTROL_PLUGIN_SOURCES)
plugin_domain_control_la_CFLAGS  = $(DOMAIN_CONTROL_PLUGIN_CFLAGS) \
				   $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_domain_control_la_LDFLAGS = -module -avoid-version
plugin_domain_control_la_LIBADD  = $(DOMAIN_CONTROL_PLUGIN_LIBS)
plugin_LTLIBRARIES              += plugin-domain-control.la
endif


# domain controller client library
lib_LTLIBRARIES += libmurphy-domain-controller.la
EXTRA_DIST      += plugins/domain-control/murphy-domain-controller.pc
pkgconfig_DATA  += plugins/domain-control/murphy-domain-controller.pc

libmurphy_domain_controller_ladir =			\
		$(includedir)/murphy/domain-control

libmurphy_domain_controller_la_HEADERS =		\
		plugins/domain-control/client.h

libmurphy_domain_controller_la_SOURCES = 		\
		plugins/domain-control/client.c		\
		plugins/domain-control/message.c
libmurphy_domain_controller_la_CFLAGS   =		\
		$(JSON_CFLAGS)
libmurphy_domain_controller_la_LIBADD   = 	\
		libmurphy-common.la		\
		murphy-db/mql/libmql.la		\
		murphy-db/mqi/libmqi.la		\
		murphy-db/mdb/libmdb.la		\
		$(JSON_LIBS)

if WEBSOCKETS_ENABLED
EXTRA_DIST          += $(domain_control_DATA)
domain_controldir    = $(datadir)/murphy/domain-control
domain_control_DATA  = 						\
		plugins/domain-control/domain-control-api.js	\
		plugins/domain-control/domain-control-test.html
endif

# test domain controller (disabled until we have a readline replacement)
bin_PROGRAMS += test-domain-controller

test_domain_controller_SOURCES = plugins/domain-control/test-client.c
test_domain_controller_CFLAGS  = $(AM_CFLAGS)
test_domain_controller_LDADD   = libmurphy-domain-controller.la \
				 libbreedline-murphy.la		\
				 libbreedline.la		\
				 libmurphy-common.la
endif

# linkedin domain control plugin linker script generation
linker-script.domain-control: $(DOMAIN_CONTROL_PLUGIN_LOADER:%.c=%.h)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q \
		-P "$(CC)" -c "$(libmurphy_domain_controller_la_CFLAGS)" -o $@ $^

clean-linker-script::
	-rm -f linker-script.domain-control

# Lua plugin
LUA_PLUGIN_SOURCES = plugins/plugin-lua.c
LUA_PLUGIN_CFLAGS  = $(AM_CFLAGS) $(LUA_CFLAGS)
LUA_PLUGIN_LIBS    = $(LUA_LIBS)


if BUILTIN_PLUGIN_LUA
BUILTIN_PLUGINS += $(LUA_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(LUA_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(LUA_PLUGIN_LIBS)
else
plugin_lua_la_SOURCES = plugins/plugin-lua.c
plugin_lua_la_CFLAGS  = $(LUA_PLUGIN_CFLAGS) \
			$(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_lua_la_LDFLAGS = -module -avoid-version
plugin_lua_la_LIBADD  = $(LUA_PLUGIN_LIBS) $(LUA_LIBS)

plugin_LTLIBRARIES    += plugin-lua.la
endif

# systemd (logging) plugin
if SYSTEMD_ENABLED
SYSTEMD_PLUGIN_SOURCES = plugins/plugin-systemd.c
SYSTEMD_PLUGIN_CFLAGS  = $(SYSTEMD_CFLAGS)
SYSTEMD_PLUGIN_LIBS    = $(SYSTEMD_LIBS)

if !DISABLED_PLUGIN_SYSTEMD
if BUILTIN_PLUGIN_SYSTEMD
BUILTIN_PLUGINS += $(SYSTEMD_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(SYSTEMD_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(SYSTEMD_PLUGIN_LIBS)
else
plugin_systemd_la_SOURCES = $(SYSTEMD_PLUGIN_SOURCES)
plugin_systemd_la_CFLAGS  = $(SYSTEMD_PLUGIN_CFLAGS) \
                            $(MURPHY_CFLAGS) \
                            $(AM_CFLAGS)
plugin_systemd_la_LDFLAGS = -module -avoid-version
plugin_systemd_la_LIBADD  = $(SYSTEMD_PLUGIN_LIBS)

plugin_LTLIBRARIES += plugin-systemd.la
endif
endif
endif


###################################
# murphy daemon
#

bin_PROGRAMS += murphyd
EXTRA_DIST   += $(config_DATA)
configdir     = $(sysconfdir)/murphy
config_DATA   = $(shell ls daemon/sample-config/*.{cfg,rules})

murphyd_SOURCES =			\
		daemon/daemon.c		\
		daemon/config.c		\
		$(BUILTIN_PLUGINS)	\
		load-linkedin-plugins.c

murphyd_CFLAGS  =			\
		$(AM_CFLAGS)		\
		$(BUILTIN_CFLAGS)	\
		$(JSON_CFLAGS)

murphyd_LDADD  =				\
		$(BUILTIN_LIBS)         	\
		$(LINKEDIN_PLUGINS)		\
		$(RESOURCE_LIBRARY)		\
		libmurphy-resolver.la		\
		libmurphy-lua-decision.la	\
		libmurphy-core.la		\
		libmurphy-lua-utils.la		\
		libmurphy-common.la		\
		$(JSON_LIBS)

if LIBDBUS_ENABLED
murphyd_LDADD +=				\
		libmurphy-dbus-libdbus.la	\
		-lrt $(LIBDBUS_LIBS)
endif


murphyd_LDFLAGS = -rdynamic

install-data-hook::
	rm -f $(DESTDIR)/$(sysconfdir)/murphy/murphy.cfg
	echo "load-plugin lua config=\"$(sysconfdir)/murphy/main.cfg\"" \
	    > $(DESTDIR)/$(sysconfdir)/murphy/murphy.conf

###################################
# linkedin (DSO) loader generation
#

linkedin-%-loader.c:
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linkedin-loader \
	    -p $(shell echo $@ | \
	        sed 's/linkedin-//g;s/-loader.c//g') -o $@

linkedin-%-loader.h:
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linkedin-loader \
	    -p $(shell echo $@ | \
	        sed 's/linkedin-//g;s/-loader.h//g') -o $@

load-linkedin-plugins.c:
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linkedin-loader \
	    -o $@ -d $(shell echo $(LINKEDIN_PLUGINS) | \
	                   sed 's/.*plugin-//g;s/\.[^\.]*$$//g')

clean-local::
	-rm -f linkedin-*-loader.[hc] load-linkedin-plugins.c

###################################
# murphy console client
#

if CONSOLE_ENABLED
bin_PROGRAMS += murphy-console

murphy_console_SOURCES =		\
		console-client/client.c

murphy_console_CFLAGS  =		\
		$(AM_CFLAGS)

murphy_console_LDADD  =			\
		libbreedline-murphy.la	\
		libbreedline.la		\
		libmurphy-common.la

if LIBDBUS_ENABLED
murphy_console_LDADD +=	libmurphy-dbus-libdbus.la
endif

murphy_console_LDFLAGS = -rdynamic
endif

# cleanup
clean-local:: # clean-linker-script
	-rm -f *~
