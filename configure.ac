
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([murphy],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [krisztian.litkey at intel.com])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([src/config.h])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

m4_define(version_major, `echo $VERSION | cut -d. -f1 | cut -d- -f1`)
m4_define(version_minor, `echo $VERSION | cut -d. -f2 | cut -d- -f1`)
m4_define(version_patch, `echo $VERSION | cut -d. -f3 | cut -d- -f1`)

AC_SUBST(VERSION)
AC_SUBST(VERSION_MAJOR, version_major)
AC_SUBST(VERSION_MINOR, version_minor)
AC_SUBST(VERSION_PATCH, version_patch)
AC_SUBST(VERSION_FULL, version_major.version_minor.version_patch)

MURPHY_VERSION_INFO="0:0:0"
AC_SUBST(MURPHY_VERSION_INFO)

# Disable static libraries.
AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
# We need AC_PROG_CXX if Qt support is enabled but (at least some
# versions of autotools) cannot handle conditional use of this.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_INSTALL
AM_PROG_CC_C_O
AM_PROG_LIBTOOL
AC_PROG_LEX
AC_PROG_YACC
AM_PROG_LEX
AC_SUBST(LEXLIB)

# Don't require ctags (we keep linker scripts and debug files in the repo now).
#AC_CHECK_PROG(CTAGS, ctags, "ctags")
#if test "$CTAGS" != "ctags" ; then
#   AC_MSG_ERROR([ctags is required])
#fi

if test "$LEX" != "flex" ; then
   AC_MSG_ERROR([flex is required])
fi

if test "$YACC" != "bison -y" ; then
   AC_MSG_ERROR([bison is required])
fi

# Checks for libraries.
AC_CHECK_LIB([dl], [dlopen dlclose dlsym dlerror])

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h stddef.h stdint.h stdlib.h string.h sys/statvfs.h sys/vfs.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_HEADER_MAJOR
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime memmove memset regcomp strcasecmp strchr strdup strrchr strtol strtoul])

# Check and enable extra compiler warnings if they are supported.
AC_ARG_ENABLE(extra-warnings,
              [  --enable-extra-warnings enable extra compiler warnings],
	      [extra_warnings=$enableval], [extra_warnings=auto])

WARNING_CFLAGS=""
warncflags="-Wall -Wextra"
if test "$extra_warnings" != "no"; then
    save_CPPFLAGS="$CPPFLAGS"
    for opt in $warncflags; do
        AC_PREPROC_IFELSE([AC_LANG_PROGRAM([])],
	                  [WARNING_CFLAGS="$WARNING_CFLAGS $opt"])
    done
    CPPFLAGS="$save_CPPFLAGS"
fi

AC_SUBST(WARNING_CFLAGS)

# check for Lua
PKG_CHECK_MODULES(LUA, lua >= 5.1.1, [try_lua51=no], [try_lua51=yes])

# At least Ubuntu packages lua 5.1 as lua5.1... try that if needed.
if test "$try_lua51" = "yes"; then
    PKG_CHECK_MODULES(LUA, lua5.1 >= 5.1.1)
fi
AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)

# Check if potentially GPL bits are allowed to be enabled.
AC_ARG_ENABLE(gpl,
              [  --enable-gpl            enable linking against GPL code],
	      [enable_gpl=$enableval], [enable_gpl=no])

# Check if DBUS was enabled.
AC_ARG_ENABLE(dbus,
              [  --enable-dbus           enable D-BUS support],
	      [enable_dbus=$enableval], [enable_dbus=no])

if test "$enable_dbus" = "yes"; then
    if test "$enable_gpl" = "no"; then
        AC_MSG_ERROR([D-Bus support requires the --enable-gpl option.])
    fi
    PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.70)

    DBUS_SESSION_DIR="`pkg-config --variable session_bus_services_dir dbus-1`"
    AC_SUBST(DBUS_SESSION_DIR)
else
    AC_MSG_NOTICE([D-Bus support is disabled.])
fi

