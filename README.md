LPrint - A Label Printer Application
====================================

LPrint implements printing for a variety of common label and receipt printers
connected via network or USB.  Features include:

- A single executable handles spooling, status, and server functionality.
- Multiple printer support.
- Each printer implements an IPP Everywhere™ print service and is compatible
  with the driverless printing support in iOS, macOS, and Linux clients.
- Each printer can support options such as label modes, tear-off offsets,
  media tracking, media top offset, print darkness, resolution, roll selection,
  and speed.
- Each printer can print "raw", Apple/PWG Raster, and/or PNG files.
- Each printer automatically recovers from out-of-media, power loss, and
  disconnected/bad cable issues.

For more information, see the file "DOCUMENTATION.md", the man pages in the
"man" directory, and/or the LPrint project page at
<https://www.msweet.org/lprint>.

> Note: Please use the Github issue tracker to report issues or request
> features/improvements in LPrint:
>
> <https://github.com/michaelrsweet/lprint/issues>


Requirements
------------

LPrint depends on:

- A "make" program (both GNU and BSD make are known to work),
- A C99 compiler (both Clang and GCC are known to work),
- [Avahi](https://www.avahi.org) 0.7 or later or mDNSResponder to provide
  DNS-SD support,
- [CUPS](https://www.cups.org) 2.2.0 or later to provide HTTP, IPP, and
  Apple/PWG Raster support,
- [libpng](https://www.libpng.org) 1.6.0 or later to provide PNG printing
  support, and
- [libusb](https://libusb.info) 1.0 or later to provide USB printing support.


Supported Printers
------------------

The following printers are currently supported:

- Dymo LabelWriter printers
- Zebra ZPL printers

Others will be added as time and access to printers permits.


Standards
---------

LPrint implements PWG 5100.14-2013: IPP Everywhere™ for each printer, and has a
partial implementation of PWG 5100.22-2019: IPP System Service v1.0 for
managing the print queues and default printer.

IPP extensions for label printers have been proposed in the Printer Working
Group in the IPP Label Printing Extensions v1.0 document; LPrint will be updated
to conform to the final names and values that are approved by the IPP workgroup.


Legal Stuff
-----------

LPrint is Copyright © 2019-2020 by Michael R Sweet.

LPrint is licensed under the Apache License Version 2.0 with an exception to
allow linking against GPL2/LGPL2 software (like older versions of CUPS).  See
the files "LICENSE" and "NOTICE" for more information.

LPrint is based loosely on the "ippeveprinter.c" and "rastertolabel.c" code
from CUPS.
