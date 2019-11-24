//
// Common driver code for LPrint, a Label Printer Utility
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
// 'lprintCreateDriver()' - Create a driver for a printer.
//

lprint_driver_t	*			// O - New driver structure
lprintCreateDriver(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (NULL);
}


//
// 'lprintDeleteDriver()' - Delete a driver for a printer.
//

void
lprintDeleteDriver(
    lprint_driver_t *driver)		// I - Driver
{
  if (driver)
  {
    lprintCloseDevice(driver->device);
    free(driver);
  }
}
