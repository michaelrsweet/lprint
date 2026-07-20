//
// Header file for LPrint, a Label Printer Application
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef LPRINT_BROTHER_H
#  define LPRINT_BROTHER_H

enum {
	BROTHER_FLAG_DOUBLE_DPI = 1,               // Supports double vertical resolution
	// TODO: Should we split this flag in "BROTHER_FLAG_RASTER_CMD_UPPER_G" and "BROTHER_FLAG_LONG_RESET" maybe?
	BROTHER_FLAG_IS_PT = 2,                    // Is a PT-series (not a QL series)
	BROTHER_FLAG_RASTER_NO_Z_CMD = 4,          // Do not use the Z command for empty lines
};

typedef struct lprint_brother_driver_data_s
{
	unsigned dpi;                        // Resolution for both directions
	unsigned head_width;                 // Width print head in pixels
	unsigned min_feed;                   // Minimum feed amount when cutting (before and after printable area) in 100th mm
	unsigned min_feed_noprecut;          // Minimum feed amount when not cutting (before printable area) in 100th mm
	// TODO: max_feed might be the same for all (documented) printers
	unsigned max_feed;                   // Maximum feed amount (before and after printable area) in 100th mm
	unsigned min_len;                    // Minimum label length (including margins/feed) in 100th mm
	unsigned max_len;                    // Maximum label length (including margins/feed) in 100th mm
	unsigned flags;
} lprint_brother_driver_data_t;

#endif // !LPRINT_BROTHER_H
