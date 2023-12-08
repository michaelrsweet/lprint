Brother Combined Command Reference
==================================

Last Updated: November 27, 2023


QL-Series Printers
------------------

- 300x300dpi: All
- 300x600dpi: QL-570/580N/700/8xx


PT-Series Printers
------------------

- 180dpi: All


Commands
--------

- Invalidate Print Data: NUL (400 bytes for QL-series, 100 bytes for PT-series)
- Status Information Request: ESC i S (see below for 32-byte response)
- Initialize (reset): ESC @
- Set Margin (between labels, 0 for cut labels): ESC i d {nLSB} {nMSB}
- Set Raster Mode: ESC i a ^A
- Set Print Information: ESC i z {k} {media-type} {media-width} {media-length} {countLSB} {count} {count} {countMSB} {page} ^@
  - k=2: media-type specified
  - k=4: media-width specified
  - k=8: media-length specified?
  - k=64: prefer print quality
  - k=128: print recovery always on
  - media-type is as reported by status info request
  - media-width and media-length are millimeters
  - count=num-colors * (width-bytes + 3) * number-non-zero-rows + number-zero-rows
  - page=0 for first page, 1 for remaining pages
- Set Automatic Status Notification Mode: ESC i ! {n}
  - n=0: Notify (default)
  - n=1: Do not notify
- Send Monochrome Raster Graphics (PT-series): g {nLSB} {nMSB} {d1 ... dN}
  - n=number of data bytes
- Send Monochrome Raster Graphics (QL-series): g ^@ {n} {d1 ... dN}
  - n=number of data bytes
- Send Two-Color Raster Graphics: w {c} {n} {d1 ... dN}
  - c=1: First color (high energy)
  - c=2: Second color (low energy)
  - n=number of data bytes
- Zero Raster Graphics: Z
  - Not supported by QL-800
- Print Command: FF
- Print Command w/Feed: ^Z
- Set Compression Mode: M {n}
  - n=0: No compression
  - n=2: TIFF PackBits compression
- Set Number of Pages: ESC i A {n}
  - n=1-255 (number of pages)
- Set Modes: ESC i M {n}
  - n=64: auto-cut
  - n=0: no auto-cut
- Set Expanded Modes: ESC i K {n}
  - n=8: cut at end
  - n=64: higher resolution printing


Status Information Response
---------------------------

| Byte(s) | Name            | Value(s)             |
| :-----: | --------------- | -------------------- |
| 0       | Print Head Mark | 0x80                 |
| 1       | Size            | 32 (size of this)    |
| 2       | Reserved        | 0x42 ("B")           |
| 3       | Series Code     | See below            |
| 4-5     | Model Code      | See below            |
| 6       | Reserved        | 0x30 ("0")           |
| 7       | Reserved        | 0x00                 |
| 8       | Error Info 1    | See below            |
| 9       | Error Info 2    | See below            |
| 10      | Media Width     | Millimeters          |
| 11      | Media Type      | See below            |
| 12      | Reserved        | 0x00                 |
| 13      | Reserved        | 0x00                 |
| 14      | Reserved        | Not set              |
| 15      | Mode            | Various mode or 0x00 |
| 16      | Reserved        | 0x00                 |
| 17      | Media Length    | Millimeters or 0     |
| 18      | Status Type     | See below            |
| 19      | Phase Type      | See below            |
| 20-21   | Phase Number    | See below            |
| 22      | Notification #  | See below            |
| 23      | Reserved        | 0x00                 |
| 24      | Tape Color      | See below            |
| 25      | Text Color      | See below            |
| 26-29   | Hardware Info   | 0x00                 |
| 30-31   | Reserved        | 0x00                 |


Series Codes
------------

- 0x30 ("0"): PT-series
- 0x34 ("4"): QL-series


Model Codes
-----------

- 0x304F ("0O"): QL-500/550
- 0x3051 ("0Q"): QL-650TD
- 0x3050 ("0P"): QL-1050
- 0x3431 ("41"): QL-560
- 0x3432 ("42"): QL-570
- 0x3433 ("43"): QL-580N
- 0x3435 ("45"): QL-700
- 0x3434 ("44"): QL-1060N
- 0x3830 ("80"): QL-800
- 0x3930 ("90"): QL-810W
- 0x4130 ("A0"): QL-820NWB
- 0x4330 ("C0"): QL-1100
- 0x4430 ("D0"): QL-1110NWB
- 0x4530 ("E0"): QL-1115NWB
- 0x4730 ("G0"): QL-600
- 0x6430 ("d0"): PT-H500
- 0x6530 ("e0"): PT-E500
- 0x6730 ("g0"): PT-P700


Error Info 1
------------

- 0x01: media-empty
- 0x02: end-of-media
- 0x04: cutter-jam?
- 0x08: low battery
- 0x10: printer in use
- 0x20: printer turned off (powered-off?)
- 0x40: high-voltage adapter
- 0x80: fan motor error


Error Info 2
------------

- 0x01: replace/wrong media (media-needed?)
- 0x02: expansion buffer full
- 0x04: communication error
- 0x08: communication buffer full
- 0x10: cover-open
- 0x20: cancel key (QL-series)/overheating error (PT-series)
- 0x40: media cannot be fed
- 0x80: system error


Media Types
-----------

- 0x00: No media
- 0x01: Laminated tape
- 0x03: Non-laminated tape
- 0x11: Heat-shrink tube (2:1)
- 0x17: Heat-shrink tube (3:1)
- 0x0A/0x4A: Continuous paper/film
- 0x0B/0x4B: Die-cut labels
- 0xFF: Incompatible tape


Media Lengths
-------------

- 0 means "continuous tape/paper"


Status Types
------------

- 0x00: reply to status request
- 0x01: printing completed
- 0x02: error occurred
- 0x04: turned off
- 0x05: notification
- 0x06: phase change


Phase Types/Numbers
-------------------

- 0x00+0x00+0x00: Receiving state
- 0x01+0x00+0x00: Printing state


Notification Numbers
--------------------

- 0x00: Not available
- 0x01: Cover open
- 0x02: Cover closed
- 0x03: Cooling (started)
- 0x04: Cooling (finished)


Tape/Text Colors
----------------

- 0x00: All QL-series
- 0x01: white
- 0x02: other
- 0x03: clear
- 0x04: red
- 0x05: blue
- 0x06: yellow
- 0x07: green
- 0x08: black
- 0x09: clear (white text)
- 0x0A: gold
- 0x20: matte white
- 0x21: matte clear
- 0x22: matte silver
- 0x23: satin gold
- 0x24: satin silver
- 0x30: blue
- 0x31: red
- 0x40: fluorescent orange
- 0x41: fluorescent yellow
- 0x50: berry pink
- 0x51: light gray
- 0x52: lime green
- 0x60: yellow
- 0x61: pink
- 0x62: blue
- 0x70: white heat-shrink tube
- 0x90: white flexible ID
- 0x91: yellow flexible ID
- 0xf0: clearning
- 0xf1: stencil
- 0xff: incompatible


Hardware Info
-------------

