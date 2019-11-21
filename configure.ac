dnl
dnl LPrint configuration script
dnl
dnl Copyright © 2019 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_INIT([lprint], [0.1], [https://github.com/michaelrsweet/lprint/issues], [lprint], [https://michaelrsweet.github.io/lprint])
AC_CONFIG_HEADERS(config.h)

dnl Define the version number...
MOAUTH_VERSION="AC_PACKAGE_VERSION"
AC_SUBST(LPRINT_VERSION)
AC_DEFINE_UNQUOTED(LPRINT_VERSION, "$LPRINT_VERSION")

dnl Standard programs...
AC_PROG_CC(clang cc gcc)
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_MSG_CHECKING(for install-sh script)
INSTALL="`pwd`/install-sh"
AC_SUBST(INSTALL)
AC_MSG_RESULT(using $INSTALL)
AC_PATH_PROG(MKDIR,mkdir)
AC_PATH_PROG(RM,rm)
AC_PATH_TOOL(CUPSCONFIG, cups-config)

if test "x$CUPSCONFIG" = x; then
    AC_MSG_ERROR([Sorry, this software requires CUPS.])
fi

dnl Figure out the correct "ar" command flags...
if test "$ac_cv_prog_ranlib" = ":"; then
    ARFLAGS="crs"
else
    ARFLAGS="cr"
fi
AC_SUBST(ARFLAGS)

dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"

dnl Debugging...
AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
    [if eval "test x$enable_debug = xyes"; then
	CFLAGS="-g $(CFLAGS)"
	LDFLAGS="-g $(LDFLAGS)"
    fi])

dnl CUPS library...
AC_ARG_ENABLE(static_cups, [  --enable-static-cups    use static CUPS libraries, default=no])

if test x$enable_static_cups = xyes; then
	CFLAGS="$CFLAGS `cups-config --static --cflags`"
	LDFLAGS="$LDFLAGS `cups-config --static --ldflags`"
	LIBS="$LIBS `cups-config --image --static --libs`"
else
	CFLAGS="$CFLAGS `cups-config --cflags`"
	LDFLAGS="$LDFLAGS `cups-config --ldflags`"
	LIBS="$LIBS `cups-config --image --libs`"
fi

dnl POSIX threads...
AC_CHECK_HEADER(pthread.h, AC_DEFINE(HAVE_PTHREAD_H))

if test x$ac_cv_header_pthread_h != xyes; then
    AC_MSG_ERROR([Sorry, this software requires POSIX threading support.])
fi

dnl Check various threading options for the platforms we support
for flag in -lpthreads -lpthread -pthread; do
    AC_MSG_CHECKING([for pthread_create using $flag])
    SAVELIBS="$LIBS"
    LIBS="$flag $LIBS"
    AC_LANG([C])
    AC_LINK_IFELSE([
        AC_LANG_PROGRAM([#include <pthread.h>],[pthread_create(0, 0, 0, 0);])],
        [have_pthread=yes],
	[LIBS="$SAVELIBS"])

    if test x$have_pthread = xyes; then
        AC_MSG_RESULT([yes])
        CFLAGS="$CFLAGS -D_THREAD_SAFE -D_REENTRANT"
        break
    else
        AC_MSG_RESULT([no])
    fi
done

dnl PAM support...
AC_ARG_ENABLE(pam, [  --disable-pam           disable PAM support])

PAMLIBS=""

if test x$enable_pam != xno; then
    SAVELIBS="$LIBS"

    AC_CHECK_LIB(dl,dlopen)
    AC_CHECK_LIB(pam,pam_start)
    AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE(HAVE_SECURITY_PAM_APPL_H))
    AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE(HAVE_PAM_PAM_APPL_H))

    if test x$ac_cv_lib_pam_pam_start != xno; then
	# Set the necessary libraries for PAM...
	if test x$ac_cv_lib_dl_dlopen != xno; then
	    PAMLIBS="-lpam -ldl"
	else
	    PAMLIBS="-lpam"
	fi
    fi

    LIBS="$SAVELIBS"
fi

AC_SUBST(PAMLIBS)

dnl Generate the Makefile...
AC_OUTPUT(Makefile)
