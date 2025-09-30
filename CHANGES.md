LPrint Change History
=====================

v1.4.0 - YYYY-MM-DD
-------------------

- Updated the default state file to match current PAPPL defaults (Issue #129)
- Updated the maximum label width of TSPL printers to 105mm (Issue #141)
- Now enable the TLS web interface unless the "no-tls" option is specified
  (Issue #161)
- Added 4 x 7.83 inch label size (Issue #144)
- Added DYMO LabelWriter Twin Turbo driver (Issue #193)
- Fixed the margins for DYMO printers (Issue #132)
- Fixed some Brother driver problems (Issue #167)
- Fixed a NumCopies issue in the TSPL driver (Issue #188)
- Added support new printers:
  - Arkscan 2054a
  - HP KE103


v1.3.1 - 2024-02-09
-------------------

- Updated "print-speed" support in TSPL driver (Issue #120 and #121)
- Fixed lprint-modify man page (Issue #122 and #126)
- Fixed snap documentation for connecting LPrint to Avahi.


v1.3.0 - 2024-01-31
-------------------

- Added new dithering algorithm to better support barcode printing with shaded
  content.
- Added experimental Brother printer support (Issue #15)
- Added basic TSPL printer support (Issue #54)
- Added basic SEIKO printer support (Issue #58)
- Added experimental Zebra CPCL printer support.
- Added support for configuration files in "/etc", "/usr/local/etc", or
  "/Library/Application Support" (macOS).
- Updated ZPL driver to report 'media-needed' reason when out of labels during a
  job.
- Fixed copies support for ZPL printers (Issue #100)
- Fixed darkness calculations for EPL and ZPL printers (Issue #104)


v1.2.0 - 2022-12-31
-------------------

- Documentation corrections (Issue #53, Issue #76)
- Added snap server configuration.
- Added `--with-systemd` configure option and install to
 `$prefix/lib/systemd/system` by default (Issue #50)
- Added EPL2 and ZPL auto-typing support (Issue #64)
- Changed the default log level for systemd to info (Issue #82)
- Fixed macOS installer to start LPrint server (Issue #48)
- Fixed configure script not using warning/preprocessor options in build
  (Issue #60)
- Fixed printer renaming algorithm to not truncate the name (Issue #60)
- Fixed missing "print-color-mode=bi-level" for bar code printing (Issue #76)
- Fixed label mode support in EPL and ZPL drivers (Issue #79)
- Fixed driver names for EPL printers (Issue #52)


v1.1.0 - 2021-12-23
-------------------

- Switched to PAPPL (Issue #20, #35)
- Fixed `lprint default` and `lprint delete` not working (Issue #17, Issue #40)
- Fixed server crashes on `SIGINT` (Issue #18)
- Fixed the reported date and time information when no printers were added
  (Issue #26)
- Fixed a startup bug on macOS (Issue #34)
- Now support auto-detection of printer drivers and auto-add USB printers the
  first time LPrint is run.
- Added the missing macOS binary package (Issue #33)
- Added launchd and systemd service files for running lprint as a service.
- Added support for Zebra/Eltron EPL2 printers (Issue #38)
- Added support for 600dpi ZPL thermal transfer printers.
- Added support for ZPL status/ready media updates.
- Added support for test pages.
- Temporarily removed support for DYMO LabelWriter Wireless printer (Issue #23)


v1.0 - 2020-02-17
-----------------

- First production release.


v1.0rc1 - 2020-02-10
--------------------

- Updated media documentation (Issue #13)
- The `lprint options` command now reports both English and metric dimensions
  for all supported sizes (Issue #14)
- Now support setting the default media source, top offset, tracking, and type
  values.
- Now save and load next job-id value for each printer, along with the
  printer-config-change-[date-]time and printer-impressions-completed values.
- Fixed some small memory leaks.


v1.0b2 - 2020-01-26
-------------------

- Added support for authentication of remote administrative requests (Issue #1)
- Added support for managing printers via web browser (Issue #1)
- The `add`, `default`, `delete`, and `modify` sub-commands now support the "-u"
  option (Issue #1)
- Fixed an ASLR issue with some Linux compilers (Issue #5)
- Added code to detach the Linux usblp kernel driver since apparently there are
  still Linux distributions shipping the old (and broken) usblp kernel driver
  (Issue #7)
- Device errors are now logged to stderr (for `lprint devices`) or the server
  log (for `lprint server`) so that it is possible to see permission and other
  access errors (Issue #8)
- Job attributes are now validated properly against the driver attributes
  (Issue #9)
- Fixed an issue in server mode - a failed IPv6 listener would prevent the
  server from starting (Issue #12)
- Added a "spool-directory" option for the `server` sub-command.
- Added a spec file for building RPMs.


v1.0b1 - 2020-01-15
-------------------

- First public release.
