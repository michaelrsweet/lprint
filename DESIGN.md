Design Notes for LPrint
=======================

LPrint is packaged as a single executable with sub-commands, similar to how Git
and other tools are provided.  The sub-commands are:

- "cancel" to cancel one or more jobs
- "config" to add, modify, and delete printers
- "jobs" to list pending jobs and their status
- "printers" to list printers and their status
- "server" to run a server in the background
- "shutdown" to stop a running server
- "submit" to submit file(s) for printing (this is the default subcommand)


Architecture
------------

LPrint has a front-end that handles all of the client-side functions, and
automatically runs itself in the background to handle background spooling
and communication with the printers, exiting when there is nothing else to
do.

Each printer conforms to the IPP Everywhere specification and supports both
PWG Raster and the printer's native format (CPCL, Dymo, EPL, FGL, PCL, ZPL),
and if libpng is available the PNG file format, with color and grayscale image
data being thresholded to bi-level suitable for thermal label printers.  JPEG
is explicitly not supported since it is not suitable for bi-level (barcodes,
etc.) imagery.  Up to two (2) media sizes are reported (whatever is loaded in
the printer) and can be updated via the `lprint config` command.

The background server listens on a per-user UNIX domain socket.  If run
explicitly, it will also listen for network connections and advertise its
printers via DNS-SD.  The server supports a small subset of the IPP System
Service operations and attributes - basically just the Create-Printer, Delete-
Printer, Get-Printers, and Shutdown-All-Printers operations and their
corresponding attributes.  For security, System operations can only be sent
locally over the domain socket.

If the server crashes or is killed, any pending jobs are lost.  Only printers
are persisted (by default in the ~/.lprint.conf file) and their corresponding
configuration settings.
