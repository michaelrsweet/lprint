//
// Add printer sub-command for LPrint, a Label Printer Utility
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
// 'lprintAddDefaults()' - Add default printer attributes from options.
//

void
lprintAddDefaults(
    ipp_t         *request,		// I - IPP request
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*value;			// String value
  int		intvalue;		// Integer value


  if ((value = cupsGetOption("printer-location", num_options, options)) != NULL)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, value);
  if ((value = cupsGetOption("printer-geo-location", num_options, options)) != NULL)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, value);
  if ((value = cupsGetOption("printer-organization", num_options, options)) != NULL)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, value);
  if ((value = cupsGetOption("printer-organizational-unit", num_options, options)) != NULL)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, value);

  if ((value = cupsGetOption("copies-default", num_options, options)) == NULL)
    value = cupsGetOption("copies", num_options, options);
  if (value)
    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", atoi(value));

  if ((value = cupsGetOption("finishings-default", num_options, options)) == NULL)
    value = cupsGetOption("finishings", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("finishings", value)) != 0)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", intvalue);
    else
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", atoi(value));
  }

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

  if ((value = cupsGetOption("orientation-requested-default", num_options, options)) == NULL)
    value = cupsGetOption("orientation-requested", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("orientation-requested", value)) != 0)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", intvalue);
    else
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", atoi(value));
  }

  if ((value = cupsGetOption("print-color-mode-default", num_options, options)) == NULL)
    value = cupsGetOption("print-color-mode", num_options, options);
  if (value)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-color-mode-default", NULL, value);

  if ((value = cupsGetOption("print-content-optimize-default", num_options, options)) == NULL)
    value = cupsGetOption("print-content-optimize-mode", num_options, options);
  if (value)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-content-optimize-mode-default", NULL, value);

  if ((value = cupsGetOption("print-quality-default", num_options, options)) == NULL)
    value = cupsGetOption("print-quality", num_options, options);
  if (value)
  {
    if ((intvalue = ippEnumValue("print-quality", value)) != 0)
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", intvalue);
    else
      ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", atoi(value));
  }

  if ((value = cupsGetOption("printer-resolution-default", num_options, options)) == NULL)
    value = cupsGetOption("printer-resolution", num_options, options);
  if (value)
  {
    int		xres, yres;		// Resolution values
    char	units[32];		// Resolution units

    if (sscanf(value, "%dx%d%31s", &xres, &yres, units) != 3)
    {
      if (sscanf(value, "%d%31s", &xres, units) != 2)
      {
        xres = 300;

        strncpy(units, "dpi", sizeof(units) - 1);
        units[sizeof(units) - 1] = '\0';
      }

      yres = xres;
    }

    ippAddResolution(request, IPP_TAG_PRINTER, "printer-resolution-default", xres, yres, !strcmp(units, "dpi") ? IPP_RES_PER_INCH : IPP_RES_PER_CM);
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
  if ((http = lprintConnect()) == NULL)
    return (1);

  // Send a Create-Printer request to the server...
  request = ippNewRequest(IPP_OP_CREATE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "lprint-driver", NULL, lprint_driver);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, device_uri);

  lprintAddDefaults(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to add printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
