# Macro to check if websockets support was enabled. This macro also
# takes care of detecting older versions of libwebsockets (lacking
# pkg-config support, no per-context userdata) and propagating this
# information to config.h and the compilation process.
#

AC_DEFUN([CHECK_WEBSOCKETS],
[
AC_LANG_PUSH([C])
AC_ARG_ENABLE(websockets,
              [  --enable-websockets     enable websockets support],
              [enable_websockets=$enableval], [enable_websockets=auto])

# Check if we have properly packaged libwebsockets (and json-c)
if test "$enable_websockets" != "no"; then
    PKG_CHECK_MODULES(WEBSOCKETS, [libwebsockets json],
                      [have_websockets=yes], [have_websockets=no])

    # If we don't, make sure next we have at least json-c before proceeding.
    if test "$have_websockets" != "yes"; then
        PKG_CHECK_MODULES(JSON, [json],
                          [have_json=yes], [have_json=no])

        if test "$have_json" = "no"; then
            AC_MSG_ERROR([No json-c development libraries found.])
        fi

        # Now check for older websockets.
        saved_LDFLAGS="$LDFLAGS"
        LDFLAGS="-lwebsockets"
        AC_MSG_CHECKING([for WEBSOCKETS without pkg-config support])
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[struct libwebsocket_context *ctx;
                   ctx = libwebsocket_create_context(0, NULL, NULL, NULL,
                                                     NULL, NULL, NULL,
                                                     -1, -1, 0, NULL);]])],
            [have_websockets=yes],
            [have_websockets=no])
        AC_MSG_RESULT([$have_websockets])

        # If still no luck, check for a really old libwebsockets
        # still without per-context user data.
        if test "$have_websockets" = "no"; then
            AC_MSG_CHECKING([for really old WEBSOCKETS])
            AC_LINK_IFELSE(
                [AC_LANG_PROGRAM(
                     [[#include <stdlib.h>
                       #include <libwebsockets.h>]],
                     [[struct libwebsocket_context *ctx;
                       ctx = libwebsocket_create_context(0, NULL, NULL, NULL,
                                                         NULL, NULL,
                                                         -1, -1, 0);]])],
                [have_websockets=yes;old_websockets=yes],
                [have_websockets=no])
            AC_MSG_RESULT([$have_websockets])
        fi

        WEBSOCKETS_CFLAGS="$JSON_CFLAGS"
        WEBSOCKETS_LIBS="-lwebsockets $JSON_LIBS"
        if test "$old_websockets" = "yes"; then
            WEBSOCKETS_CFLAGS="$WEBSOCKET_CFLAGS -DWEBSOCKETS_OLD"
        fi
        LDFLAGS="$saved_LDFLAGS"
    fi
else
    AC_MSG_NOTICE([libwebsockets support is disabled.])
fi

# Bail out if we lack mandatory support.
if test "$enable_websockets" = "yes" -a "$have_websockets" = "no"; then
    AC_MSG_ERROR([libwebsockets development libraries not found.])
fi

# Enable if found and autosupport requested.
if test "$enable_websockets" = "auto"; then
    enable_websockets=$have_websockets
fi

# If enabled, set up our autoconf variables accordingly.
if test "$enable_websockets" = "yes"; then
    AC_DEFINE([WEBSOCKETS_ENABLED], 1, [Enable websockets support ?])
    enable_json=yes
    AC_DEFINE([JSON_ENABLED], 1, [Enable JSON encoding/decoding support ?])
    if test "$old_websockets" = "yes"; then
        AC_DEFINE([WEBSOCKETS_OLD], 1, [No per-context userdata ?])
    fi
else
    enable_json=no
fi

# Finally substitute everything.
AM_CONDITIONAL(WEBSOCKETS_ENABLED, [test "$enable_websockets" = "yes"])
AM_CONDITIONAL(WEBSOCKETS_OLD, [test "$old_websockets" = "yes"])
AC_SUBST(WEBSOCKETS_ENABLED)
AC_SUBST(WEBSOCKETS_CFLAGS)
AC_SUBST(WEBSOCKETS_LIBS)
AC_SUBST(WEBSOCKETS_OLD)

AC_LANG_POP
])