if test "$enable_dbus" = "yes"; then
    AC_DEFINE([DBUS_ENABLED], 1, [Enable D-BUS support ?])
fi

AM_CONDITIONAL(DBUS_ENABLED, [test "$enable_dbus" = "yes"])
AC_SUBST(DBUS_ENABLED)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# Check if PulseAudio mainloop support was enabled.
AC_ARG_ENABLE(pulse,
              [  --enable-pulse          enable PulseAudio mainloop support],
	      [enable_pulse=$enableval], [enable_pulse=auto])

if test "$enable_pulse" != "no"; then
    PKG_CHECK_MODULES(PULSE, libpulse >= 0.9.22,
    			     [have_pulse=yes], [have_pulse=no])
    if test "$have_pulse" = "no" -a "$enable_pulse" = "yes"; then
        AC_MSG_ERROR([PulseAudio development libraries not found.])
    fi

    if test "$enable_gpl" = "no"; then
        if test "$enable_pulse" = "yes"; then
           AC_MSG_ERROR([PulseAudio support requires the --enable-gpl option.])
        else
           enable_pulse="no"
        fi
    else
        enable_pulse="$have_pulse"
    fi
else
    AC_MSG_NOTICE([PulseAudio mainloop support is disabled.])
fi

if test "$enable_pulse" = "yes"; then
    AC_DEFINE([PULSE_ENABLED], 1, [Enable PulseAudio mainloop support ?])
fi
AM_CONDITIONAL(PULSE_ENABLED, [test "$enable_pulse" = "yes"])
AC_SUBST(PULSE_ENABLED)
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)

# Check if EFL/ecore mainloop support was enabled.
AC_ARG_ENABLE(ecore,
              [  --enable-ecore          enable EFL/ecore mainloop support],
	      [enable_ecore=$enableval], [enable_ecore=auto])

if test "$enable_ecore" != "no"; then
    PKG_CHECK_MODULES(ECORE, ecore,
                      [have_ecore=yes], [have_ecore=no])
    if test "$have_ecore" = "no" -a "$enable_ecore" = "yes"; then
        AC_MSG_ERROR([EFL/ecore development libraries not found.])
    fi

    enable_ecore="$have_ecore"
else
    AC_MSG_NOTICE([EFL/ecore mainloop support is disabled.])
fi

if test "$enable_ecore" = "yes"; then
    AC_DEFINE([ECORE_ENABLED], 1, [Enable EFL/ecore mainloop support ?])
fi
AM_CONDITIONAL(ECORE_ENABLED, [test "$enable_ecore" = "yes"])
AC_SUBST(ECORE_ENABLED)
AC_SUBST(ECORE_CFLAGS)
AC_SUBST(ECORE_LIBS)

# Check if glib mainloop support was enabled.
AC_ARG_ENABLE(glib,
              [  --enable-glib           enable glib mainloop support],
	      [enable_glib=$enableval], [enable_glib=auto])

if test "$enable_glib" != "no"; then
    PKG_CHECK_MODULES(GLIB, glib-2.0,
                      [have_glib=yes], [have_glib=no])
    if test "$have_glib" = "no" -a "$enable_glib" = "yes"; then
        AC_MSG_ERROR([glib development libraries not found.])
    fi

    enable_glib="$have_glib"
else
    AC_MSG_NOTICE([glib mainloop support is disabled.])
fi

if test "$enable_glib" = "yes"; then
    AC_DEFINE([GLIB_ENABLED], 1, [Enable glib mainloop support ?])
fi
AM_CONDITIONAL(GLIB_ENABLED, [test "$enable_glib" = "yes"])
AC_SUBST(GLIB_ENABLED)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# Check if qt mainloop support was enabled.
AC_ARG_ENABLE(qt,
              [  --enable-qt             enable qt mainloop support],
              [enable_qt=$enableval], [enable_qt=auto])

if test "$enable_qt" != "no"; then
    PKG_CHECK_MODULES(QTCORE, QtCore,
                      [have_qt=yes], [have_qt=no])
    if test "$have_qt" = "no" -a "$enable_qt" = "yes"; then
        AC_MSG_ERROR([Qt(Core) development libraries not found.])
    fi

    enable_qt="$have_qt"
