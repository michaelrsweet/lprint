dnl
dnl Configuration script for LPrint, a Label Printer Application
dnl
dnl Copyright © 2019-2024 by Michael R Sweet
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl


dnl We need at least autoconf 2.60...
AC_PREREQ(2.60)


dnl Package name and version...
AC_INIT([lprint], [1.4.0], [https://github.com/michaelrsweet/lprint/issues], [lprint], [https://michaelrsweet.github.io/lprint])
AC_CONFIG_HEADERS([config.h])

LPRINT_VERSION="AC_PACKAGE_VERSION"
AC_SUBST([LPRINT_VERSION])
AC_DEFINE_UNQUOTED([LPRINT_VERSION], "$LPRINT_VERSION", [Version number])

LPRINT_MAJOR_VERSION="$(echo AC_PACKAGE_VERSION | awk -F. '{print $1}')"
LPRINT_MINOR_VERSION="$(echo AC_PACKAGE_VERSION | awk -F. '{print $2}')"
LPRINT_PATCH_VERSION="$(echo AC_PACKAGE_VERSION | awk -F. '{print $3}')"

AC_DEFINE_UNQUOTED([LPRINT_MAJOR_VERSION], $LPRINT_MAJOR_VERSION, [Major version number])
AC_DEFINE_UNQUOTED([LPRINT_MINOR_VERSION], $LPRINT_MINOR_VERSION, [Minor version number])
AC_DEFINE_UNQUOTED([LPRINT_PATCH_VERSION], $LPRINT_PATCH_VERSION, [Patch version number])


dnl This line is provided to ensure that you don't run the autoheader program
dnl against this project.  Doing so is completely unsupported and WILL cause
dnl problems!
AH_TOP([#error "Somebody ran autoheader on this project which is unsupported and WILL cause problems."])


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name=$(echo $host_os | sed -e '1,$s/[0-9.]*$//g')]
[host_os_version=$(echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}')]
# Linux often does not yield an OS version we can use...
AS_IF([test "x$host_os_version" = x], [
    host_os_version="0"
])


dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"


dnl Figure out prefix - needed for systemd unit dir
AS_IF([test $prefix = NONE], [
    prefix=$ac_default_prefix
], [])

dnl Fix "exec_prefix" variable if it hasn't been specified...
AS_IF([test "$exec_prefix" = "NONE"], [
    exec_prefix="$prefix"
])

dnl Fix "bindir" variable...
AS_IF([test "$bindir" = "\${exec_prefix}/bin"], [
    bindir="$exec_prefix/bin"
])


dnl Standard programs...
AC_PROG_CC
AC_PROG_RANLIB
AC_PATH_PROG([AR], [ar])
AC_PATH_PROG([MKDIR], [mkdir])
AC_PATH_PROG([RM], [rm])


dnl Figure out the correct "ar" command flags...
AS_IF([test "$ac_cv_prog_ranlib" = ":"], [
    ARFLAGS="crs"
], [
    ARFLAGS="cr"
])
AC_SUBST([ARFLAGS])


dnl install-sh
AC_MSG_CHECKING([for install-sh script])
INSTALL="$(pwd)/install-sh"
AC_SUBST([INSTALL])
AC_MSG_RESULT([using $INSTALL])


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL([PKGCONFIG], [pkg-config])


dnl PAPPL library...
AS_IF([$PKGCONFIG --exists pappl --atleast-version=1.2], [
    CFLAGS="$CFLAGS $($PKGCONFIG --cflags pappl)"
    LIBS="$LIBS $($PKGCONFIG --libs pappl)"
], [
    AC_MSG_ERROR([PAPPL 1.2 or later is required for LPrint.])
])


dnl CUPS library...
AC_PATH_TOOL([CUPSCONFIG], [cups-config])
AC_MSG_CHECKING([for CUPS library v2.4 or higher])
AS_IF([$PKGCONFIG --exists cups3], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups3)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CFLAGS="$CFLAGS $($PKGCONFIG --cflags cups3)"
    LIBS="$LIBS $($PKGCONFIG --libs cups3)"
], [$PKGCONFIG --exists cups], [
    CUPS_VERSION="$($PKGCONFIG --modversion cups)"
    AC_MSG_RESULT([yes, v$CUPS_VERSION])

    CFLAGS="$CFLAGS $($PKGCONFIG --cflags cups)"
    LIBS="$LIBS $($PKGCONFIG --libs cups)"
], [test "x$CUPSCONFIG" = x], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([CUPS 2.4 or later is required for LPrint.])
], [
    CUPS_VERSION="$($CUPSCONFIG --version)"
    AS_CASE(["$CUPS_VERSION"],
        [1.* | 2.0* | 2.1* | 2.2* | 2.3*], [
            AC_MSG_RESULT(no)
	    AC_MSG_ERROR([CUPS 2.4 or later is required for LPrint.])
    ])

    AC_MSG_RESULT([yes, v$CUPS_VERSION])
    CFLAGS="$CFLAGS $($CUPSCONFIG --cflags)"
    LDFLAGS="$LDFLAGSS $($CUPSCONFIG --ldflags)"
    LIBS="$LIBS $($CUPSCONFIG --image --libs)"
])


dnl systemd support...
unitdir=""
AC_SUBST([unitdir])

AC_ARG_WITH([systemd], AS_HELP_STRING([--with-systemd[=PATH]], [install systemd service file, default=auto]), [
    AS_CASE([$withval], [yes], [
	with_systemd=yes
	unitdir="\$(prefix)/lib/systemd/system"
    ], [no], [
	with_systemd=no
    ], [
	with_systemd=yes
	unitdir="$withval"
    ])
], [
    AS_IF([test $host_os_name != darwin], [
	with_systemd=yes
	unitdir="\$(prefix)/lib/systemd/system"
    ])
])


dnl Support for experimental drivers...
AC_ARG_ENABLE([experimental], AS_HELP_STRING([--enable-experimental], [turn on experimental drivers, default=no]))
AS_IF([test x$enable_experimental = xyes], [
    AC_DEFINE([LPRINT_EXPERIMENTAL])
])


dnl Extra compiler options...
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AC_ARG_ENABLE([maintainer], AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode, default=no]))
AC_ARG_ENABLE([sanitizer], AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer, default=no]))

