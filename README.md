LPrint - Label Printer Utility
==============================

LPrint implements printing for a variety of common label and receipt printers
connected via network or USB.  Features include:

- A single executable handles spooling, status, and server functionality.
- Multiple printers can be managed.
- Implements an IPP Everywhere print service.
- Exposes printer-specific options such as print darkness, resolution, and
  speed.
- Can print "raw", PWG Raster, and PNG files.

LPrint depends on the [CUPS software](https://www.cups.org) to provide the
HTTP, IPP, and PWG Raster support, and the
[libpng library](https://www.libpng.org) to provide PNG printing support.


Supported Printers
------------------

The following printers are currently supported:

- Dymo LabelWriter printers
- Intellitech PCL printers
- Various FGL printers
- Zebra CPCL printers
- Zebra EPL1 printers
- Zebra EPL2 printers
- Zebra ZPL printers


Legal Stuff
-----------

LPrint is Copyright © 2019 by Michael R Sweet.

LPrint is licensed under the Apache License Version 2.0 with an exception to
allow linking against GPL2/LGPL2 software (like older versions of CUPS).  See
the files "LICENSE" and "NOTICE" for more information.