else
    AC_MSG_NOTICE([Qt mainloop support is disabled.])
fi

if test "$enable_qt" = "yes"; then
    AC_DEFINE([QT_ENABLED], 1, [Enable qt mainloop support ?])
    QT_MOC="`pkg-config --variable moc_location QtCore`"
    AC_SUBST(QT_MOC)
fi
AM_CONDITIONAL(QT_ENABLED, [test "$enable_qt" = "yes"])
AC_SUBST(QT_ENABLED)
AC_SUBST(QTCORE_CFLAGS)
AC_SUBST(QTCORE_LIBS)

# Check if building murphy-console was enabled.
AC_ARG_ENABLE(console,
              [  --enable-console        build Murphy console],
	      [enable_console=$enableval], [enable_console=yes])

if test "$enable_console" = "no"; then
    AC_MSG_NOTICE([Murphy console binary is disabled.])
else
    AC_MSG_NOTICE([Murphy console binary is enabled.])
fi

if test "$enable_console" = "yes"; then
    AC_DEFINE([CONSOLE_ENABLED], 1, [Build Murphy console ?])
fi
AM_CONDITIONAL(CONSOLE_ENABLED, [test "$enable_console" = "yes"])
AC_SUBST(CONSOLE_ENABLED)
AC_SUBST(READLINE_CFLAGS)
AC_SUBST(READLINE_LIBS)

# Set up murphy CFLAGS and LIBS.
MURPHY_CFLAGS=""
MURPHY_LIBS=""
AC_SUBST(MURPHY_CFLAGS)
AC_SUBST(MURPHY_LIBS)

# Add LIBDIR to config.h.
AC_MSG_CHECKING([libdir])
AC_MSG_RESULT([$libdir])
AC_SUBST(LIBDIR, [$libdir])

#Check whether we build resources or not
AC_ARG_WITH(resources,
            [  --with-resources wheter to build resource management support],
	    [with_resources=$withval],[with_resources=yes])

AM_CONDITIONAL(BUILD_RESOURCES,  [ test x$with_resources = "xyes" ])


# Check which plugins should be disabled.
AC_ARG_WITH(disabled-plugins,
            [  --with-disabled-plugins=<plugin-list> specify which plugins to disable],
            [disabled_plugins=$withval],[disabled_plugins=none])

# Check which plugins should be compiled as standalone DSOs.
AC_ARG_WITH(dynamic-plugins,
            [  --with-dynamic-plugins=<plugin-list>  specify which plugins compile as DSOs],
            [dynamic_plugins=$withval],[dynamic_plugins=none])

all_plugins=$(find src/plugins/. -name plugin-*.c 2>/dev/null | \
              sed 's#^.*/plugin-##g;s#\.c$##g' | tr '\n' ' ')

#echo "all plugins: [$all_plugins]"

case $dynamic_plugins in
    all)  dynamic_plugins="$all_plugins";;
    none) dynamic_plugins="";;
esac

internal=""; it=""
external=""; et=""
disabled=""; dt=""
for plugin in $all_plugins; do 
    type=internal

    for p in ${dynamic_plugins//,/ }; do
        if test "$plugin" = "$p"; then
            type=external
        fi
    done

    for p in ${disabled_plugins//,/ }; do
        if test "$plugin" = "$p"; then
            type=disabled
        fi
    done

    case $type in
        internal) internal="$internal$it$plugin"; it=" ";;
        external) external="$external$et$plugin"; et=" ";;
        disabled) disabled="$disabled$dt$plugin"; dt=" ";;
    esac
done

DISABLED_PLUGINS="$disabled"
INTERNAL_PLUGINS="$internal"
EXTERNAL_PLUGINS="$external"


function check_if_disabled() {
    for p in $DISABLED_PLUGINS; do
        if test "$1" = "$p"; then
            return 0
        fi
    done

    return 1
}