AS_IF([test x$enable_debug = xyes], [
    OPTIM="-g"
    CSFLAGS=""
], [
    OPTIM="-g -Os"
    CSFLAGS="-o runtime"
])
AC_SUBST([CSFLAGS])
AC_SUBST([OPTIM])

WARNINGS=""
AC_SUBST([WARNINGS])

AS_IF([test -n "$GCC"], [
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="-g -fsanitize=address"
    ], [
	# Otherwise use the Fortify enhancements to catch any unbounded
	# string operations...
	CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=2"
    ])

    dnl Show all standard warnings + unused variables when compiling...
    WARNINGS="-Wall -Wunused"

    dnl Drop some not-useful/unreliable warnings...
    for warning in char-subscripts format-truncation format-y2k switch unused-result; do
	AC_MSG_CHECKING([whether compiler supports -Wno-$warning])

	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wno-$warning -Werror"

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
	    AC_MSG_RESULT(yes)
	    WARNINGS="$WARNINGS -Wno-$warning"
        ], [
	    AC_MSG_RESULT(no)
	])

	CFLAGS="$OLDCFLAGS"
    done

    dnl Maintainer mode enables -Werror...
    AS_IF([test x$enable_maintainer = xyes], [
	WARNINGS="$WARNINGS -Werror"
    ])

    dnl See if PIE options are supported...
    AC_MSG_CHECKING(whether compiler supports -fPIE)
    OLDCFLAGS="$CFLAGS"
    AS_CASE(["$host_os_name"],
    [darwin*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -Wl,-pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -Wl,-pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ], [*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ])
    CFLAGS="$OLDCFLAGS"

    dnl OS-specific compiler options...
    AS_CASE(["$host_os_name"],
    [linux*], [
	CPPFLAGS="$CPPFLAGS -D__USE_MISC -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"
    ], [darwin*], [
        AS_IF([test "$host_os_version" -ge 200 -a x$enable_debug != xyes], [
            # macOS 11.0 and higher support the Apple Silicon (arm64) CPUs
	    OPTIM="$OPTIM -mmacosx-version-min=11.0 -arch x86_64 -arch arm64"
	], [test x$enable_debug != xyes], [
	    OPTIM="$OPTIM -mmacosx-version-min=11.0 -arch x86_64"
	])
    ])
])


dnl Extra linker options...
AC_ARG_WITH(ldflags, AS_HELP_STRING([--with-ldflags=...], [Specify additional LDFLAGS]), [
    LDFLAGS="$withval $LDFLAGS"
])

dnl Generate the Makefile...
AC_CONFIG_FILES([
    Makefile
    lprint.service
])
AC_OUTPUT
