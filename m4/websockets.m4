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

# Check if we have properly packaged libwebsockets (json-c is now mandatory
# and already has been tested for).
if test "$enable_websockets" != "no"; then
    PKG_CHECK_MODULES(WEBSOCKETS, [libwebsockets],
                      [have_websockets=yes], [have_websockets=no])
    if test "$have_websockets" = "yes"; then
        WEBSOCKETS_CFLAGS="`pkg-config --cflags libwebsockets`"
        # Check for a couple of recent features we need to adopt to.
        saved_CFLAGS="$CFLAGS"
        saved_LDFLAGS="$LDFLAGS"
        saved_LIBS="$LIBS"
        # Note that (at least with autoconf 2.69 and gcc 4.7.2), setting
        # LD_AS_NEEDED to 1 breaks AC_LINK_IFELSE. That macro generates
        # the compilation command so that the libraries are specified
        # before the generated C source so all referenced/tested symbols
        # from any of the libraries end up being undefined. This fools
        # AC_LINK_IFELSE to consider the test a failure and select the
        # else branch.
        # rpmbuild always sets LD_AS_NEEDED to 1. To work around this save
        # and restore LD_AS_NEEDED for the duration of the AC_LINK_IFELSE
        # tests.
        saved_LD_AS_NEEDED="$LD_AS_NEEDED"
        unset LD_AS_NEEDED
        CFLAGS="`pkg-config --cflags libwebsockets`"
        LIBS="`pkg-config --libs libwebsockets`"

        # Check for new context creation API.
        AC_MSG_CHECKING([for WEBSOCKETS new context creation API])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[struct libwebsocket_context *ctx;
                   ctx = libwebsocket_create_context(NULL);]])],
            [websockets_cci=yes],
            [websockets_cci=no])
        AC_MSG_RESULT([$websockets_cci])

        # Check for ssl_cipher_list in context creation info
        if test "$websockets_cci" = "yes"; then
            AC_MSG_CHECKING([for WEBSOCKETS cipher list support])
            AC_LINK_IFELSE(
               [AC_LANG_PROGRAM(
                     [[#include <stdlib.h>
                       #include <libwebsockets.h>]],
                     [[struct libwebsocket_context *ctx;
                       void *ptr;
                       ptr = &ctx->ssl_cipher_list;]])],
                [websockets_cipherlist=yes],
                [websockets_cipherlist=no])
            AC_MSG_RESULT([$websockets_cipherlist])
        fi

        # Check for pollargs struct support
        if test "$websockets_cci" = "yes"; then
            AC_MSG_CHECKING([for WEBSOCKETS pollargs support])
            AC_LINK_IFELSE(
               [AC_LANG_PROGRAM(
                     [[#include <stdlib.h>
                       #include <libwebsockets.h>]],
                     [[struct libwebsocket_pollargs = NULL;]])],
                [websockets_passfd=pollargs],
                [websockets_passfd=no])
            AC_MSG_RESULT([$websockets_passfd])
        else
            websockets_passfd=no
        fi

        # Argh... try to detect the poll fd passing API...
        if test "$websocket_passfd" != "pollargs"; then
            AC_MSG_CHECKING([for WEBSOCKETS POLL fd passing API])
            AC_LINK_IFELSE(
               [AC_LANG_PROGRAM(
                     [[#include <stdlib.h>
                       #include <libwebsockets.h>]],
                     [[printf("%d\n", LWS_MAX_HEADER_NAME_LENGTH);]])],
                [websockets_passfd=user],
                [websockets_passfd=in])
            AC_MSG_RESULT([$websockets_passfd])
        fi

        # Check for new libwebsockets_get_internal_extensions.
        AC_MSG_CHECKING([for WEBSOCKETS internal extension query API])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[struct libwebsocket_extension *ext;
                   ext = libwebsocket_get_internal_extensions();]])],
            [websockets_query_ext=yes],
            [websockets_query_ext=no])
        AC_MSG_RESULT([$websockets_query_ext])

        # Check for newer lws_set_log_level API.
        # Note that we cheat heavily here: instead of rolling a proper
        # test, we blindly assume gcc, turn on the -Werror flag (to catch
        # calls with a mismatching function pointer) and hope that we will
        # not get false negatives because of other warnings.
        no_werror_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -Werror"
        AC_MSG_CHECKING([for WEBSOCKETS updated logging API.])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>
                   static void logger(int level, const char *line) {
                       return;
                   }]],
                 [[lws_set_log_level(LLL_INFO, logger);]])],
            [websockets_log_with_level=yes],
            [websockets_log_with_level=no])
        AC_MSG_RESULT([$websockets_log_with_level])

        # Check whether we have libwebsocket_close_and_free_session.
        AC_MSG_CHECKING([for WEBSOCKETS close_and_free_session API])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[libwebsocket_close_and_free_session(NULL, NULL, 0);]])],
            [websockets_close_session=yes],
            [websockets_close_session=no])
        AC_MSG_RESULT([$websockets_close_session])

        # Check for LWS_CALLBACK_FILTER_HTTP_CONNECTION.
        AC_MSG_CHECKING([for WEBSOCKETS LWS_CALLBACK_FILTER_HTTP_CONNECTION])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[int foo = LWS_CALLBACK_FILTER_HTTP_CONNECTION;]])],
            [websockets_filter_http_connection=yes],
            [websockets_filter_http_connection=no])
        AC_MSG_RESULT([$websockets_filter_http_connection])

        # Check for LWS_CALLBACK_CHANGE_MODE_POLL_FD.
        AC_MSG_CHECKING([for WEBSOCKETS LWS_CALLBACK_CHANGE_MODE_POLL_FD])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[int foo = LWS_CALLBACK_CHANGE_MODE_POLL_FD;]])],
            [websockets_change_poll=yes],
            [websockets_change_poll=no])
        AC_MSG_RESULT([$websockets_change_poll])

        # Check the signature of libwebsockets_serve_http_file to see if
        # it takes an extra (other_headers) argument.
        AC_MSG_CHECKING([for WEBSOCKETS libwebsockets_serve_http_file other_headers argument])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[return libwebsockets_serve_http_file(NULL, NULL, "", "", "");]])],
            [websockets_serve_file_extraarg=yes],
            [websockets_serve_file_extraarg=no])
        AC_MSG_RESULT([$websockets_serve_file_extraarg])

        CFLAGS="$saved_CFLAGS"
        LDFLAGS="$saved_LDFLAGS"
        LIBS="$saved_LIBS"
        if test -n "$saved_LD_AS_NEEDED"; then
            export LD_AS_NEEDED="$saved_LD_AS_NEEDED"
        fi
    else
        WEBSOCKETS_CFLAGS=""
    fi

    # Check for older websockets.
    if test "$have_websockets" = "no"; then
        saved_LDFLAGS="$LDFLAGS"
        saved_LIBS="$LIBS"
        LIBS="-lwebsockets"
        AC_MSG_CHECKING([for WEBSOCKETS without pkg-config support])
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM(
                 [[#include <stdlib.h>
                   #include <libwebsockets.h>]],
                 [[struct libwebsocket_context *ctx;
                   ctx = libwebsocket_create_context(0, NULL, NULL, NULL,
                                                     NULL, NULL, NULL,
                                                     -1, -1, 0, NULL);]])],
            [have_websockets=yes;old_websockets=no],
            [have_websockets=no])
        AC_MSG_RESULT([$have_websockets])
    fi

    # Check if we have a really old libwebsockets, still without
    # per-context user data.
    if test "$old_websockets" != "no"; then
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
            [old_websockets=no])
        AC_MSG_RESULT([$old_websockets])
    fi

    WEBSOCKETS_LIBS="-lwebsockets"
    if test "$old_websockets" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_OLD"
    fi
    if test "$websockets_cci" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_CONTEXT_INFO"
    fi
    if test "$websockets_cipherlist" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_CIPHER_LIST"
    fi
    if test "$websockets_passfd" = "pollargs"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_PASSFD_POLLARGS"
    else
        if test "$websockets_passfd" = "in"; then
            WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_PASSFD_IN"
        else
            WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_PASSFD_USER"
        fi
    fi

    if test "$websockets_query_ext" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_QUERY_EXTENSIONS"
    fi
    if test "$websockets_log_with_level" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_LOG_WITH_LEVEL"
    fi
    if test "$websockets_close_session" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_CLOSE_SESSION"
    fi
    if test "$websockets_filter_http_connection" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_FILTER_HTTP_CONNECTION"
    fi
    if test "$websockets_change_poll" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_CHANGE_MODE_POLL_FD"
    fi
    if test "$websockets_serve_file_extraarg" = "yes"; then
        WEBSOCKETS_CFLAGS="$WEBSOCKETS_CFLAGS -DWEBSOCKETS_SERVE_FILE_EXTRAARG"
    fi

    LDFLAGS="$saved_LDFLAGS"
    LIBS="$saved_LIBS"
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
    if test "$old_websockets" = "yes"; then
        AC_DEFINE([WEBSOCKETS_OLD], 1, [No per-context userdata ?])
    fi
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
