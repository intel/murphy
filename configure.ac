
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
AC_PROG_AWK
AC_PROG_INSTALL
AM_PROG_CC_C_O
AM_PROG_LIBTOOL
AC_PROG_LEX
AC_PROG_YACC

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

# Check for glib.
PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# Check for low-level DBUS libs.
PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.70)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

DBUS_SESSION_DIR="`pkg-config --variable session_bus_services_dir dbus-1`"
AC_SUBST(DBUS_SESSION_DIR)


# Check and enable extra compiler warnings if they are supported.
AC_ARG_ENABLE(extra-warnings,
              [  --enable-extra-warnings         enable extra compiler warnings],
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


# Set up murphy CFLAGS and LIBS.
MURPHY_CFLAGS="$GLIB_CFLAGS $DBUS_CFLAGS"
MURPHY_LIBS="$GLIB_LIBS $DBUS_LIBS"
AC_SUBST(MURPHY_CFLAGS)
AC_SUBST(MURPHY_LIBS)

# Add LIBDIR to config.h.
AC_MSG_CHECKING([libdir])
AC_MSG_RESULT([$libdir])
AC_SUBST(LIBDIR, [$libdir])

# Check which plugins should be built in.
AC_ARG_WITH(builtin-plugins,
            [  --with-builtin-plugins=<plugin-list>  specify which plugins to link in],
            [builtin_plugins=$withval],[builtin_plugins=all])

all_plugins=$(ls src/plugins/*.c 2>/dev/null | \
              sed 's#src/plugins/plugin-##g;s#\.c$##g' | tr '\n' ' ')

case $builtin_plugins in
    all)  builtin_plugins="$all_plugins";;
    none) builtin_plugins="";;
esac

internal=""; it=""
external=""; et=""
for plugin in $all_plugins; do 
    type=external

    for p in ${builtin_plugins//,/ }; do
        if test "$plugin" = "$p"; then
            internal="$internal$it$plugin"
            type=internal
            it=" "
        fi
    done

    if test "$type" = "external"; then
        external="$external$et$plugin"
        et=" "
    fi
done

INTERNAL_PLUGINS="$internal"
EXTERNAL_PLUGINS="$external"

function check_if_internal() {
    for p in $INTERNAL_PLUGINS; do
        if test "$1" = "$p"; then
            return 0
        fi
    done

    return 1
}

AM_CONDITIONAL(BUILTIN_PLUGIN_TEST,    [check_if_internal test])
AM_CONDITIONAL(BUILTIN_PLUGIN_DBUS,    [check_if_internal dbus])
AM_CONDITIONAL(BUILTIN_PLUGIN_GLIB,    [check_if_internal glib])

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
		 src/daemon/tests/Makefile
		 src/common/murphy-common.pc
		 src/core/murphy-core.pc
                 src/murphy-db/Makefile
                 src/murphy-db/mdb/Makefile
                 src/murphy-db/mqi/Makefile
                 src/murphy-db/mql/Makefile
                 src/murphy-db/include/Makefile
                 src/murphy-db/tests/Makefile
		 doc/Makefile
		 ])
AC_OUTPUT

# Display the configuration.
echo "----- configuration -----"
echo "Extra C warnings flags: $WARNING_CFLAGS"
echo "Plugins:"
echo "  - linked-in:"
for plugin in ${INTERNAL_PLUGINS:-none}; do
    echo "      $plugin"
done

echo "  - dynamic:"
for plugin in ${EXTERNAL_PLUGINS:-none}; do
    echo "      $plugin"
done
