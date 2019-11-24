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
#  define LPRINT_MAX_SOURCE	4	// Maximum number of sources/rolls
#  define LPRINT_MAX_SUPPLY	4	// Maximum number of supplies


//
// Types...
//

typedef struct lprint_printer_s lprint_printer_t;
					// Forward defined printer
typedef struct lprint_job_s lprint_job_t;
					// Forward defined job

typedef int (*lprint_initfunc_t)(lprint_printer_t *printer);
					// Initialize printer attributes
typedef int (*lprint_printfunc_t)(lprint_printer_t *printer, lprint_job_t *job);
					// Print a job
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
  lprint_device_t	*device;	// Connection to device
  lprint_initfunc_t	initfunc;	// Initialization function
  lprint_printfunc_t	printfunc;	// Print function
  lprint_statusfunc_t	statusfunc;	// Status function
  int			num_media;	// Number of supported media
  const char		*media[LPRINT_MAX_MEDIA];
					// Supported media
  int			num_ready;	// Number of ready media
  const char		*ready[LPRINT_MAX_SOURCE];
					// Ready media
  int			num_sources;	// Number of media sources (rolls)
  const char		*sources[LPRINT_MAX_SOURCE];
					// Media sources
  int			num_supply;	// Number of supplies
  lprint_supply_t	supply[LPRINT_MAX_SUPPLY];
					// Supplies
} lprint_driver_t;


//
// Functions...
//

extern lprint_driver_t	*lprintCreateDriver(lprint_printer_t *printer);
extern void		lprintDeleteDriver(lprint_driver_t *driver);

#endif // !_DRIVER_COMMON_H_