function check_if_internal() {
    for p in $INTERNAL_PLUGINS; do
        if test "$1" = "$p"; then
            return 0
        fi
    done

    return 1
}

AM_CONDITIONAL(DISABLED_PLUGIN_TEST,     [check_if_disabled test])
AM_CONDITIONAL(DISABLED_PLUGIN_DBUS,     [check_if_disabled dbus])
AM_CONDITIONAL(DISABLED_PLUGIN_GLIB,     [check_if_disabled glib])
AM_CONDITIONAL(DISABLED_PLUGIN_CONSOLE,  [check_if_disabled console])
AM_CONDITIONAL(DISABLED_PLUGIN_SIGNALLING, [check_if_disabled signalling])
AM_CONDITIONAL(DISABLED_PLUGIN_RESOURCE_DBUS, [check_if_disabled resource-dbus])
AM_CONDITIONAL(DISABLED_PLUGIN_DOMAIN_CONTROL,
               [check_if_disabled domain-control])

AM_CONDITIONAL(BUILTIN_PLUGIN_TEST,     [check_if_internal test])
AM_CONDITIONAL(BUILTIN_PLUGIN_DBUS,     [check_if_internal dbus])
AM_CONDITIONAL(BUILTIN_PLUGIN_GLIB,     [check_if_internal glib])
AM_CONDITIONAL(BUILTIN_PLUGIN_CONSOLE,  [check_if_internal console])
AM_CONDITIONAL(BUILTIN_PLUGIN_SIGNALLING, [check_if_internal signalling])
AM_CONDITIONAL(BUILTIN_PLUGIN_RESOURCE_DBUS, [check_if_internal resource-dbus])
AM_CONDITIONAL(BUILTIN_PLUGIN_DOMAIN_CONTROL,
               [check_if_internal domain-control])
AM_CONDITIONAL(BUILTIN_PLUGIN_LUA,      [check_if_internal lua])

# Check for Check (unit test framework).
PKG_CHECK_MODULES(CHECK, 
                  check >= 0.9.4,
                  [has_check="yes"], [has_check="no"])
AM_CONDITIONAL(HAVE_CHECK, test "x$has_check" = "xyes")

AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

if test "x$has_check" = "xno"; then
    AC_MSG_WARN([Check framework not found, unit tests are DISABLED.])
fi

# Check for documentation tools
AC_ARG_WITH([documentation],
            [AS_HELP_STRING([--with-documentation],
                            [generate pdf, html and other doc files])],
            [],
            [with_documentation=auto]
)

AS_IF( [ test x$with_documentation = xno ],
       [ has_doc_tools="no" ],
       [ AC_PATH_TOOL([MRP_DOXYGEN], doxygen)
         AC_PATH_TOOL([MRP_LYX], lyx)
         AC_PATH_TOOL([MRP_INKSCAPE], inkscape)
         AC_PATH_TOOL([MRP_PYTHON], python)
         AC_PATH_TOOL([MRP_TOUCH], touch)
         AC_PATH_TOOL([MRP_DBLATEX], dblatex)
         AC_PATH_TOOL([MRP_XMLTO], xmlto)

         AS_IF( [ test x$MRP_DOXYGEN = x -o x$MRP_LYX = x -o \
                       x$MRP_INKSCAPE = x -o x$MRP_PYTHON = x -o \
                       x$MRP_TOUCH = x],
                [ has_doc_tools="no";
                  AC_MSG_WARN([Some essential doc-tool is missing]) ],
                [ has_doc_tools="yes";
                  MRP_DOCINIT() ]
         ) ]
)

AS_IF( [ test x$has_doc_tools == "xno" -o x$MRP_DBLATEX = x ],
       [ can_make_pdfs="no";
         AC_WARN([No PDF documentation will be generated]) ],
       [ can_make_pdfs="yes"]
)

AS_IF([ test x$has_doc_tools == "xno" -o x$MRP_XMLTO = x ],
      [ can_make_html="no";
        AC_WARN([No HTML documentation will be generated]) ],
      [ can_make_html="yes" ]
)


