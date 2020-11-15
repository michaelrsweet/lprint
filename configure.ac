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
AC_INIT([lprint], [1.1.0], [https://github.com/michaelrsweet/lprint/issues], [lprint], [https://michaelrsweet.github.io/lprint])
AC_CONFIG_HEADERS([config.h])

LPRINT_VERSION="AC_PACKAGE_VERSION"
AC_SUBST([LPRINT_VERSION])
AC_DEFINE_UNQUOTED([LPRINT_VERSION], "$LPRINT_VERSION", [Version number])

LPRINT_MAJOR_VERSION="`echo AC_PACKAGE_VERSION | awk -F. '{print $1}'`"
LPRINT_MINOR_VERSION="`echo AC_PACKAGE_VERSION | awk -F. '{print $2}'`"
LPRINT_PATCH_VERSION="`echo AC_PACKAGE_VERSION | awk -F. '{print $3}'`"

AC_DEFINE_UNQUOTED([LPRINT_MAJOR_VERSION], $LPRINT_MAJOR_VERSION, [Major version number])
AC_DEFINE_UNQUOTED([LPRINT_MINOR_VERSION], $LPRINT_MINOR_VERSION, [Minor version number])
AC_DEFINE_UNQUOTED([LPRINT_PATCH_VERSION], $LPRINT_PATCH_VERSION, [Patch version number])


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


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL(PKGCONFIG, pkg-config)


dnl PAPPL library...
if $PKGCONFIG --exists pappl; then
	CFLAGS="$CFLAGS `$PKGCONFIG --cflags pappl`"
	LIBS="$LIBS `$PKGCONFIG --libs pappl`"
else
	AC_MSG_ERROR([PAPPL is required for LPrint.])
fi


dnl Extra compiler options...
AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no])
AC_ARG_ENABLE(sanitizer, [  --enable-sanitizer      build with AddressSanitizer, default=no])

if test x$enable_debug = xyes; then
	OPTIM="-g"
	CSFLAGS=""
else
	OPTIM="-g -Os"
	CSFLAGS="-o runtime"
fi
AC_SUBST(CSFLAGS)
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
