dnl
dnl LPrint configuration script
dnl
dnl Copyright © 2019-2020 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl


dnl We need at least autoconf 2.60...
AC_PREREQ(2.60)


dnl Package name and version...
AC_INIT([lprint], [1.0b1], [https://github.com/michaelrsweet/lprint/issues], [lprint], [https://michaelrsweet.github.io/lprint])
AC_CONFIG_HEADERS(config.h)

LPRINT_VERSION="AC_PACKAGE_VERSION"
AC_SUBST(LPRINT_VERSION)
AC_DEFINE_UNQUOTED(LPRINT_VERSION, "$LPRINT_VERSION")


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name=`echo $host_os | sed -e '1,$s/[0-9.]*$//g'`]
[host_os_version=`echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}'`]
# Linux often does not yield an OS version we can use...
if test "x$host_os_version" = x; then
        host_os_version="0"
fi


dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no])
if test x$enable_debug = xyes; then
	CFLAGS="-g $CFLAGS"
	LDFLAGS="-g $LDFLAGS"
else
	CFLAGS="-g -Os $CFLAGS"
	LDFLAGS="-g -Os $LDFLAGS"
fi


dnl Standard programs...
AC_PROG_CC(clang cc gcc)
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_PATH_PROG(MKDIR,mkdir)
AC_PATH_PROG(RM,rm)


dnl Figure out the correct "ar" command flags...
if test "$ac_cv_prog_ranlib" = ":"; then
    ARFLAGS="crs"
else
    ARFLAGS="cr"
fi
AC_SUBST(ARFLAGS)


dnl install-sh
AC_MSG_CHECKING(for install-sh script)
INSTALL="`pwd`/install-sh"
AC_SUBST(INSTALL)
AC_MSG_RESULT(using $INSTALL)


dnl CUPS
AC_PATH_TOOL(CUPSCONFIG, cups-config)

if test "x$CUPSCONFIG" = x; then
    AC_MSG_ERROR([Sorry, this software requires libcups-dev.])
fi


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL(PKGCONFIG, pkg-config)


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
AC_CHECK_HEADER(pthread.h)

if test x$ac_cv_header_pthread_h != xyes; then
    AC_MSG_ERROR([Sorry, this software requires POSIX threading support.])
fi

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


dnl DNS-SD support...
AC_ARG_WITH(dnssd, [  --with-dnssd=LIBRARY    set DNS-SD library (auto, avahi, mdnsresponder)])

if test "x$with_dnssd" != xmdnsresponder -a "x$with_dnssd" != xno -a "x$PKGCONFIG" != x -a x$host_os_name != xdarwin; then
	AC_MSG_CHECKING(for Avahi)
	if $PKGCONFIG --exists avahi-client; then
		AC_MSG_RESULT([yes])
		CFLAGS="$CFLAGS `$PKGCONFIG --cflags avahi-client`"
		LIBS="$LIBS `$PKGCONFIG --libs avahi-client`"
		AC_DEFINE(HAVE_AVAHI)
	else
		AC_MSG_RESULT([no])
		if test x$with_dnssd = xavahi; then
			AC_MSG_ERROR([libavahi-client-dev needed for --with-dnssd=avahi.])
		fi
	fi
elif test x$with_dnssd = xavahi; then
	AC_MSG_ERROR([pkgconfig and libavahi-client-dev needed for --with-dnssd=avahi.])
elif test x$with_dnssd != xavahi -a "x$with_dnssd" != xno; then
	AC_CHECK_HEADER(dns_sd.h, [
		case "$host_os_name" in
			darwin*)
				# Darwin and macOS...
				AC_DEFINE(HAVE_DNSSD)
				LIBS="$LIBS -framework CoreFoundation -framework SystemConfiguration"
				;;
			*)
				# All others...
				AC_MSG_CHECKING(for current version of dns_sd library)
				SAVELIBS="$LIBS"
				LIBS="$LIBS -ldns_sd"
				AC_TRY_COMPILE([#include <dns_sd.h>],[
					int constant = kDNSServiceFlagsShareConnection;
					unsigned char txtRecord[100];
					uint8_t valueLen;
					TXTRecordGetValuePtr(sizeof(txtRecord),
					    txtRecord, "value", &valueLen);],[
					AC_MSG_RESULT([yes])
					AC_DEFINE(HAVE_DNSSD)],[
					AC_MSG_RESULT([no])
					LIBS="$SAVELIBS"
					if test x$with_dnssd = xmdnsresponder; then
						AC_MSG_ERROR([mDNSResponder required for --with-dnssd=mdnsresponder.])
					fi])
				;;
		esac
	])
fi


dnl libpng...
AC_ARG_ENABLE(libpng, [  --enable-libpng         use libpng for PNG printing])

if test x$enable_libpng != xno; then
	AC_SEARCH_LIBS(png_image_begin_read_from_file, png, [AC_DEFINE(HAVE_LIBPNG)])

	if test x$ac_png_image_begin_read_from_file = xno -a x$enable_libpng = xyes; then
		AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
	fi
fi


dnl libusb...
AC_ARG_ENABLE(libusb, [  --enable-libusb         use libusb for USB printing])

if test "x$PKGCONFIG" != x -a x$enable_libusb != xno; then
	AC_MSG_CHECKING(for libusb-1.0)
	if $PKGCONFIG --exists libusb-1.0; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_LIBUSB)
		CFLAGS="$CFLAGS `$PKGCONFIG --cflags libusb-1.0`"
		LIBS="$LIBS `$PKGCONFIG --libs libusb-1.0`"
		if test "x$host_os_name" = xdarwin; then
			LIBS="$LIBS -framework IOKit"
		fi
	else
		AC_MSG_RESULT(no)
		if test x$enable_libusb = xyes; then
			AC_MSG_ERROR([libusb-1.0-0-dev required for --enable-libusb.])
		fi
	fi
elif test x$enable_libusb = xyes; then
	AC_MSG_ERROR([pkgconfig and libusb-1.0-0-dev required for --enable-libusb.])
fi


dnl PAM support...
AC_ARG_ENABLE(pam, [  --disable-pam           disable PAM support])

if test x$enable_pam != xno; then
	AC_CHECK_LIB(dl,dlopen)
	AC_CHECK_LIB(pam,pam_start)
	AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE(HAVE_SECURITY_PAM_APPL_H))
	AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE(HAVE_PAM_PAM_APPL_H))

	if test x$ac_pam_start = xno -a x$enable_pam = xyes; then
		AC_MSG_ERROR([libpam-dev required for --enable-pam.])
	fi
fi


dnl Generate the Makefile...
AC_OUTPUT(Makefile)