AM_CONDITIONAL(BUILD_DOCUMENTATION,  [ test x$has_doc_tools = "xyes" ])
AM_CONDITIONAL(BUILD_PDF_DOCUMENTS,  [ test x$can_make_pdfs = "xyes" ])
AM_CONDITIONAL(BUILD_HTML_DOCUMENTS, [ test x$can_make_html = "xyes" ])

AC_SUBST(MRP_DOCDIR, [`pwd`/doc])
AC_SUBST(MRP_FIGDIR, [$MRP_DOCDIR/common/figures])
AC_SUBST(MRP_MAKE_DOCRULES, [$MRP_DOCDIR/Makefile.rules])
AC_SUBST(MRP_DOCSCRIPT_DIR, [$MRP_DOCDIR/scripts])
AC_SUBST(MRP_ABNF, [$MRP_DOCSCRIPT_DIR/abnf.py])
AC_SUBST(MRP_DBLYXFIX, [$MRP_DOCSCRIPT_DIR/dblyxfix.py])
AC_SUBST(MRP_DOXML2DB, [$MRP_DOCSCRIPT_DIR/doxml2db.py])
AC_SUBST(MRP_DOXYDEPS, [$MRP_DOCSCRIPT_DIR/doxydeps.py])


# Shave by default.
SHAVE_INIT([build-aux], [enable])

# Create murphy symlink to src.
if test ! -L murphy; then
    AC_MSG_NOTICE([Symlinking src to murphy...])
    ln -s src murphy
fi

# Generate output.
AC_CONFIG_FILES([build-aux/shave
		 build-aux/shave-libtool
		 Makefile
		 src/Makefile
		 src/common/tests/Makefile
		 src/core/tests/Makefile
		 src/core/lua-decision/tests/Makefile
		 src/daemon/tests/Makefile
		 src/plugins/tests/Makefile
		 src/common/murphy-common.pc
		 src/common/murphy-dbus.pc
		 src/common/murphy-pulse.pc
		 src/common/murphy-ecore.pc
		 src/common/murphy-glib.pc
         src/common/murphy-qt.pc
		 src/core/murphy-core.pc
         src/core/lua-utils/murphy-lua-utils.pc
         src/core/lua-decision/murphy-lua-decision.pc
		 src/breedline/breedline.pc
		 src/breedline/breedline-murphy.pc
		 src/breedline/breedline-glib.pc
		 src/breedline/tests/Makefile
		 src/murphy-db/Makefile
		 src/murphy-db/mdb/Makefile
		 src/murphy-db/mqi/Makefile
		 src/murphy-db/mql/Makefile
		 src/murphy-db/include/Makefile
		 src/murphy-db/tests/Makefile
		 src/resolver/murphy-resolver.pc
		 src/resolver/tests/Makefile
		 src/resource/murphy-resource.pc
		 src/plugins/domain-control/murphy-domain-controller.pc
		 doc/Makefile
		 doc/plugin-developer-guide/Makefile
		 doc/plugin-developer-guide/db/Makefile
		 doc/plugin-developer-guide/doxml/Makefile
		 ])
AC_OUTPUT


# Display the configuration.
echo "----- configuration -----"
echo "Extra C warnings flags: $WARNING_CFLAGS"
echo "D-Bus support: $enable_dbus"
echo "PulseAudio mainloop support: $enable_pulse"
echo "EFL/ecore mainloop support: $enable_ecore"
echo "glib mainloop support: $enable_glib"
echo "Qt mainloop support: $enable_qt"
echo "Murphy console plugin and client: $enable_console"
echo "Resource management support: $with_resources"
echo "Plugins:"
echo "  - linked-in:"
for plugin in ${INTERNAL_PLUGINS:-none}; do
    echo "      $plugin"
done
echo "  - dynamic:"
for plugin in ${EXTERNAL_PLUGINS:-none}; do
    echo "      $plugin"
done
echo "  - disabled:"
for plugin in ${DISABLED_PLUGINS:-none}; do
    echo "      $plugin"
done
