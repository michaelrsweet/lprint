//
// Common driver code for LPrint, a Label Printer Utility
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
  "pwg_2inch",
  "pwg_4inch",
  "zpl_2inch-203dpi",
  "zpl_2inch-300dpi",
  "zpl_4inch-203dpi",
  "zpl_4inch-300dpi"
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
  "PWG Raster 2 inch Test Printer",
  "PWG Raster 4 inch Test Printer",
  "Zebra ZPL 2 inch/203dpi Printer",
  "Zebra ZPL 2 inch/300dpi Printer",
  "Zebra ZPL 4 inch/203dpi Printer",
  "Zebra ZPL 4 inch/300dpi Printer"
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
	driver->name   = strdup(driver_name);
	driver->attrs  = ippNew();
	driver->format = "application/octet-stream";

	if (!strncmp(driver_name, "dymo_", 5))
	  lprintInitDYMO(driver);
	else if (!strncmp(driver_name, "pwg_", 4))
	  lprintInitPWG(driver);
	else
	  lprintInitZPL(driver);

	// media-xxx
	lprint_copy_media(driver);

	// printer-make-and-model
	ippAddString(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-make-and-model", NULL, lprint_models[i]);

	// printer-resolution-supported, -default
	if (driver->num_resolution > 0)
	  ippAddResolutions(driver->attrs, IPP_TAG_PRINTER, "printer-resolution-supported", driver->num_resolution, IPP_RES_PER_INCH, driver->x_resolution, driver->y_resolution);

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
    lprint_media_col_t *media,		// I - Media values
    int                db)		// I - 1 for media-col-database, 0 otherwise
{
  ipp_t		*col = ippNew(),	// Collection value
		*size = lprint_create_media_size(media->size_name);
					// media-size value


  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", media->bottom_margin);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", media->left_margin);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", media->right_margin);
  ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
  ippDelete(size);
  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-size-name", NULL, media->size_name);
  if (media->source[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-source", NULL, media->source);
  ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", media->top_margin);
  if (!db)
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-offset", media->top_offset);
  if (media->tracking)
    ippAddString(col, IPP_TAG_ZERO, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking", NULL, lprintMediaTrackingString(media->tracking));
  if (media->type[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_KEYWORD, "media-type", NULL, media->type);

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
    const char *driver_name)		// I - Driver name
{
  int	i;				// Looping var


  if (driver_name)
  {
    for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
    {
      if (!strcmp(lprint_drivers[i], driver_name))
	return (lprint_models[i]);
    }
  }

  return (NULL);
}


//
// 'lprintLabelModeString()' - Return the string associated with a label mode bit value.
//

const char *				// O - String
lprintLabelModeString(
    lprint_label_mode_t v)		// I - Bit value
{
  switch (v)
  {
    case LPRINT_LABEL_MODE_APPLICATOR :
        return ("applicator");

    case LPRINT_LABEL_MODE_CUTTER :
        return ("cutter");

    case LPRINT_LABEL_MODE_CUTTER_DELAYED :
        return ("cutter-delayed");

    case LPRINT_LABEL_MODE_KIOSK :
        return ("kiosk");

    case LPRINT_LABEL_MODE_PEEL_OFF :
        return ("peel-off");

    case LPRINT_LABEL_MODE_PEEL_OFF_PREPEEL :
        return ("peel-off-prepeel");

    case LPRINT_LABEL_MODE_REWIND :
        return ("rewind");

    case LPRINT_LABEL_MODE_RFID :
        return ("rfid");

    case LPRINT_LABEL_MODE_TEAR_OFF :
        return ("tear-off");

    default :
        return ("unknown");
  }
}


//
// 'lprintLabelModeValue()' - Return the bit value associated with a string.
//

lprint_label_mode_t			// O - Bit value
lprintLabelModeValue(const char *s)	// I - String
{
  if (!strcmp(s, "applicator"))
    return (LPRINT_LABEL_MODE_APPLICATOR);
  else if (!strcmp(s, "cutter"))
    return (LPRINT_LABEL_MODE_CUTTER);
  else if (!strcmp(s, "cutter-delayed"))
    return (LPRINT_LABEL_MODE_CUTTER_DELAYED);
  else if (!strcmp(s, "kiosk"))
    return (LPRINT_LABEL_MODE_KIOSK);
  else if (!strcmp(s, "peel-off"))
    return (LPRINT_LABEL_MODE_PEEL_OFF);
  else if (!strcmp(s, "peel-off-prepeel"))
    return (LPRINT_LABEL_MODE_PEEL_OFF_PREPEEL);
  else if (!strcmp(s, "rewind"))
    return (LPRINT_LABEL_MODE_REWIND);
  else if (!strcmp(s, "rfid"))
    return (LPRINT_LABEL_MODE_RFID);
  else if (!strcmp(s, "tear-off"))
    return (LPRINT_LABEL_MODE_TEAR_OFF);
  else
    return (0);
}


//
// 'lprintMediaTrackingString()' - Return the string associated with a bit value.
//

const char *				// O - String
lprintMediaTrackingString(
    lprint_media_tracking_t v)		// I - Bit value
{
  if (v == LPRINT_MEDIA_TRACKING_CONTINUOUS)
    return ("continuous");
  else if (v == LPRINT_MEDIA_TRACKING_MARK)
    return ("mark");
  else if (v == LPRINT_MEDIA_TRACKING_WEB)
    return ("web");
  else
    return ("unknown");
}


//
// 'lprintMediaTrackingValue()' - Return the bit value associated with a string.
//

lprint_media_tracking_t			// O - Bit value
lprintMediaTrackingValue(const char *s)	// I - String
{
  if (!strcmp(s, "continuous"))
    return (LPRINT_MEDIA_TRACKING_CONTINUOUS);
  else if (!strcmp(s, "mark"))
    return (LPRINT_MEDIA_TRACKING_MARK);
  else if (!strcmp(s, "web"))
    return (LPRINT_MEDIA_TRACKING_WEB);
  else
    return (0);
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
  lprint_media_col_t	media;		// Media values
  const char		*max_name = NULL,// Maximum size
		    	*min_name = NULL;// Minimum size


  // media-bottom-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", driver->bottom_top);

  // media-col-database
  for (i = 0, count = 0; i < driver->num_media; i ++)
  {
    if (!strncmp(driver->media[i], "roll_max_", 9))
      max_name = driver->media[i];
    else if (!strncmp(driver->media[i], "roll_min_", 9))
      min_name = driver->media[i];
    else
      count ++;
  }

  if (max_name && min_name)
    count ++;

  attr = ippAddCollections(driver->attrs, IPP_TAG_PRINTER, "media-col-database", count, NULL);

  memset(&media, 0, sizeof(media));
  media.bottom_margin = driver->bottom_top;
  media.left_margin   = driver->left_right;
  media.right_margin  = driver->left_right;
  media.top_margin    = driver->bottom_top;

  for (i = 0, count = 0; i < driver->num_media; i ++)
  {
    // Skip custom size ranges...
    if (driver->media[i] == max_name || driver->media[i] == min_name)
      continue;

    // Add the fixed size...
    strlcpy(media.size_name, driver->media[i], sizeof(media.size_name));
    col = lprintCreateMediaCol(&media, 1);
    ippSetCollection(driver->attrs, &attr, count, col);
    ippDelete(col);
    count ++;
  }

  if (max_name && min_name)
  {
    ipp_t	*size = ippNew();
    pwg_media_t	*max_pwg = pwgMediaForPWG(max_name),
					// Maximum size
		*min_pwg = pwgMediaForPWG(min_name);
					// Minimum size

    ippAddRange(size, IPP_TAG_ZERO, "x-dimension", min_pwg->width, max_pwg->width);
    ippAddRange(size, IPP_TAG_ZERO, "y-dimension", min_pwg->length, max_pwg->length);

    col = ippNew();
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-bottom-margin", media.bottom_margin);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-left-margin", media.left_margin);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-right-margin", media.right_margin);
    ippAddCollection(col, IPP_TAG_ZERO, "media-size", size);
    ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "media-top-margin", media.top_margin);
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
  {
    if (driver->media[i] != max_name && driver->media[i] != min_name)
      count ++;
  }

  if (max_name && min_name)
    count ++;

  attr = ippAddCollections(driver->attrs, IPP_TAG_PRINTER, "media-size-supported", count, NULL);

  for (i = 0, count = 0; i < driver->num_media; i ++)
  {
    if (driver->media[i] == max_name || driver->media[i] == min_name)
      continue;

    // Add the fixed size...
    col = lprint_create_media_size(driver->media[i]);
    ippSetCollection(driver->attrs, &attr, count, col);
    ippDelete(col);
    count ++;
  }

  if (max_name && min_name)
  {
    ipp_t	*size = ippNew();
    pwg_media_t	*max_pwg = pwgMediaForPWG(max_name),
					// Maximum size
		*min_pwg = pwgMediaForPWG(min_name);
					// Minimum size

    ippAddRange(size, IPP_TAG_ZERO, "x-dimension", min_pwg->width, max_pwg->width);
    ippAddRange(size, IPP_TAG_ZERO, "y-dimension", min_pwg->length, max_pwg->length);

    ippSetCollection(driver->attrs, &attr, count, size);
    ippDelete(size);
  }

  // media-source-supported
  if (driver->num_source)
    ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", driver->num_source, NULL, driver->source);

  // media-supported
  ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-supported", driver->num_media, NULL, driver->media);

  // media-top-margin-supported
  ippAddInteger(driver->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", driver->bottom_top);

  // media-top-offset-supported
  if (driver->top_offset_supported[0] || driver->top_offset_supported[1])
    ippAddRange(driver->attrs, IPP_TAG_PRINTER, "media-top-offset-supported", driver->top_offset_supported[0], driver->top_offset_supported[1]);

  // media-tracking-supported
  if (driver->tracking_supported)
  {
    const char	*values[3];		// media-tracking values

    for (count = 0, i = LPRINT_MEDIA_TRACKING_CONTINUOUS; i <= LPRINT_MEDIA_TRACKING_WEB; i *= 2)
    {
      if (driver->tracking_supported & i)
        values[count++] = lprintMediaTrackingString(i);
    }

    ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking-supported", count, NULL, values);
  }

  // media-type-supported
  if (driver->num_type)
    ippAddStrings(driver->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", driver->num_type, NULL, driver->type);
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
