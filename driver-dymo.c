//
// Dymo driver for LPrint, a Label Printer Utility
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

static const char * const lprint_dymo_media[] =
{					// Supported media sizes
  "oe_thin-multipurpose-label_0.375x2.8125in",
  "oe_library-barcode-label_0.5x1.875in",
  "oe_hanging-file-tab-insert_0.5625x2in",
  "oe_file-folder-label_0.5625x3.4375in",
  "oe_return-address-label_0.75x2in",
  "oe_barcode-label_0.75x2.5in",
  "oe_video-spine-label_0.75x5.875in",
  "oe_price-tag-label_0.9375x0.875in",
  "oe_square-multipurpose-label_1x1in",
  "oe_book-spine-label_1x1.5in",
  "oe_sm-multipurpose-label_1x2.125in",
  "oe_2-up-file-folder-label_1.125x3.4375in",
  "oe_internet-postage-label_1.25x1.625in",
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_video-top-label_1.8x3.1in",
  "oe_multipurpose-label_2x2.3125in",
  "oe_md-appointment-card_2x3.5in",
  "oe_lg-multipurpose-label_2.125x.75in",
  "oe_shipping-label_2.125x4in",
  "oe_continuous-label_2.125x3600in",
  "oe_md-multipurpose-label_2.25x1.25in",
  "oe_media-label_2.25x2.25in",
  "oe_2-up-address-label_2.25x3.5in",
  "oe_name-badge-label_2.25x4in",
  "oe_3-part-postage-label_2.25x7in",
  "oe_2-part-internet-postage-label_2.25x7.5in",
  "oe_shipping-label_2.3125x4in",
  "oe_internet-postage-label_2.3125x7in",
  "oe_internet-postage-confirmation-label_2.3125x10.5in",
  "oe_name-badge-label_2.4375x4.3125in",
  "oe_hc-address-label_3.5x1.125in",
  "oe_hc-shipping-label_4x2.3.125in",
  "oe_xl-shipping-label_4x6in",

  "roll_max_2.3125x3600in",
  "roll_min_0.25x0.25in"
};


//
// Local functions...
//

static int	lprint_dymo_print(lprint_printer_t *printer, lprint_job_t *job);
static int	lprint_dymo_status(lprint_printer_t *printer);


//
// 'lprintInitDymo()' - Initialize the driver.
//

void
lprintInitDYMO(
    lprint_driver_t *driver)		// I - Driver
{
  pthread_rwlock_wrlock(&driver->rwlock);

  driver->printfunc  = lprint_dymo_print;
  driver->statusfunc = lprint_dymo_status;
  driver->format     = "application/vnd.dymo-lw";

  driver->num_resolution  = 1;
  driver->x_resolution[0] = 203;
  driver->y_resolution[0] = 203;

  driver->num_media = (int)(sizeof(lprint_dymo_media) / sizeof(lprint_dymo_media[0]));
  memcpy(driver->media, lprint_dymo_media, sizeof(lprint_dymo_media));

  driver->num_source = 1;
  driver->source[0]  = "main-roll";

  driver->num_type = 1;
  driver->type[0]  = "labels";

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_dymo_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_print(
    lprint_printer_t *printer,		// I - Printer
    lprint_job_t     *job)		// I - Job
{
  (void)printer;
  (void)job;

  return (1);
}


//
// 'lprint_dymo_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
