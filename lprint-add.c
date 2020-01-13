//
// Add printer sub-command for LPrint, a Label Printer Application
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
// Local functions...
//

static int	get_length(const char *value);


//
// 'lprintAddOptions()' - Add default/Job Template attributes from options.
//

void
lprintAddOptions(
    ipp_t         *request,		// I - IPP request
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  int		is_default;		// Adding xxx-default attributes?
  ipp_tag_t	group_tag;		// Group to add to
  const char	*value;			// String value
  int		intvalue;		// Integer value


  group_tag  = ippGetOperation(request) == IPP_PRINT_JOB ? IPP_TAG_JOB : IPP_TAG_PRINTER;
  is_default = group_tag == IPP_TAG_PRINTER;

  LPRINT_DEBUG("group_tag=%s, is_default=%d\n", ippTagString(group_tag), is_default);

  if (is_default)
  {
    // Add Printer Description attributes...
    if ((value = cupsGetOption("label-mode-configured", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "label-model-configured", NULL, value);

    if ((value = cupsGetOption("label-tear-offset-configured", num_options, options)) != NULL)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "label-tear-offset-configured", get_length(value));

    if ((value = cupsGetOption("media-default", num_options, options)) == NULL)
      value = cupsGetOption("media", num_options, options);
    if (value)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", NULL, value);

    if ((value = cupsGetOption("media-ready", num_options, options)) != NULL)
    {
      int		num_values;		// Number of values
      char	*values[4],		// Pointers to size strings
		  sizes[4][256];		// Size strings


      if ((num_values = sscanf(value, "%255s,%255s,%255s,%255s", sizes[0], sizes[1], sizes[2], sizes[3])) > 0)
      {
	values[0] = sizes[0];
	values[1] = sizes[1];
	values[2] = sizes[2];
	values[3] = sizes[3];

	ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", num_values, NULL, (const char * const *)values);
      }
    }

    if ((value = cupsGetOption("printer-darkness-configured", num_options, options)) != NULL)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-configured", atoi(value));

    if ((value = cupsGetOption("printer-geo-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, value);

    if ((value = cupsGetOption("printer-location", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, value);

    if ((value = cupsGetOption("printer-organization", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, value);

    if ((value = cupsGetOption("printer-organizational-unit", num_options, options)) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, value);
  }
  else
  {
    // Add Job Template attributes...
    const char	*media_source = cupsGetOption("media-source", num_options, options),
		*media_top_offset = cupsGetOption("media-top-offset", num_options, options),
		*media_tracking = cupsGetOption("media-tracking", num_options, options),
		*media_type = cupsGetOption("media-type", num_options, options);
					// media-xxx member values

    value = cupsGetOption("media", num_options, options);
    if (value || media_source || media_top_offset || media_tracking || media_type)
    {
      // Add media-col
      ipp_t 	*media_col = ippNew();	// media-col value
      pwg_media_t *pwg = pwgMediaForPWG(value);
					// Size

      if (pwg)
      {
        ipp_t		*media_size = ippNew();
					// media-size value

        ippAddInteger(media_size, IPP_TAG_JOB, IPP_TAG_INTEGER, "x-dimension", pwg->width);
        ippAddInteger(media_size, IPP_TAG_JOB, IPP_TAG_INTEGER, "y-dimension", pwg->length);
        ippAddCollection(media_col, IPP_TAG_JOB, "media-size", media_size);
        ippDelete(media_size);
      }

      if (media_source)
        ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-source", NULL, media_source);

      if (media_top_offset)
        ippAddInteger(media_col, IPP_TAG_JOB, IPP_TAG_INTEGER, "media-top-offset", get_length(media_top_offset));

      if (media_tracking)
        ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-tracking", NULL, media_tracking);

      if (media_type)
        ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-type", NULL, media_type);

      ippAddCollection(request, IPP_TAG_JOB, "media-col", media_col);
      ippDelete(media_col);
    }
    else if (value)
    {
      // Add media
      ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media", NULL, value);
    }
  }

  if ((value = cupsGetOption("copies", num_options, options)) == NULL)
    value = cupsGetOption("copies-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "copies-default" : "copies", atoi(value));

  if ((value = cupsGetOption("orientation-requested", num_options, options)) == NULL)
    value = cupsGetOption("orientation-requested-default", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("orientation-requested", value)) != 0)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "orientation-requested-default" : "orientation-requested", intvalue);
    else
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "orientation-requested-default" : "orientation-requested", atoi(value));
  }

  if ((value = cupsGetOption("print-color-mode", num_options, options)) == NULL)
    value = cupsGetOption("print-color-mode-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "print-color-mode-default" : "print-color-mode", NULL, value);

  if ((value = cupsGetOption("print-content-optimize", num_options, options)) == NULL)
    value = cupsGetOption("print-content-optimize-default", num_options, options);
  if (value)
    ippAddString(request, group_tag, IPP_TAG_KEYWORD, is_default ? "print-content-optimize-mode-default" : "print-content-optimize", NULL, value);

  if ((value = cupsGetOption("print-darkness", num_options, options)) == NULL)
    value = cupsGetOption("print-darkness-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-darkness-default" : "print-darkness", atoi(value));

  if ((value = cupsGetOption("print-quality", num_options, options)) == NULL)
    value = cupsGetOption("print-quality-default", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("print-quality", value)) != 0)
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", intvalue);
    else
      ippAddInteger(request, group_tag, IPP_TAG_ENUM, is_default ? "print-quality-default" : "print-quality", atoi(value));
  }

  if ((value = cupsGetOption("print-speed", num_options, options)) == NULL)
    value = cupsGetOption("print-speed-default", num_options, options);
  if (value)
    ippAddInteger(request, group_tag, IPP_TAG_INTEGER, is_default ? "print-speed-default" : "print-speed", get_length(value));

  if ((value = cupsGetOption("printer-resolution", num_options, options)) == NULL)
    value = cupsGetOption("printer-resolution-default", num_options, options);
  LPRINT_DEBUG("printer-resolution=%s\n", value);
  if (value)
  {
    int		xres, yres;		// Resolution values
    char	units[32];		// Resolution units

    if (sscanf(value, "%dx%d%31s", &xres, &yres, units) != 3)
    {
      if (sscanf(value, "%d%31s", &xres, units) != 2)
      {
        xres = 300;

        strlcpy(units, "dpi", sizeof(units));
      }

      yres = xres;
    }

    ippAddResolution(request, group_tag, is_default ? "printer-resolution-default" : "printer-resolution", !strcmp(units, "dpi") ? IPP_RES_PER_INCH : IPP_RES_PER_CM, xres, yres);
  }
}


//
// 'lprintDoAdd()' - Do the add printer sub-command.
//

int					// O - Exit status
lprintDoAdd(int           num_options,	// I - Number of options
	    cups_option_t *options)	// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Create-Printer request
  const char	*device_uri,		// Device URI
		*lprint_driver,		// Name of driver
		*printer_name;		// Name of printer


  // Get required values...
  device_uri    = cupsGetOption("device-uri", num_options, options);
  lprint_driver = cupsGetOption("lprint-driver", num_options, options);
  printer_name  = cupsGetOption("printer-name", num_options, options);

  if (!device_uri || !lprint_driver || !printer_name)
  {
    if (!device_uri)
      fputs("lprint: Missing '-v DEVICE-URI' option.\n", stderr);
    if (!lprint_driver)
      fputs("lprint: Missing '-m DRIVER' option.\n", stderr);
    if (!printer_name)
      fputs("lprint: Missing '-d PRINTER' option.\n", stderr);

    return (1);
  }

  // Open a connection to the server...
  if ((http = lprintConnect(1)) == NULL)
    return (1);

  // Send a Create-Printer request to the server...
  request = ippNewRequest(IPP_OP_CREATE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "lprint-driver", NULL, lprint_driver);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, device_uri);

  lprintAddOptions(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to add printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// 'get_length()' - Get a length in hundredths of millimeters.
//

static int				// O - Length value
get_length(const char *value)		// I - Length string
{
  double	n;			// Number
  char		*units;			// Pointer to units

  n = strtod(value, &units);

  if (units && !strcmp(units, "cm"))
    return ((int)(n * 1000.0));
  if (units && !strcmp(units, "in"))
    return ((int)(n * 2540.0));
  else if (units && !strcmp(units, "mm"))
    return ((int)(n * 100.0));
  else if (units && !strcmp(units, "m"))
    return ((int)(n * 100000.0));
  else
    return ((int)n);
}
