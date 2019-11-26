//
// EPL2 driver for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
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

static const char * const lprint_epl2_media[] =
{					// Supported media sizes
  "oe_1.25x0.25-label_1.25x0.25in",
  "oe_1.25x2.25-label_1.25x2.25in",

  "oe_1.5x0.25-label_1.5x0.25in",
  "oe_1.5x0.5-label_1.5x0.5in",
  "oe_1.5x1-label_1.5x1in",
  "oe_1.5x2-label_1.5x2in",

  "oe_2x0.37-label_2x0.37in",
  "oe_2x0.5-label_2x0.5in",
  "oe_2x1-label_2x1in",
  "oe_2x1.25-label_2x1.25in",
  "oe_2x2-label_2x2in",
  "oe_2x3-label_2x3in",
  "oe_2x4-label_2x4in",
  "oe_2x5.5-label_2x5.5in",

  "oe_2.25x0.5-label_2.25xin",
  "oe_2.25x1.25-label_2.25xin",
  "oe_30859-paint-can-label_2.25x3.125in",
  "oe_2.25x4-label_2.25x4in",
  "oe_2.25x5.5-label_2.25x5.5in",

  "oe_2.38x5.5-label_2.38x5.5in",

  "oe_2.5x1-label_2.5x1in",
  "oe_2.5x2-label_2.5x2in",

  "oe_2.75x1.25-label_2.75x1.25in",

  "oe_2.9x1-label_2.9x1in",

  "oe_3x1-label_3x1in",
  "oe_3x1.25-label_3x1.25in",
  "oe_3x2-label_3x2in",
  "oe_3x3-label_3x3in",
  "oe_3x5-label_3x5in",

  "oe_3.25x2-label_3.25x2in",
  "oe_3.25x5-label_3.25x5in",
  "oe_3.25x5.5-label_3.25x5.5in",
  "oe_3.25x5.83-label_3.25x5.83in",
  "oe_3.25x7.83-label_3.25x7.83in",

  "oe_3.5x1-label_3.5x1in",

  "oe_4x1-label_4x1in",
  "oe_4x2-label_4x2in",
  "oe_4x3-label_4x3in",
  "oe_4x4-label_4x4in",
  "oe_4x5-label_4x5in",
  "oe_4x6-label_4x6in",
  "oe_4x6.5-label_4x6.5in",
  "oe_4x13-label_4x13in",

  "oe_6x1-label_6x1in",
  "oe_6x2-label_6x2in",
  "oe_6x3-label_6x3in",
  "oe_6x4-label_6x4in",
  "oe_6x5-label_6x5in",
  "oe_6x6-label_6x6in",
  "oe_6x6.5-label_6x6.5in",
  "oe_6x13-label_6x13in",

  "oe_8x1-label_8x1in",
  "oe_8x2-label_8x2in",
  "oe_8x3-label_8x3in",
  "oe_8x4-label_8x4in",
  "oe_8x5-label_8x5in",
  "oe_8x6-label_8x6in",
  "oe_8x6.5-label_8x6.5in",
  "oe_8x13-label_8x13in",

  "roll_max_8x100in",
  "roll_min_1.25x0.25in"
};


//
// Local functions...
//

static int	lprint_epl2_print(lprint_printer_t *printer, lprint_job_t *job);
static int	lprint_epl2_status(lprint_printer_t *printer);


//
// 'lprintInitEPL2()' - Initialize the driver.
//

void
lprintInitEPL2(
    lprint_driver_t *driver)		// I - Driver
{
  pthread_rwlock_wrlock(&driver->rwlock);

  driver->printfunc  = lprint_epl2_print;
  driver->statusfunc = lprint_epl2_status;
  driver->format     = "application/vnd.-";

  driver->num_resolution  = 1;
  driver->x_resolution[0] = 203;
  driver->y_resolution[0] = 203;

  driver->num_media = (int)(sizeof(lprint_epl2_media) / sizeof(lprint_epl2_media[0]));
  memcpy(driver->media, lprint_epl2_media, sizeof(lprint_epl2_media));

  driver->num_ready = 0;

  driver->num_source = 1;
  driver->source[0]  = "main-roll";

  driver->num_type = 1;
  driver->type[0]  = "labels";

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_epl2_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_epl2_print(
    lprint_printer_t *printer,		// I - Printer
    lprint_job_t     *job)		// I - Job
{
  (void)printer;
  (void)job;

  return (1);
}


//
// 'lprint_epl2_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_epl2_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
