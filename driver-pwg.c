//
// PWG Raster test driver for LPrint, a Label Printer Utility
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// Local globals...
//

static const char * const lprint_pwg_2inch_media[] =
{					// Supported media sizes
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",

  "roll_max_2x3600in",
  "roll_min_0.25x0.25in"
};

static const char * const lprint_pwg_4inch_media[] =
{					// Supported media sizes
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",
  "oe_xl-shipping-label_4x6in",

  "roll_max_4x3600in",
  "roll_min_0.25x0.25in"
};


//
// Local functions...
//

static int	lprint_pwg_print(lprint_printer_t *printer, lprint_job_t *job, lprint_options_t *options);
static int	lprint_pwg_rend(lprint_printer_t *printer, lprint_job_t *job, lprint_options_t *options, void *user_data, unsigned page);
static int	lprint_pwg_rstart(lprint_printer_t *printer, lprint_job_t *job, lprint_options_t *options, void *user_data, unsigned page);
static int	lprint_pwg_rwrite(lprint_printer_t *printer, lprint_job_t *job, lprint_options_t *options, void *user_data, unsigned y, const unsigned char *line);
static int	lprint_pwg_status(lprint_printer_t *printer);


//
// 'lprintInitPWG()' - Initialize the driver.
//

void
lprintInitPWG(
    lprint_driver_t *driver)		// I - Driver
{
  pthread_rwlock_wrlock(&driver->rwlock);

  driver->printfunc  = lprint_pwg_print;
  driver->rendfunc   = lprint_pwg_rend;
  driver->rstartfunc = lprint_pwg_rstart;
  driver->rwritefunc = lprint_pwg_rwrite;
  driver->statusfunc = lprint_pwg_status;
  driver->format     = "image/pwg-raster";

  driver->num_resolution  = 2;
  driver->x_resolution[0] = 203;
  driver->y_resolution[0] = 203;
  driver->x_resolution[1] = 300;
  driver->y_resolution[1] = 300;

  if (!strcmp(driver->name, "pwg_2inch"))
  {
    driver->num_media = (int)(sizeof(lprint_pwg_2inch_media) / sizeof(lprint_pwg_2inch_media[0]));
    memcpy(driver->media, lprint_pwg_2inch_media, sizeof(lprint_pwg_2inch_media));
  }
  else
  {
    driver->num_media = (int)(sizeof(lprint_pwg_4inch_media) / sizeof(lprint_pwg_4inch_media[0]));
    memcpy(driver->media, lprint_pwg_4inch_media, sizeof(lprint_pwg_4inch_media));
  }

  driver->num_source = 2;
  driver->source[0]  = "main-roll";
  driver->source[1]  = "alternate-roll";

  driver->num_type = 2;
  driver->type[0]  = "continuous";
  driver->type[1]  = "labels";

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_pwg_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_print(
    lprint_printer_t *printer,		// I - Printer
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  (void)printer;
  (void)job;
  (void)options;

  return (1);
}


//
// 'lprint_pwg_rend()' - End a page/job.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rend(
    lprint_printer_t *printer,		// I - Printer
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    void             *user_data,	// I - User data
    unsigned         page)		// I - Page number
{
  (void)printer;
  (void)job;
  (void)options;
  (void)user_data;
  (void)page;

  return (1);
}


//
// 'lprint_pwg_rstart()' - Start a page/job.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rstart(
    lprint_printer_t *printer,		// I - Printer
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    void             *user_data,	// I - User data
    unsigned         page)		// I - Page number
{
  (void)printer;
  (void)job;
  (void)options;
  (void)user_data;
  (void)page;

  return (1);
}


//
// 'lprint_pwg_rwrite()' - Write a raster line.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rwrite(
    lprint_printer_t    *printer,	// I - Printer
    lprint_job_t        *job,		// I - Job
    lprint_options_t    *options,	// I - Job options
    void                *user_data,	// I - User data
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  (void)printer;
  (void)job;
  (void)options;
  (void)user_data;
  (void)y;
  (void)line;

  return (1);
}


//
// 'lprint_pwg_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
