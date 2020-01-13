//
// Driver header file for LPrint, a Label Printer Utility
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _DRIVER_COMMON_H_
#  define _DRIVER_COMMON_H_

//
// Include necessary headers...
//

#  include "common.h"
#  include <cups/raster.h>


//
// Limits...
//

#  define LPRINT_MAX_MEDIA	100	// Maximum number of media sizes
#  define LPRINT_MAX_RESOLUTION	4	// Maximum number of printer resolutions
#  define LPRINT_MAX_SOURCE	4	// Maximum number of sources/rolls
#  define LPRINT_MAX_SUPPLY	4	// Maximum number of supplies
#  define LPRINT_MAX_TYPE	4	// Maximum number of media types


//
// Types...
//

typedef struct lprint_printer_s lprint_printer_t;
					// Forward defined printer
typedef struct lprint_job_s lprint_job_t;
					// Forward defined job

typedef unsigned char lprint_dither_t[16];
					// Dither array

enum lprint_label_mode_e		// Label printing modes
{
  LPRINT_LABEL_MODE_APPLICATOR = 0x0001,
  LPRINT_LABEL_MODE_CUTTER = 0x0002,
  LPRINT_LABEL_MODE_CUTTER_DELAYED = 0x0004,
  LPRINT_LABEL_MODE_KIOSK = 0x0008,
  LPRINT_LABEL_MODE_PEEL_OFF = 0x0010,
  LPRINT_LABEL_MODE_PEEL_OFF_PREPEEL = 0x0020,
  LPRINT_LABEL_MODE_REWIND = 0x0040,
  LPRINT_LABEL_MODE_RFID = 0x0080,
  LPRINT_LABEL_MODE_TEAR_OFF = 0x0100
};
typedef unsigned short lprint_label_mode_t;

enum lprint_media_tracking_e		// Media tracking modes
{
  LPRINT_MEDIA_TRACKING_CONTINUOUS = 0x0001,
  LPRINT_MEDIA_TRACKING_MARK = 0x0002,
  LPRINT_MEDIA_TRACKING_WEB = 0x0004
};
typedef unsigned short lprint_media_tracking_t;

typedef struct lprint_media_col_s	// Media details
{
  int			bottom_margin,	// Bottom margin in hundredths of millimeters
			left_margin,	// Left margin in hundredths of millimeters
			right_margin,	// Right margin in hundredths of millimeters
			size_width,	// Width in hundredths of millimeters
			size_length;	// Height in hundredths of millimeters
  char			size_name[64],	// PWG media size name
			source[64];	// PWG media source name
  int			top_margin,	// Top margin in hundredths of millimeters
			top_offset;	// Top offset in hundredths of millimeters
  lprint_media_tracking_t tracking;	// Media tracking
  char			type[64];	// PWG media type name
} lprint_media_col_t;

typedef struct lprint_options_s		// Computed job options
{
  cups_page_header2_t	header;		// Raster header
  unsigned		num_pages;	// Number of pages in job
  const lprint_dither_t	*dither;	// Dither array
  int			copies;	 	// copies
  lprint_media_col_t	media;		// media/media-col
  ipp_orient_t		orientation_requested;
					// orientation-requested
  const char		*print_color_mode,
					// print-color-mode
			*print_content_optimize;
					// print-content-optimize
  int			print_darkness;	// print-darkness
  ipp_quality_t		print_quality;	// print-quality
  int			print_speed;	// print-speed
  int			printer_resolution[2];
					// printer-resolution
} lprint_options_t;

typedef int (*lprint_printfunc_t)(lprint_job_t *job, lprint_options_t *options);
					// Print a job
typedef int (*lprint_rendjobfunc_t)(lprint_job_t *job, lprint_options_t *options);
					// End a raster job
typedef int (*lprint_rendpagefunc_t)(lprint_job_t *job, lprint_options_t *options, unsigned page);
					// End a raster job
typedef int (*lprint_rstartjobfunc_t)(lprint_job_t *job, lprint_options_t *options);
					// Start a raster job
typedef int (*lprint_rstartpagefunc_t)(lprint_job_t *job, lprint_options_t *options, unsigned page);
					// Start a raster page
typedef int (*lprint_rwritefunc_t)(lprint_job_t *job, lprint_options_t *options, unsigned y, const unsigned char *line);
					// Write a line of raster graphics
