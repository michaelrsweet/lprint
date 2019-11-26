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
// Local globals...
//

// Note: lprint_drivers and lprint_models need to be kept in sync
static const char * const lprint_drivers[] =
{					// Driver keywords
  "dymo_lm-400",
  "dymo_lm-450",
  "dymo_lm-pc",
  "dymo_lm-pc-ii",
  "dymo_lm-pnp",
  "dymo_lp-350",
  "dymo_lw-300",
  "dymo_lw-310",
  "dymo_lw-315",
  "dymo_lw-320",
  "dymo_lw-330-turbo",
  "dymo_lw-330",
  "dymo_lw-400-turbo",
  "dymo_lw-400",
  "dymo_lw-450-duo-label",
  "dymo_lw-450-duo-tape",
  "dymo_lw-450-turbo",
  "dymo_lw-450-twin-turbo",
  "dymo_lw-450",
  "dymo_lw-4xl",
  "dymo_lw-duo-label",
  "dymo_lw-duo-tape",
  "dymo_lw-duo-tape-128",
  "dymo_lw-se450",
  "dymo_lw-wireless",
};

static const char * const lprint_models[] =
{					// Make and model strings
  "Dymo LabelMANAGER 400",
  "Dymo LabelMANAGER 450",
  "Dymo LabelMANAGER PC",
  "Dymo LabelMANAGER PC II",
  "Dymo LabelMANAGER PNP",
  "Dymo LabelPOINT 350",
  "Dymo LabelWriter 300",
  "Dymo LabelWriter 310",
  "Dymo LabelWriter 315",
  "Dymo LabelWriter 320",
  "Dymo LabelWriter 330 Turbo",
  "Dymo LabelWriter 330",
  "Dymo LabelWriter 400 Turbo",
  "Dymo LabelWriter 400",
  "Dymo LabelWriter 450 DUO Label",
  "Dymo LabelWriter 450 DUO Tape",
  "Dymo LabelWriter 450 Turbo",
  "Dymo LabelWriter 450 Twin Turbo",
  "Dymo LabelWriter 450",
  "Dymo LabelWriter 4XL",
  "Dymo LabelWriter DUO Label",
  "Dymo LabelWriter DUO Tape",
  "Dymo LabelWriter DUO Tape 128",
  "Dymo LabelWriter SE450",
  "Dymo LabelWriter Wireless",
};


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


//
// 'lprintGetDrivers()' - Get the list of supported drivers.
//

const char * const *			// O - Driver keywords
lprintGetDrivers(int *num_drivers)	// O - Number of drivers
{
  *num_drivers = (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0]));

  return (lprint_drivers);
}


//
// 'lprintGetMakeAndModel()' - Get the make and model string for a driver.
//

const char *				// O - Make and model string
lprintGetMakeAndModel(
    lprint_driver_t *driver)		// I - Driver
{
  int	i;				// Looping var


  for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
  {
    if (!strcmp(lprint_drivers[i], driver->name))
      return (lprint_models[i]);
  }

  return ("Unknown");
}
