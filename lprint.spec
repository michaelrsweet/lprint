#
# RPM "spec" file for LPrint.
#

Name: lprint
Version: 1.0b2
Release: 0
Summary: LPrint, A Label Printer Application
License: ASL 2.0
Source0: https://github.com/michaelrsweet/lprint/releases/download/v%{version}/lprint-%{version}.tar.gz
URL: https://www.msweet.org/lprint
Packager: Anonymous <anonymous@example.com>
Vendor: Example Corp
# Note: Package names are as defined for Red Hat (and clone) distributions
BuildRequires: avahi-devel, cups-devel >= 2.2, gnutls-devel, libpng-devel >= 1.6, libusb-devel >= 1.0, pam-devel
#BuildRoot: /tmp/%{name}-root

%description
LPrint implements printing for a variety of common label and receipt printers
connected via network or USB.  Features include:

- A single executable handles spooling, status, and server functionality.
- Multiple printer support.
- Each printer implements an IPP Everywhere™ print service and is compatible
  with the driverless printing support in iOS, macOS, and Linux clients.
- Each printer can support options such as label modes, tear-off offsets,
  media tracking, media top offset, print darkness, resolution, roll
  selection, and speed.
- Each printer can print "raw", Apple/PWG Raster, and/or PNG files.

For more information, see the file "DOCUMENTATION.md", the man pages in the
"man" directory, and/or the LPrint project page at
"https://www.msweet.org/lprint".

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" LDFLAGS="$RPM_OPT_FLAGS" ./configure --enable-libpng --enable-libusb --enable-pam --with-dnssd=avahi
make

%install
make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/bin/lprint
%dir /usr/share/man/man1
/usr/share/man/man1/*
%dir /usr/share/man/man5
/usr/share/man/man5/*