typedef int (*lprint_statusfunc_t)(lprint_printer_t *printer);
					// Update printer status

typedef struct lprint_supply_s		// Supply data
{
  const char		*color;		// Colorant, if any
  const char		*description;	// Description
  int			is_consumed;	// Is this a supply that is consumed?
  int			level;		// Level (0-100, -1 = unknown)
  const char		*type;		// Type
} lprint_supply_t;

typedef struct lprint_driver_s		// Driver data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  char			*name;		// Name of driver
  ipp_t			*attrs;		// Capability attributes
  lprint_device_t	*device;	// Connection to device
  void			*job_data;	// Driver job data
  lprint_printfunc_t	print;		// Print (file) function
  lprint_rendjobfunc_t	rendjob;	// End raster job function
  lprint_rendpagefunc_t	rendpage;	// End raster page function
  lprint_rstartjobfunc_t rstartjob;	// Start raster job function
  lprint_rstartpagefunc_t rstartpage;	// Start raster page function
  lprint_rwritefunc_t	rwrite;		// Write raster line function
  lprint_statusfunc_t	status;		// Status function
  const char		*format;	// Printer-specific format
  int			num_resolution,	// Number of printer resolutions
			x_resolution[LPRINT_MAX_RESOLUTION],
			y_resolution[LPRINT_MAX_RESOLUTION];
					// Printer resolutions
  int			left_right,	// Left and right margins in hundredths of millimeters
			bottom_top;	// Bottom and top margins in hundredths of millimeters
  int			num_media;	// Number of supported media
  const char		*media[LPRINT_MAX_MEDIA];
					// Supported media
  lprint_media_col_t	media_default,	// Default media
			media_ready[LPRINT_MAX_SOURCE];
					// Ready media
  int			num_source;	// Number of media sources (rolls)
  const char		*source[LPRINT_MAX_SOURCE];
					// Media sources
  int			top_offset_supported[2];
					// media-top-offset-supported (0,0 for none)
  lprint_media_tracking_t tracking_supported;
					// media-tracking-supported
  int			num_type;	// Number of media types
  const char		*type[LPRINT_MAX_TYPE];
					// Media types
  lprint_label_mode_t	mode_configured,// label-mode-configured
			mode_supported;	// label-mode-supported
  int			tear_offset_configured,
					// label-tear-offset-configured
			tear_offset_supported[2];
					// label-tear-offset-supported (0,0 for none)
  int			speed_supported[2],// print-speed-supported (0,0 for none)
			speed_default;	// print-speed-default
  int			darkness_configured,
					// printer-darkness-configured
			darkness_supported;
					// printer-darkness-supported (0 for none)
  int			num_supply;	// Number of printer-supply
  lprint_supply_t	supply[LPRINT_MAX_SUPPLY];
					// printer-supply
} lprint_driver_t;


//
// Functions...
//

extern lprint_driver_t	*lprintCreateDriver(const char *driver_name);
extern ipp_t		*lprintCreateMediaCol(lprint_media_col_t *media, int db);
extern void		lprintDeleteDriver(lprint_driver_t *driver);
extern const char * const *lprintGetDrivers(int *num_drivers);
extern const char	*lprintGetMakeAndModel(const char *driver_name);
extern void		lprintImportMediaCol(ipp_t *col, lprint_media_col_t *media);

extern void		lprintInitDYMO(lprint_driver_t *driver);
extern void		lprintInitPWG(lprint_driver_t *driver);
extern void		lprintInitZPL(lprint_driver_t *driver);

extern const char	*lprintLabelModeString(lprint_label_mode_t v);
extern lprint_label_mode_t lprintLabelModeValue(const char *s);
extern const char	*lprintMediaTrackingString(lprint_media_tracking_t v);
extern lprint_media_tracking_t lprintMediaTrackingValue(const char *s);

// Future planned drivers that are not yet implemented
//extern void		lprintInitCPCL(lprint_driver_t *driver);
//extern void		lprintInitEPL1(lprint_driver_t *driver);
//extern void		lprintInitEPL2(lprint_driver_t *driver);
//extern void		lprintInitFGL(lprint_driver_t *driver);
//extern void		lprintInitPCL(lprint_driver_t *driver);

#endif // !_DRIVER_COMMON_H_
