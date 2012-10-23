SUBDIRS         = murphy-db . \
		  common/tests core/tests daemon/tests resolver/tests plugins/tests
AM_CFLAGS       = $(WARNING_CFLAGS) -I$(top_builddir) -DLIBDIR=\"@LIBDIR@\"
MURPHY_CFLAGS   = 
pkgconfigdir    = ${libdir}/pkgconfig

bin_PROGRAMS    =
lib_LTLIBRARIES =
pkgconfig_DATA  =
EXTRA_DIST      =

QUIET_GEN       = $(Q:@=@echo '  GEN   '$@;)

LEXCOMPILE      = $(LEX) $(LFLAGS) $(AM_LFLAGS)
YACCCOMPILE     = $(YACC) $(YFLAGS) $(AM_YFLAGS)

INCLUDES        = -I$(top_builddir)/src/murphy-db/include -I$(top_builddir)

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
		common/mm.h		\
		common/hashtbl.h	\
		common/mainloop.h	\
		common/utils.h		\
		common/file-utils.h	\
		common/msg.h		\
		common/refcnt.h		\
		common/transport.h

libmurphy_common_la_REGULAR_SOURCES =		\
		common/log.c			\
		common/debug.c			\
		common/mm.c			\
		common/hashtbl.c		\
		common/mainloop.c		\
		common/utils.c			\
		common/file-utils.c		\
		common/msg.c			\
		common/transport.c		\
		common/stream-transport.c	\
		common/internal-transport.c	\
		common/dgram-transport.c

libmurphy_common_la_SOURCES =				\
		$(libmurphy_common_la_REGULAR_SOURCES)	\
		common-func-info.c

libmurphy_common_la_CFLAGS  =		\
		$(AM_CFLAGS)

libmurphy_common_la_LDFLAGS =					\
		-Wl,-version-script=linker-script.common	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_common_la_LIBADD  = 		\
		-lrt

libmurphy_common_la_DEPENDENCIES = linker-script.common

libcommonincludedir      = $(includedir)/murphy/common
libcommoninclude_HEADERS = $(libmurphy_common_la_HEADERS)

