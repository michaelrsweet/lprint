LPrint - A Label Printer Application
====================================

LPrint implements printing for a variety of common label and receipt printers
connected via network or USB.  Features include:

- A single executable handles spooling, status, and server functionality.
- Multiple printer support.
- Each printer implements an IPP Everywhere™ print service and is compatible
  with the driverless printing support in Android™, Chrome OS™, iOS®, Linux®,
  macOS®, and Windows® 10/11 clients.
- Each printer can support options such as label modes, tear-off offsets,
  media tracking, media top offset, print darkness, resolution, roll selection,
  and speed.
- Each printer can directly print "raw", Apple/PWG Raster, and/or PNG files.
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

- A POSIX-compliant "make" program (both GNU and BSD make are known to work),
- A C99 compiler (both Clang and GCC are known to work),
- [PAPPL](https://www.msweet.org/pappl) 1.1 or later.
- [CUPS](https://openprinting.github.io/cups) 2.2 or later (for libcups).


Supported Printers
------------------

The following printers are currently supported:

- DYMO LabelWriter printers
- Zebra/Eltron EPL2 printers
- Zebra ZPL printers

Others will be added as time and access to printers permits.


Standards
---------

Through PAPPL, LPrint implements PWG 5100.14-2013: IPP Everywhere™ and the IPP
Label Printing Extensions v1.0 for each printer, and has a partial
implementation of PWG 5100.22-2019: IPP System Service v1.0 for managing the
print queues and default printer.


Legal Stuff
-----------

LPrint is Copyright © 2019-2021 by Michael R Sweet.

LPrint is licensed under the Apache License Version 2.0.  See the files
"LICENSE" and "NOTICE" for more information.

LPrint is based loosely on the "rastertolabel.c" code from CUPS.
