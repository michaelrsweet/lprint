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
// Local functions...
//

static void	lprint_copy_media(lprint_driver_t *driver);
static ipp_t	*lprint_create_media_size(const char *size_name);


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
    const char *driver_name)		// I - Driver name
{
  int			i;		// Looping vars
  lprint_driver_t	*driver = NULL;	// New driver


  for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
  {
    if (!strcmp(driver_name, lprint_drivers[i]))
    {
      if ((driver = calloc(1, sizeof(lprint_driver_t))) != NULL)
      {
	// Initialize the driver structure...
	driver->name  = strdup(driver_name);
	driver->attrs = ippNew();

	if (!strncmp(driver_name, "cpcl_", 5))
	  lprintInitCPCL(driver);
	else if (!strncmp(driver_name, "dymo_", 5))
	  lprintInitDYMO(driver);
	else if (!strncmp(driver_name, "epl1_", 5))
	  lprintInitEPL1(driver);
	else if (!strncmp(driver_name, "epl2_", 5))
	  lprintInitEPL2(driver);
	else if (!strncmp(driver_name, "fgl_", 4))
	  lprintInitFGL(driver);
	else if (!strncmp(driver_name, "pcl_", 4))
	  lprintInitPCL(driver);
	else
	  lprintInitZPL(driver);

	// media-xxx
	lprint_copy_media(driver);

	// printer-make-and-model
	ippAddString(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, lprint_models[i]);

	// printer-resolution-supported, -default
	if (driver->num_resolution > 0)
	{
	  ippAddResolution(driver->attrs, IPP_TAG_PRINTER, "printer-resolution-default", IPP_RES_PER_INCH, driver->x_resolution[driver->num_resolution - 1], driver->y_resolution[driver->num_resolution - 1]);
	  ippAddResolutions(driver->attrs, IPP_TAG_PRINTER, "printer-resolution-supported", driver->num_resolution, IPP_RES_PER_INCH, driver->x_resolution, driver->y_resolution);
	}

	// pwg-raster-document-resolution-supported
	if (driver->num_resolution > 0)
	  ippAddResolutions(driver->attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", driver->num_resolution, IPP_RES_PER_INCH, driver->x_resolution, driver->y_resolution);

	// urf-supported
	if (driver->num_resolution > 0)
	{
	  const char	*values[3];		// urf-supported values
	  char		rs[32];			// RS value

	  if (driver->num_resolution == 1)
	    snprintf(rs, sizeof(rs), "RS%d", driver->x_resolution[0]);
	  else
	    snprintf(rs, sizeof(rs), "RS%d-%d", driver->x_resolution[driver->num_resolution - 2], driver->x_resolution[driver->num_resolution - 1]);

	  values[0] = "V1.4";
	  values[1] = "W8";
	  values[2] = rs;

	  ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", 3, NULL, values);
	}

	break;
      }
    }
  }

  return (driver);
}


//
// 'lprintCreateMediaCol()' - Create a media-col collection.
//

ipp_t *					// O - Collection value
lprintCreateMediaCol(
    const char *size_name,		// I - Media size name
    const char *source,			// I - Media source, if any
    const char *type,			// I - Media type, if any
    int        left_right,		// I - Left and right margins
    int        bottom_top)		// I - Bottom and top margins
{
  ipp_t		*col = ippNew(),	// Collection value
		*size = lprint_create_media_size(size_name);
					// media-size value


  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-size-name", NULL, size_name);
  ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
  ippDelete(size);

  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", bottom_top);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", left_right);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", left_right);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", bottom_top);

  if (source)
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-source", NULL, source);

  if (type)
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-type", NULL, type);

  return (col);
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


//
// 'lprint_copy_media()' - Copy media capability attributes.
//

static void
lprint_copy_media(
    lprint_driver_t  *driver)		// I - Driver
{
  int			i,		// Looping var
			count;		// Number of values
  ipp_attribute_t	*attr;		// Current attribute
  ipp_t			*col;		// Media collection value


  // media-bottom-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", driver->bottom_top);

  // media-col-database
  for (i = 0, count = 0; i < driver->num_media; i ++)
    if (strncmp(driver->media[i], "roll_", 5))
      count ++;

  if (driver->max_media[0] && driver->min_media[0])
    count ++;

  attr = ippAddCollections(driver->attrs, IPP_TAG_PRINTER, "media-col-database", count, NULL);

  for (i = 0, count = 0; i < driver->num_media; i ++)
  {
    if (!strncmp(driver->media[i], "roll_", 5))
      continue;

    // Add the fixed size...
    col = lprintCreateMediaCol(driver->media[i], NULL, NULL, driver->left_right, driver->bottom_top);
    ippSetCollection(driver->attrs, &attr, count, col);
    ippDelete(col);
    count ++;
  }

  if (driver->max_media[0] && driver->min_media[0])
  {
    ipp_t	*size = ippNew();
    pwg_media_t	*maxpwg = pwgMediaForPWG(driver->max_media),
					// Maximum size
		*minpwg = pwgMediaForPWG(driver->min_media);
					// Minimum size

    ippAddRange(size, IPP_TAG_ZERO, "x-dimension", minpwg->width, maxpwg->width);
    ippAddRange(size, IPP_TAG_ZERO, "y-dimension", minpwg->length, maxpwg->length);

    col = ippNew();
    ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
    ippDelete(size);

    ippSetCollection(driver->attrs, &attr, count, col);
    ippDelete(col);
  }

  // media-left-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", driver->left_right);

  // media-right-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", driver->left_right);

  // media-size-supported
  for (i = 0, count = 0; i < driver->num_media; i ++)
    if (strncmp(driver->media[i], "roll_", 5))
      count ++;

  if (driver->max_media[0] && driver->min_media[0])
    count ++;

  attr = ippAddCollections(driver->attrs, IPP_TAG_PRINTER, "media-size-supported", count, NULL);

  for (i = 0, count = 0; i < driver->num_media; i ++)
  {
    if (!strncmp(driver->media[i], "roll_", 5))
      continue;

    // Add the fixed size...
    col = lprint_create_media_size(driver->media[i]);
    ippSetCollection(driver->attrs, &attr, count, col);
    ippDelete(col);
    count ++;
  }

  if (driver->max_media[0] && driver->min_media[0])
  {
    ipp_t	*size = ippNew();
    pwg_media_t	*maxpwg = pwgMediaForPWG(driver->max_media),
					// Maximum size
		*minpwg = pwgMediaForPWG(driver->min_media);
					// Minimum size

    ippAddRange(size, IPP_TAG_ZERO, "x-dimension", minpwg->width, maxpwg->width);
    ippAddRange(size, IPP_TAG_ZERO, "y-dimension", minpwg->length, maxpwg->length);

    ippSetCollection(driver->attrs, &attr, count, size);
    ippDelete(size);
  }

  // media-source-supported
  if (driver->num_source)
    ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-source-supported", driver->num_source, NULL, driver->source);

  // media-supported
  ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-supported", driver->num_media, NULL, driver->media);

  // media-top-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", driver->bottom_top);

  // media-type-supported
  if (driver->num_type)
    ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-type-supported", driver->num_type, NULL, driver->type);
}


//
// 'lprint_create_media_size()' - Create a media-size collection.
//

static ipp_t *				// O - Collection value
lprint_create_media_size(
    const char *size_name)		// I - Media size name
{
  ipp_t		*col = ippNew();	// Collection value
  pwg_media_t	*pwg = pwgMediaForPWG(size_name);
					// Size information

  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", pwg->width);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", pwg->length);

  return (col);
}
