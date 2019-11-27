//
// Driver header file for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
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

typedef int (*lprint_printfunc_t)(lprint_printer_t *printer, lprint_job_t *job);
					// Print a job
typedef int (*lprint_rendfunc_t)(lprint_printer_t *printer, lprint_job_t *job, cups_page_header2_t *header, void *user_data, unsigned page);
					// End a raster job
typedef int (*lprint_rstartfunc_t)(lprint_printer_t *printer, lprint_job_t *job, cups_page_header2_t *header, void *user_data, unsigned page);
					// Start a raster job
typedef int (*lprint_rwritefunc_t)(lprint_printer_t *printer, lprint_job_t *job, cups_page_header2_t *header, void *user_data, unsigned y, const unsigned char *line);
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
  lprint_device_t	*device;	// Connection to device
  lprint_printfunc_t	printfunc;	// Print (file) function
  lprint_rendfunc_t	rendfunc;	// End raster job function
  lprint_rstartfunc_t	rstartfunc;	// Start raster job function
  lprint_rwritefunc_t	rwritefunc;	// Write raster line function
  lprint_statusfunc_t	statusfunc;	// Status function
  const char		*format;	// Printer-specific format
  int			num_resolution,	// Number of printer resolutions
			x_resolution[LPRINT_MAX_RESOLUTION],
			y_resolution[LPRINT_MAX_RESOLUTION];
					// Printer resolutions
  int			num_media;	// Number of supported media
  const char		*media[LPRINT_MAX_MEDIA];
					// Supported media
  char			custom_media[LPRINT_MAX_SOURCE][64];
					// Custom media sizes
  int			num_ready;	// Number of ready media
  const char		*ready[LPRINT_MAX_SOURCE];
					// Ready media
  int			num_source;	// Number of media sources (rolls)
  const char		*source[LPRINT_MAX_SOURCE];
					// Media sources
  int			num_type;	// Number of media types
  const char		*type[LPRINT_MAX_TYPE];
					// Media types
  int			num_supply;	// Number of supplies
  lprint_supply_t	supply[LPRINT_MAX_SUPPLY];
					// Supplies
} lprint_driver_t;


//
// Functions...
//

extern lprint_driver_t	*lprintCreateDriver(lprint_printer_t *printer);
extern void		lprintDeleteDriver(lprint_driver_t *driver);
extern const char * const *lprintGetDrivers(int *num_drivers);
extern const char	*lprintGetMakeAndModel(lprint_driver_t *driver);
extern void		lprintInitCPCL(lprint_driver_t *driver);
extern void		lprintInitDYMO(lprint_driver_t *driver);
extern void		lprintInitEPL1(lprint_driver_t *driver);
extern void		lprintInitEPL2(lprint_driver_t *driver);
extern void		lprintInitFGL(lprint_driver_t *driver);
extern void		lprintInitPCL(lprint_driver_t *driver);
extern void		lprintInitZPL(lprint_driver_t *driver);

#endif // !_DRIVER_COMMON_H_
