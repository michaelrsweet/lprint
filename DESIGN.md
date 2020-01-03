Design Notes for LPrint
=======================

LPrint is packaged as a single executable with sub-commands, similar to how Git
and other tools are provided.  The sub-commands are:

- "add" to add printers
- "cancel" to cancel one or more jobs
- "delete" to delete printers
- "jobs" to list pending jobs and their status
- "modify" to modify printers
- "printers" to list printers
- "server" to run a server in the background
- "shutdown" to stop a running server
- "status" to show server or printer status
- "submit" to submit file(s) for printing (this is the default subcommand)


Architecture
------------

LPrint has a front-end that handles all of the client-side functions, and
automatically runs itself in the background to handle background spooling
and communication with the printers.

Each printer conforms to the IPP Everywhere specification and supports Apple
Raster, PWG Raster, and the printer's native format (CPCL, Dymo, EPL, FGL, PCL,
ZPL, etc.), and if libpng is available the PNG file format, with color and
grayscale image data being dithered (clustered dot) or thresholded to bi-level
suitable for thermal label printers.  JPEG is explicitly not supported since it
is not suitable for bi-level (barcodes, etc.) imagery.  Up to four (4) ready
media sizes are reported (whatever is loaded in the printer) and can be updated
manually via the `lprint modify` command.

The background server listens on a per-user UNIX domain socket.  If run
explicitly, it will also listen for network connections and advertise its
printers via DNS-SD.  The server supports a small subset of the IPP System
Service operations and attributes - basically just the Create-Printer, Delete-
Printer, Get-Printers, Get-System-Attributes, Set-System-Attributes, and
Shutdown-All-Printers operations and their corresponding attributes.  For
security, all System operations except for Get-Printers can only be sent
locally over the domain socket.

If the server crashes or is killed, any pending jobs are lost.  Only printers
are persisted (by default in the ~/.lprint.conf file) and their corresponding
configuration settings.


Attributes
----------

The following operation attributes are supported:

- "document-format (mimeMediaType)": The format of the document being printed,
  including 'application/octet-stream' for auto-typed content.
- "document-name (name(MAX))": The document title/name/filename.
- "ipp-attribute-fidelity (boolean)": Force Job Template values.
- "job-name (name(MAX))": The job title.
- "requesting-user-name (name(MAX))": The job owner.  This attribute is
  overridden by authenticated user name, if present.

The following Job Template attributes are supported:

- "copies (integer(1:MAX))": The number of copies of each label.
- "media (type2 keyword | name(MAX))": The media size.
- "media-col (collection)": The (detailed) media margins, size, source,
  tracking, and type.
- "media-col.media-source (type2 keyword | name(MAX))": 'main-roll',
  'alternate-roll'.
- "media-col.media-top-offset (integer)": The offset from the top of the label
  to start printing in hundredths of millimeters.
- "media-col.media-tracking (type2 keyword)": The media tracking mode -
  'continuous', 'mark', and 'web'.
- "media-col.media-type (type2 keyword | name(MAX))": 'continuous' and 'labels'.
- "multiple-document-handling (type2 keyword)": 'separate-documents-collated-
  copies' and 'separate-documents-uncollated-copies' to control collated copies.
- "orientation-requested (type2 enum)": 'portrait' (3), 'landscape' (4),
  'reverse-landscape' (5), and 'reverse-portrait' (6).
- "print-color-mode (type2 keyword)": 'monochrome' and 'bi-level' to provide
  dithered or threshold B&W output.
- "print-content-optimize (type2 keyword)": 'auto', 'graphic', 'photo', 'text',
  and 'text-and-graphic'. Basically 'auto' will choose a default
  "print-color-mode" value of 'monochrome' for grayscale input and 'bi-level'
  otherwise, 'photo' will choose a default "print-color-mode" value of
  'monochrome', and all other values will choose a default "print-color-mode"
  value of 'bi-level'.
- "print-darkness (integer(-100:100))": The relative darkness value for thermal
  markers - values less than 0 are lighter, greater than 0 are darker.
- "print-quality (type2 enum)": 'draft' (3), 'normal' (4), and 'high' (5).
  Chooses default "print-speed" and "printer-resolution" values accordingly.
- "print-speed (integer(1:MAX))": Print speed in hundredths of millimeters per
  second (2540 = 1 inch/sec).
- "printer-resolution (resolution)": Printer resolution in dots-per-inch - most
  printers only support a single resolution.

The following Printer Description attributes are supported:

- "copies-default (integer(1:MAX))" and
  "copies-supported (rangeOfInteger(1:MAX))"
- "document-format-default (mimeMediaType)" and
  "document-format-supported (1setOf mimeMediaType)"
- "label-mode-configured (type2 keyword)" and
  "label-mode-supported (1setOf type2 keyword)": The label printing mode -
  'applicator', 'cutter', 'cutter-delayed', 'kiosk', 'peel-off',
  'peel-off-prepeel', 'rewind', 'rfid', 'tear-off'
- "label-tear-off-configured (integer)" and
  "label-tear-off-supported (rangeOfInteger)": Offset in hundredths of
  millimeters to allow labels/receipts to be torn off safely. Positive values
  extend the media roll while negative values retract the media roll.
- "media-default (type2 keyword | name(MAX))",
  "media-ready (1setOf (type2 keyword | name(MAX)))", and
  "media-supported (1setOf (type2 keyword | name(MAX)))"
- "media-bottom-margin-supported (1setOf integer(0:MAX))",
  "media-col-database (1setOf collection)", "media-col-default (collection)",
  "media-col-ready (1setOf collection)", "media-col-supported (1setOf keyword)",
  "media-left-margin-supported (1setOf integer(0:MAX))",
  "media-right-margin-supported (1setOf integer(0:MAX))",
  "media-size-supported (1setOf collection)",
  "media-source-supported (1setOf (type2 keyword | name(MAX)))",
  "media-top-margin-supported (1setOf integer(0:MAX))",
  "media-top-offset-supported (rangeOfInteger)",
  "media-tracking-supported (1setOf type2 keyword)", and
  "media-type-supported (1setOf (type2 keyword | name(MAX)))"
- "multiple-document-handling-default (type2 keyword)" and
  "multiple-document-handling-supported (1setOf type2 keyword)"
- "orientation-requested-default (type2 enum)" and
  "orientation-requested-supported (1setOf type2 enum)"
- "pdl-override-supported (type2 keyword)" with a value of 'attempted'.
- "print-color-mode-default (type2 keyword)" and
  "print-color-mode-supported (1setOf type2 keyword"
- "print-content-optimize-default (type2 keyword)" and
  "print-content-optimize-supported (1setOf type2 keyword)"
- "print-darkness-default (integer(-100:100))" and
  "print-darkness-supported (integer(1:100))": The -supported value specifies
  the number of discrete darkness values that are actually supported.
- "print-quality-default (type2 enum)" and
  "print-quality-supported (1setOf type2 enum)"
- "print-speed-default (integer(1:MAX))" and
  "print-speed-supported (1setOf (integer(1:MAX) | rangeOfInteger(1:MAX)))"
- "printer-darkness-configured (integer(0:100))": The base darkness from
  0 (lightest) to 100 (darkest) for thermal markers.
- "printer-darkness-supported (integer(1:100))": The number of discrete darkness
  values that are actually supported.
- "printer-resolution-default (resolution)" and
  "printer-resolution-supported (1setOf resolution)"
