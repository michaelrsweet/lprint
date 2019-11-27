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
#include <assert.h>


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
  int			i, j;		// Looping vars
  const char		*name;		// Driver name
  lprint_driver_t	*driver = NULL;	// New driver
  ipp_attribute_t	*attr;		// Printer attribute


  pthread_rwlock_wrlock(&printer->rwlock);

  if ((name = ippGetString(ippFindAttribute(printer->attrs, "lprint-driver", IPP_TAG_KEYWORD), 0, NULL)) != NULL)
  {
    for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
    {
      if (!strcmp(name, lprint_drivers[i]))
      {
	if ((driver = calloc(1, sizeof(lprint_driver_t))) != NULL)
	{
	  // Initialize the driver structure...
	  pthread_rwlock_init(&driver->rwlock, NULL);

	  driver->name = strdup(name);

          if (!strncmp(name, "cpcl_", 5))
            lprintInitCPCL(driver);
          else if (!strncmp(name, "dymo_", 5))
            lprintInitDYMO(driver);
          else if (!strncmp(name, "epl1_", 5))
            lprintInitEPL1(driver);
          else if (!strncmp(name, "epl2_", 5))
            lprintInitEPL2(driver);
          else if (!strncmp(name, "fgl_", 4))
            lprintInitFGL(driver);
          else if (!strncmp(name, "pcl_", 4))
            lprintInitPCL(driver);
          else
            lprintInitZPL(driver);

          // Assign to printer and copy capabilities...
          printer->driver = driver;

          // printer-make-and-model
          if ((attr = ippFindAttribute(printer->attrs, "printer-make-and-model", IPP_TAG_TEXT)) != NULL)
            ippSetString(printer->attrs, &attr, 0, lprint_models[i]);
          else
            ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, lprint_models[i]);

          // printer-resolution-supported, -default
          if ((attr = ippFindAttribute(printer->attrs, "printer-resolution-default", IPP_TAG_RESOLUTION)) != NULL)
            ippDeleteAttribute(printer->attrs, attr);
          if ((attr = ippFindAttribute(printer->attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
            ippDeleteAttribute(printer->attrs, attr);

          if (driver->num_resolution > 0)
          {
	    ippAddResolution(printer->attrs, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, driver->x_resolution[driver->num_resolution - 1], driver->y_resolution[driver->num_resolution - 1]);
	    ippAddResolutions(printer->attrs, IPP_TAG_PRINTER, "printer-resolution-supported", driver->num_resolution, IPP_RES_PER_INCH, driver->x_resolution, driver->y_resolution);
          }

          // pwg-raster-document-resolution-supported
          if ((attr = ippFindAttribute(printer->attrs, "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
            ippDeleteAttribute(printer->attrs, attr);

          if (driver->num_resolution > 0)
	    ippAddResolutions(printer->attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", driver->num_resolution, IPP_RES_PER_INCH, driver->x_resolution, driver->y_resolution);

          // urf-supported
          if ((attr = ippFindAttribute(printer->attrs, "urf-supported", IPP_TAG_KEYWORD)) != NULL)
            ippDeleteAttribute(printer->attrs, attr);

          if (driver->num_resolution > 0)
          {
            const char	*values[3];		// urf-supported values
            char	rs[32];			// RS value

            if (driver->num_resolution == 1)
              snprintf(rs, sizeof(rs), "RS%d", driver->x_resolution[0]);
            else
              snprintf(rs, sizeof(rs), "RS%d-%d", driver->x_resolution[driver->num_resolution - 2], driver->x_resolution[driver->num_resolution - 1]);

            values[0] = "V1.4";
            values[1] = "W8";
            values[2] = rs;

            ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", 3, NULL, values);
          }
	}

	break;
      }
    }
  }

  pthread_rwlock_unlock(&printer->rwlock);

  return (driver);
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
  assert(sizeof(lprint_drivers) == sizeof(lprint_models));

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


  if (driver && driver->name)
  {
    for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
    {
      if (!strcmp(lprint_drivers[i], driver->name))
	return (lprint_models[i]);
    }
  }

  return ("Unknown");
}
