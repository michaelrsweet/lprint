LPrint Change History
=====================

v1.0b2 - January ??, 2020
-------------------------

- Added support for authentication of remote administrative requests (Issue #1)
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


v1.0b1 - January 15, 2020
-------------------------

- First public release.