# linker script generation
linker-script.common: $(libmurphy_common_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.common

# debug file:line-function mapping generation
common-func-info.c: $(libmurphy_common_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm common-func-info.c

###################################
# murphy core library
#

lib_LTLIBRARIES += libmurphy-core.la
EXTRA_DIST      += core/murphy-core.pc
pkgconfig_DATA  += core/murphy-core.pc

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
		core/event.h

libmurphy_core_la_REGULAR_SOURCES =	\
		core/context.c		\
		core/plugin.c		\
		core/console.c		\
		core/scripting.c	\
		core/method.c		\
		core/event.c

libmurphy_core_la_SOURCES =		\
		$(libmurphy_core_la_REGULAR_SOURCES) \
		core-func-info.c

libmurphy_core_la_CFLAGS  =		\
		$(AM_CFLAGS)

libmurphy_core_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.core \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_core_la_LIBADD  =		\
		libmurphy-common.la -ldl

libmurphy_core_la_DEPENDENCIES = linker-script.core libmurphy-common.la

# core linker script generation
linker-script.core: $(libmurphy_core_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.core

# debug file:line-function mapping generation
core-func-info.c: $(libmurphy_core_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm core-func-info.c

###################################
# murphy dbus library
#

if DBUS_ENABLED
lib_LTLIBRARIES += libmurphy-dbus.la
EXTRA_DIST      += common/murphy-dbus.pc
pkgconfig_DATA  += common/murphy-dbus.pc

libmurphy_dbus_ladir      =		\
		$(includedir)/murphy/common

libmurphy_dbus_la_HEADERS =		\
		common/dbus.h		\
		common/dbus-transport.h

libmurphy_dbus_la_REGULAR_SOURCES =	\
		common/dbus.c		\
		common/dbus-glue.c	\
		common/dbus-transport.c

libmurphy_dbus_la_SOURCES =		\
		$(libmurphy_dbus_la_REGULAR_SOURCES) \
		dbus-func-info.c

libmurphy_dbus_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(DBUS_CFLAGS)

libmurphy_dbus_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.dbus \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_dbus_la_LIBADD  = 		\
		-lrt $(DBUS_LIBS)

libmurphy_dbus_la_DEPENDENCIES = linker-script.dbus

libdbusincludedir      = $(includedir)/murphy/dbus
libdbusinclude_HEADERS = $(libmurphy_dbus_la_HEADERS)

# linker script generation
linker-script.dbus: $(libmurphy_dbus_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.dbus

# debug file:line-function mapping generation
dbus-func-info.c: $(libmurphy_dbus_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm dbus-func-info.c
endif

###################################
# murphy pulse glue library
#

if PULSE_ENABLED
lib_LTLIBRARIES += libmurphy-pulse.la
EXTRA_DIST      += common/murphy-pulse.pc
pkgconfig_DATA  += common/murphy-pulse.pc

libmurphy_pulse_ladir      =		\
		$(includedir)/murphy/common

libmurphy_pulse_la_HEADERS =		\
		common/pulse-glue.h

libmurphy_pulse_la_SOURCES =		\
		common/pulse-glue.c

libmurphy_pulse_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(PULSE_CFLAGS)

libmurphy_pulse_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.pulse \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_pulse_la_LIBADD  =

libmurphy_pulse_la_DEPENDENCIES = linker-script.pulse

libpulseincludedir      = $(includedir)/murphy/pulse
libpulseinclude_HEADERS = $(libmurphy_pulse_la_HEADERS)

# linker script generation
linker-script.pulse: $(libmurphy_pulse_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.pulse
endif

###################################
# murphy ecore glue library
#

if ECORE_ENABLED
lib_LTLIBRARIES += libmurphy-ecore.la
EXTRA_DIST      += common/murphy-ecore.pc
pkgconfig_DATA  += common/murphy-ecore.pc

libmurphy_ecore_ladir      =		\
		$(includedir)/murphy/common

libmurphy_ecore_la_HEADERS =		\
		common/ecore-glue.h

libmurphy_ecore_la_SOURCES =		\
		common/ecore-glue.c

libmurphy_ecore_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(ECORE_CFLAGS)

libmurphy_ecore_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.ecore \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_ecore_la_LIBADD  =

libmurphy_ecore_la_DEPENDENCIES = linker-script.ecore

libecoreincludedir      = $(includedir)/murphy/ecore
libecoreinclude_HEADERS = $(libmurphy_ecore_la_HEADERS)

# linker script generation
linker-script.ecore: $(libmurphy_ecore_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.ecore
endif

###################################
# murphy glib glue library
#

if GLIB_ENABLED
lib_LTLIBRARIES += libmurphy-glib.la
EXTRA_DIST      += common/murphy-glib.pc
pkgconfig_DATA  += common/murphy-glib.pc

libmurphy_glib_ladir      =		\
		$(includedir)/murphy/common

libmurphy_glib_la_HEADERS =		\
		common/glib-glue.h

libmurphy_glib_la_SOURCES =		\
		common/glib-glue.c

libmurphy_glib_la_CFLAGS  =		\
		$(AM_CFLAGS)		\
		$(GLIB_CFLAGS)

libmurphy_glib_la_LDFLAGS =		\
		-Wl,-version-script=linker-script.glib \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_glib_la_LIBADD  =

libmurphy_glib_la_DEPENDENCIES = linker-script.glib

libglibincludedir      = $(includedir)/murphy/glib
libglibinclude_HEADERS = $(libmurphy_glib_la_HEADERS)

# linker script generation
linker-script.glib: $(libmurphy_glib_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.glib
endif

###################################
# murphy resolver library
#

lib_LTLIBRARIES += libmurphy-resolver.la
EXTRA_DIST      += resolver/murphy-resolver.pc
pkgconfig_DATA  += resolver/murphy-resolver.pc

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
		$(SIMPLE_SCRIPT_SOURCES)

libmurphy_resolver_la_SOURCES =			\
		$(libmurphy_resolver_la_REGULAR_SOURCES)	\
		resolver-func-info.c

libmurphy_resolver_la_CFLAGS =			\
		$(AM_CFLAGS)                    \
		$(INCLUDES)

libmurphy_resolver_la_LDFLAGS =			\
		-Wl,-version-script=linker-script.resolver \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_resolver_la_LIBADD =	    	\
		libmurphy-core.la   	\
		libmurphy-common.la 	\
		murphy-db/mql/libmql.la \
		murphy-db/mqi/libmqi.la \
		murphy-db/mdb/libmdb.la

libmurphy_resolver_la_DEPENDENCIES = linker-script.resolver  \
				     libmurphy-core.la       \
				     libmurphy-common.la     \
				     murphy-db/mql/libmql.la \
				     murphy-db/mqi/libmqi.la \
				     murphy-db/mdb/libmdb.la

# resolver lexical analyser and parser generation
RESOLVER_PARSER_PREFIX = yy_res_

resolver/scanner.c: resolver/scanner.l resolver/parser.h
	$(LEXCOMPILE) -P $(RESOLVER_PARSER_PREFIX) $<
	mv lex.$(RESOLVER_PARSER_PREFIX).c $@

resolver/parser.h resolver/parser.c: resolver/parser.y
	$(YACCCOMPILE) -p $(RESOLVER_PARSER_PREFIX) $<
	mv -f y.tab.h resolver/parser.h
	mv -f y.tab.c resolver/parser.c

# debug file:line-function mapping generation
resolver-func-info.c: $(libmurphy_resolver_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm resolver-func-info.c

# resolver linker script generation
linker-script.resolver: $(libmurphy_resolver_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.resolver

# simple interpreter lexical analyser and parser generation
SIMPLE_PARSER_PREFIX = yy_smpl_

resolver/scripting/simple/simple-scanner.c:		\
    resolver/scripting/simple/simple-scanner.l		\
    resolver/scripting/simple/simple-parser.h
	$(LEXCOMPILE) -P $(SIMPLE_PARSER_PREFIX) $<
	mv lex.$(SIMPLE_PARSER_PREFIX).c $@

# simple interpreter parser generation
resolver/scripting/simple/simple-parser.h 	\
resolver/scripting/simple/simple-parser.c:	\
    resolver/scripting/simple/simple-parser.y
	$(YACCCOMPILE) -p $(SIMPLE_PARSER_PREFIX) $<
	mv -f y.tab.h resolver/scripting/simple/simple-parser.h
	mv -f y.tab.c resolver/scripting/simple/simple-parser.c

###################################
# murphy resource library
#

lib_LTLIBRARIES += libmurphy-resource.la
EXTRA_DIST      += resource/murphy-resource.pc
pkgconfig_DATA  += resource/murphy-resource.pc

libmurphy_resource_ladir =			\
		$(includedir)/murphy/resource

libmurphy_resource_la_HEADERS =			\
		resource/data-types.h           \
		resource/common-api.h           \
		resource/client-api.h           \
		resource/manager-api.h          \
		resource/config-api.h           \
		resource/protocol.h

libmurphy_resource_la_REGULAR_SOURCES =		\
		resource/attribute.c		\
		resource/resource.c		\
		resource/resource-set.c		\
		resource/application-class.c	\
		resource/resource-owner.c	\
		resource/resource-client.c	\
		resource/zone.c

libmurphy_resource_la_SOURCES =			\
		$(libmurphy_resource_la_REGULAR_SOURCES)	\
		resource-func-info.c

libmurphy_resource_la_CFLAGS =			\
		$(AM_CFLAGS)			\
		$(INCLUDES)

libmurphy_resource_la_LDFLAGS =			\
		-Wl,-version-script=linker-script.resource \
		-version-info @MURPHY_VERSION_INFO@

libmurphy_resource_la_LIBADD =	    \
		libmurphy-core.la   \
		libmurphy-common.la \
		murphy-db/mql/libmql.la \
		murphy-db/mqi/libmqi.la \
		murphy-db/mdb/libmdb.la

libmurphy_resource_la_DEPENDENCIES = linker-script.resource \
				     libmurphy-core.la      \
				     libmurphy-common.la

# debug file:line-function mapping generation
resource-func-info.c: $(libmurphy_resource_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm resource-func-info.c

# resource linker script generation
linker-script.resource: $(libmurphy_resource_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.resource


###################################
# murphy lua utilities
#

lib_LTLIBRARIES += libmurphy-lua-utils.la
EXTRA_DIST      += core/lua-utils/murphy-lua-utils.pc
pkgconfig_DATA  += core/lua-utils/murphy-lua-utils.pc

libmurphy_lua_utils_ladir =					\
		$(includedir)/murphy/core/lua-utils

libmurphy_lua_utils_la_HEADERS =				\
		core/lua-utils/strarray.h			\
		core/lua-utils/funcbridge.h			\
		core/lua-utils/object.h

libmurphy_lua_utils_la_REGULAR_SOURCES =			\
		core/lua-utils/strarray.c			\
		core/lua-utils/funcbridge.c			\
		core/lua-utils/object.c

libmurphy_lua_utils_la_SOURCES =				\
		$(libmurphy_lua_utils_la_REGULAR_SOURCES)	\
		lua-utils-func-info.c

libmurphy_lua_utils_la_CFLAGS =					\
		$(AM_CFLAGS)					\
		$(LUA_CFLAGS)

libmurphy_lua_utils_la_LDFLAGS =				\
		-Wl,-version-script=linker-script.lua-utils	\
		-version-info @MURPHY_VERSION_INFO@

libmurphy_lua_utils_la_LIBADD =	    				\
		$(LUA_LIBS)

libmurphy_lua_utils_la_DEPENDENCIES = linker-script.lua-utils

# debug file:line-function mapping generation
lua-utils-func-info.c: $(libmurphy_lua_utils_la_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm lua-utils-func-info.c

# lua-utils linker script generation
linker-script.lua-utils: $(libmurphy_lua_utils_la_HEADERS)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.lua-utils


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
if DBUS_ENABLED
DBUS_PLUGIN_SOURCES = plugins/plugin-dbus.c
DBUS_PLUGIN_CFLAGS  = $(DBUS_CFLAGS)
DBUS_PLUGIN_LIBS    = libmurphy-dbus.la

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
if DBUS_ENABLED
RESOURCE_DBUS_PLUGIN_SOURCES = plugins/plugin-resource-dbus.c
RESOURCE_DBUS_PLUGIN_CFLAGS  = $(DBUS_CFLAGS)
RESOURCE_DBUS_PLUGIN_LIBS    = libmurphy-resource.la

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

# console plugin
CONSOLE_PLUGIN_REGULAR_SOURCES = plugins/plugin-console.c
CONSOLE_PLUGIN_SOURCES         = $(CONSOLE_PLUGIN_REGULAR_SOURCES) \
				 plugin-console-func-info.c
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
# debug file:line-function mapping generation
plugin-console-func-info.c: $(CONSOLE_PLUGIN_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm plugin-console-func-info.c
endif

# signalling plugin
SIGNALLING_PLUGIN_SOURCES = plugins/plugin-signalling.c \
                            plugins/signalling/transaction.c \
                            plugins/signalling/util.c \
                            plugins/signalling/client.c \
                            plugins/signalling/info.c \
                            plugins/signalling/endpoint.c \
                            plugins/signalling/plugin.h \
                            plugins/signalling/transaction.h \
                            plugins/signalling/util.h \
                            plugins/signalling/client.h \
                            plugins/signalling/info.h \
                            plugins/signalling/endpoint.h
SIGNALLING_PLUGIN_HEADERS = plugins/signalling/signalling.h
SIGNALLING_PLUGIN_CFLAGS  =
SIGNALLING_PLUGIN_LIBS    =
SIGNALLING_PLUGINdir      = $(includedir)/murphy/plugins/signalling


if BUILTIN_PLUGIN_SIGNALLING
BUILTIN_PLUGINS += $(SIGNALLING_PLUGIN_SOURCES)
BUILTIN_CFLAGS  += $(SIGNALLING_PLUGIN_CFLAGS)
BUILTIN_LIBS    += $(SIGNALLING_PLUGIN_LIBS)
else
plugin_signalling_la_SOURCES = $(SIGNALLING_PLUGIN_SOURCES)
plugin_signalling_la_HEADERS = $(SIGNALLING_PLUGIN_HEADERS)
plugin_signalling_la_CFLAGS  = $(SIGNALLING_PLUGIN_CFLAGS) \
                $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_signalling_la_LDFLAGS = -module -avoid-version
plugin_signalling_la_LIBADD  = $(SIGNALLING_PLUGIN_LIBS)
plugin_signalling_ladir      = $(SIGNALLING_PLUGINdir)

plugin_LTLIBRARIES    += plugin-signalling.la
endif

# native resource plugin
PLUGIN_RESOURCE_NATIVE_REGULAR_SOURCES = 				\
		plugins/resource-native/plugin-resource-native.c
PLUGIN_RESOURCE_NATIVE_SOURCES = 					\
		$(PLUGIN_RESOURCE_NATIVE_REGULAR_SOURCES)		\
                plugin-resource-native-func-info.c
PLUGIN_RESOURCE_NATIVE_CFLAGS =						\
		$(INCLUDES)
PLUGIN_RESOURCE_NATIVE_LIBS =       					\
		libmurphy-core.la					\
		libmurphy-common.la					\
		murphy-db/mqi/libmqi.la					\
		murphy-db/mdb/libmdb.la					\
		libmurphy-resource.la

plugin_resource_native_la_SOURCES = $(PLUGIN_RESOURCE_NATIVE_SOURCES)
plugin_resource_native_la_CFLAGS  = $(PLUGIN_RESOURCE_NATIVE_CFLAGS)	\
				    $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_resource_native_la_LDFLAGS = -module -avoid-version
plugin_resource_native_la_LIBADD  = $(PLUGIN_RESOURCE_NATIVE_LIBS)

plugin_LTLIBRARIES += plugin-resource-native.la

# resource-client
bin_PROGRAMS += resource-client

resource_client_SOURCES = plugins/resource-native/resource-client.c
resource_client_CFLAGS  = $(AM_CFLAGS) $(BUILTIN_CFLAGS)
resource_client_LDADD   = $(BUILTIN_LIBS) libmurphy-common.la

# debug file:line-function mapping generation
plugin-resource-native-func-info.c: $(PLUGIN_RESOURCE_NATIVE_REGULAR_SOURCES)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-debug-table -o $@ $^

clean-func-infos::
	-rm plugin-resource-native-func-info.c

# domain control plugin
DOMAIN_CONTROL_PLUGIN_SOURCES = plugins/domain-control/plugin-domain-control.c \
				plugins/domain-control/domain-control.c        \
				plugins/domain-control/proxy.c		       \
				plugins/domain-control/table.c                 \
				plugins/domain-control/message.c               \
				plugins/domain-control/notify.c

DOMAIN_CONTROL_PLUGIN_CFLAGS  =
DOMAIN_CONTROL_PLUGIN_LIBS    =

if !DISABLED_PLUGIN_DOMAIN_CONTROL
if BUILTIN_PLUGIN_DOMAIN_CONTROL
LINKEDIN_PLUGINS             += libmurphy-plugin-domain-control.la
lib_LTLIBRARIES              += libmurphy-plugin-domain-control.la
DOMAIN_CONTROL_PLUGIN_LOADER  = linkedin-domain-control-loader.c
DOMAIN_CONTROL_PLUGIN_CFLAGS += $(BUILTIN_CFLAGS)


libmurphy_plugin_domain_control_ladir =				 \
		$(includedir)/murphy/domain-control

libmurphy_plugin_domain_control_la_SOURCES =			 \
		$(DOMAIN_CONTROL_PLUGIN_SOURCES)		 \
		$(DOMAIN_CONTROL_PLUGIN_LOADER)

libmurphy_plugin_domain_control_la_CFLAGS =			 \
		$(DOMAIN_CONTROL_PLUGIN_CFLAGS)			 \
		$(AM_CFLAGS)

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
		libmurphy-core.la			\
		libmurphy-common.la			\
		murphy-db/mql/libmql.la			\
		murphy-db/mqi/libmqi.la			\
		murphy-db/mdb/libmdb.la

# linkedin domain control plugin linker script generation
linker-script.domain-control: $(DOMAIN_CONTROL_PLUGIN_LOADER:%.c=%.h)
	$(QUIET_GEN)$(top_builddir)/build-aux/gen-linker-script -q -o $@ $^

clean-linker-script::
	-rm -f linker-script.domain-control
else
plugin_domain_control_la_SOURCES = $(DOMAIN_CONTROL_PLUGIN_SOURCES)
plugin_domain_control_la_CFLAGS  = $(DOMAIN_CONTROL_PLUGIN_CFLAGS) \
				   $(MURPHY_CFLAGS) $(AM_CFLAGS)
plugin_domain_control_la_LDFLAGS = -module -avoid-version
plugin_domain_control_la_LIBADD  = $(DOMAIN_CONTROL_PLUGIN_LIBS)
plugin_LTLIBRARIES              += plugin-domain-control.la
endif

# domain controller client library
lib_LTLIBRARIES                        += libmurphy-domain-controller.la
libmurphy_domain_controller_la_SOURCES  = plugins/domain-control/client.c      \
					  plugins/domain-control/table-common.c\
					  plugins/domain-control/message.c
libmurphy_domain_controller_la_CFLAGS   =
libmurphy_domain_controller_la_LIBADD   = libmurphy-common.la     \
					  murphy-db/mql/libmql.la \
					  murphy-db/mqi/libmqi.la \
					  murphy-db/mdb/libmdb.la

# test domain controller
bin_PROGRAMS += test-domain-controller

test_domain_controller_SOURCES = plugins/domain-control/test-client.c
test_domain_controller_CFLAGS  = $(AM_CFLAGS)
test_domain_controller_LDADD   = libmurphy-domain-controller.la    \
				 libmurphy-common.la \
				 -lreadline
endif

###################################
# murphy daemon
#

bin_PROGRAMS += murphyd
EXTRA_DIST   += $(config_DATA)
configdir     = $(sysconfdir)/murphy
config_DATA   = daemon/murphy.conf

murphyd_SOURCES =			\
		daemon/daemon.c		\
		daemon/config.c		\
		$(BUILTIN_PLUGINS)	\
		load-linkedin-plugins.c

murphyd_CFLAGS  =			\
		$(AM_CFLAGS)		\
		$(BUILTIN_CFLAGS)

murphyd_LDADD  =			\
		$(BUILTIN_LIBS)         \
		$(LINKEDIN_PLUGINS)	\
		libmurphy-resource.la   \
		libmurphy-resolver.la	\
		libmurphy-core.la	\
		libmurphy-common.la

murphyd_LDFLAGS = -rdynamic



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
		libmurphy-common.la	\
		$(BUILTIN_LIBS)

if DBUS_ENABLED
murphy_console_LDADD +=	libmurphy-dbus.la
endif

murphy_console_LDFLAGS = -rdynamic -lreadline
endif

# cleanup
clean-local:: # clean-linker-script clean-func-infos
	-rm -f *~
