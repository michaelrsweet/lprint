dnl
dnl Configuration script for LPrint, a Label Printer Application
dnl
dnl Copyright © 2019-2020 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl


dnl We need at least autoconf 2.60...
AC_PREREQ(2.60)


dnl Package name and version...
AC_INIT([lprint], [1.0], [https://github.com/michaelrsweet/lprint/issues], [lprint], [https://michaelrsweet.github.io/lprint])
AC_CONFIG_HEADERS([config.h])

LPRINT_VERSION="AC_PACKAGE_VERSION"
AC_SUBST([LPRINT_VERSION])
AC_DEFINE_UNQUOTED([LPRINT_VERSION], "$LPRINT_VERSION", [Version number])


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
AC_MSG_CHECKING([for install-sh script])
INSTALL="`pwd`/install-sh"
AC_SUBST(INSTALL)
AC_MSG_RESULT([using $INSTALL])


dnl CUPS
AC_PATH_TOOL(CUPSCONFIG, cups-config)

if test "x$CUPSCONFIG" = x; then
    AC_MSG_ERROR([Sorry, this software requires libcups-dev.])
fi

case "`$CUPSCONFIG --api-version`" in
	1.* | 2.0 | 2.1)
		AC_MSG_ERROR([Sorry, you need CUPS 2.2.0 or higher.])
		;;
esac

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


dnl Random number support...
AC_CHECK_HEADER(sys/random.h, AC_DEFINE([HAVE_SYS_RANDOM_H], 1, [Have <sys/random.h> header?]))
AC_CHECK_FUNCS(arc4random getrandom gnutls_rnd)


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
		AC_DEFINE([HAVE_AVAHI], 1, [DNS-SD (mDNSResponder or Avahi)])
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
				AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])
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
					AC_DEFINE([HAVE_DNSSD], 1, [DNS-SD (mDNSResponder or Avahi)])],[
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

if test "x$PKGCONFIG" != x -a x$enable_libpng != xno; then
	AC_MSG_CHECKING([for libpng-1.6.x])
	if $PKGCONFIG --exists libpng16; then
		AC_MSG_RESULT([yes]);
		AC_DEFINE([HAVE_LIBPNG], 1, [libpng])
		CFLAGS="$CFLAGS `$PKGCONFIG --cflags libpng16`"
		LIBS="$LIBS `$PKGCONFIG --libs libpng16`"
	else
		AC_MSG_RESULT([no]);
		if test x$enable_libpng = xyes; then
			AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
		fi
	fi
elif test x$enable_libpng = xyes; then
	AC_MSG_ERROR([libpng-dev 1.6 or later required for --enable-libpng.])
fi


dnl libusb...
AC_ARG_ENABLE(libusb, [  --enable-libusb         use libusb for USB printing])

if test "x$PKGCONFIG" != x -a x$enable_libusb != xno; then
	AC_MSG_CHECKING([for libusb-1.0])
	if $PKGCONFIG --exists libusb-1.0; then
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_LIBUSB], 1, [libusb])
		CFLAGS="$CFLAGS `$PKGCONFIG --cflags libusb-1.0`"
		LIBS="$LIBS `$PKGCONFIG --libs libusb-1.0`"
		if test "x$host_os_name" = xdarwin; then
			LIBS="$LIBS -framework IOKit"
		fi
	else
		AC_MSG_RESULT([no])
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
	dnl PAM needs dlopen...
	AC_CHECK_LIB([dl], [dlopen])
	AC_CHECK_LIB([pam], [pam_start], [
		AC_DEFINE([HAVE_LIBPAM], 1, [libpam])
		LIBS="$LIBS -lpam"])

	dnl PAM has two "standard" locations for its header...
	AC_CHECK_HEADER(security/pam_appl.h, AC_DEFINE([HAVE_SECURITY_PAM_APPL_H], 1, [Have <security/pam_appl.h> header?]))
	AC_CHECK_HEADER(pam/pam_appl.h, AC_DEFINE([HAVE_PAM_PAM_APPL_H], 1, [Have <pam/pam_appl.h> header?]))

	if test x$ac_pam_start = xno -a x$enable_pam = xyes; then
		AC_MSG_ERROR([libpam-dev required for --enable-pam.])
	fi
fi


dnl Extra compiler options...
AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no])
AC_ARG_ENABLE(sanitizer, [  --enable-sanitizer      build with AddressSanitizer, default=no])

if test x$enable_debug = xyes; then
	OPTIM="-g"
else
	OPTIM="-g -Os"
fi
AC_SUBST(OPTIM)

if test -n "$GCC"; then
	if test x$enable_sanitizer = xyes; then
		# Use -fsanitize=address with debugging...
		OPTIM="-g -fsanitize=address"
	else
		# Otherwise use the Fortify enhancements to catch any unbounded
		# string operations...
		CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
		CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2"
	fi

	dnl Show all standard warnings + unused variables when compiling...
	CFLAGS="-Wall -Wunused -Wno-unknown-warning-option -Wno-unused-result -Wno-char-subscripts -Wno-format-y2k -Wno-maybe-uninitialized -Wno-switch $CFLAGS"

	dnl See if PIE options are supported...
	AC_MSG_CHECKING(whether compiler supports -fPIE)
	OLDCFLAGS="$CFLAGS"
	case "$host_os_name" in
		darwin*)
			CFLAGS="$CFLAGS -fPIC -fPIE -Wl,-pie"
			AC_TRY_COMPILE(,,[
				OLDCFLAGS="-fPIC $OLDCFLAGS"
				LDFLAGS="-fPIE -Wl,-pie $LDFLAGS"
				AC_MSG_RESULT(yes)],
				AC_MSG_RESULT(no))
			;;

		*)
			CFLAGS="$CFLAGS -fPIC -fPIE -pie"
			AC_TRY_COMPILE(,,[
				OLDCFLAGS="-fPIC $OLDCFLAGS"
				LDFLAGS="-fPIE -pie $LDFLAGS"
				AC_MSG_RESULT(yes)],
				AC_MSG_RESULT(no))
			;;
	esac
	CFLAGS="$OLDCFLAGS"
fi


dnl Extra linker options...
AC_ARG_WITH(ldflags, [  --with-ldflags=...      Specify additional LDFLAGS ],
	LDFLAGS="$withval $LDFLAGS")

dnl Generate the Makefile...
AC_OUTPUT(Makefile)
