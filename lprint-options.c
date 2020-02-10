//
// Options sub-command for LPrint, a Label Printer Application
//
// Copyright © 2020 by Michael R Sweet.
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

static char	*get_value(ipp_attribute_t *attr, const char *name, int element, char *buffer, size_t bufsize);
static void	print_option(ipp_t *response, const char *name);


//
// 'lprintDoOptions()' - Do the options sub-command.
//

int					// O - Exit status
lprintDoOptions(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256];	// Default printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = lprintConnectURI(printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    http = lprintConnect(1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = lprintGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
	fputs("lprint: No default printer available.\n", stderr);
	httpClose(http);
	return (1);
      }
    }
  }

  // Get the xxx-supported and xxx-default attributes
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, resource);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to get printer options - %s\n", cupsLastErrorString());
    ippDelete(response);
    httpClose(http);
    return (1);
  }

  puts("Print job options:");
  puts("  -n COPIES");
  print_option(response, "media");
  print_option(response, "media-source");
  print_option(response, "media-top-offset");
  print_option(response, "media-tracking");
  print_option(response, "media-type");
  print_option(response, "orientation-requested");
  print_option(response, "print-color-mode");
  print_option(response, "print-content-optimize");
  if (ippFindAttribute(response, "print-darkness-supported", IPP_TAG_ZERO))
    puts("  -o print-darkness=-100 to 100");
  print_option(response, "print-quality");
  print_option(response, "print-speed");
  print_option(response, "printer-resolution");
  puts("  -t 'TITLE'");

  puts("\nPrinter options:");
  print_option(response, "label-mode");
  print_option(response, "label-tear-offset");
  if (ippFindAttribute(response, "printer-darkness-supported", IPP_TAG_ZERO))
    puts("  -o printer-darkness=0 to 100");
  puts("  -o printer-geo-location='geo:LATITUDE,LONGITUDE'");
  puts("  -o printer-location='LOCATION'");
  puts("  -o printer-organization='ORGANIZATION'");
  puts("  -o printer-organizational-unit='UNIT/SECTION'");

  ippDelete(response);
  httpClose(http);

  return (0);
}


//
// 'get_value() ' - Get the string representation of an attribute value.
//

static char *				// O - String value
get_value(ipp_attribute_t *attr,	// I - Attribute
          const char      *name,	// I - Base name of attribute
          int             element,	// I - Value index
          char            *buffer,	// I - String buffer
          size_t          bufsize)	// I - Size of string buffer
{
  const char	*value;			// String value
  int		intvalue;		// Integer value
  int		lower,			// Lower range value
		upper;			// Upper range value
  int		xres,			// X resolution
		yres;			// Y resolution
  ipp_res_t	units;			// Resolution units


  *buffer = '\0';

  switch (ippGetValueTag(attr))
  {
    default :
    case IPP_TAG_KEYWORD :
        if ((value = ippGetString(attr, element, NULL)) != NULL)
        {
          if (!strcmp(name, "media"))
          {
            pwg_media_t	*pwg = pwgMediaForPWG(value);
					// Media size

            if ((pwg->width % 100) == 0)
              snprintf(buffer, bufsize, "%s (%dx%dmm or %.2gx%.2gin)", value, pwg->width / 100, pwg->length / 100, pwg->width / 2540.0, pwg->length / 2540.0);
	    else
              snprintf(buffer, bufsize, "%s (%.2gx%.2gin or %dx%dmm)", value, pwg->width / 2540.0, pwg->length / 2540.0, pwg->width / 100, pwg->length / 100);
          }
          else
          {
            strlcpy(buffer, value, bufsize);
	  }
	}
	break;

    case IPP_TAG_ENUM :
        strlcpy(buffer, ippEnumString(name, ippGetInteger(attr, element)), bufsize);
        break;

    case IPP_TAG_INTEGER :
        intvalue = ippGetInteger(attr, element);

        if (!strcmp(name, "label-tear-offset") || !strcmp(name, "media-top-offset") || !strcmp(name, "print-speed"))
        {
          if ((intvalue % 635) == 0)
            snprintf(buffer, bufsize, "%.2gin", intvalue / 2540.0);
	  else
	    snprintf(buffer, bufsize, "%.2gmm", intvalue * 0.01);
        }
        else
          snprintf(buffer, bufsize, "%d", intvalue);
	break;

    case IPP_TAG_RANGE :
        lower = ippGetRange(attr, element, &upper);

        if (!strcmp(name, "label-tear-offset") || !strcmp(name, "media-top-offset") || !strcmp(name, "print-speed"))
        {
          if ((upper % 635) == 0)
            snprintf(buffer, bufsize, "%.2gin to %.2gin", lower / 2540.0, upper / 2540.0);
	  else
	    snprintf(buffer, bufsize, "%.2gmm to %.2gmm", lower * 0.01, upper * 0.01);
        }
        else
          snprintf(buffer, bufsize, "%d to %d", lower, upper);
	break;

    case IPP_TAG_RESOLUTION :
        xres = ippGetResolution(attr, element, &yres, &units);
        if (xres == yres)
	  snprintf(buffer, bufsize, "%d%s", xres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	else
	  snprintf(buffer, bufsize, "%dx%d%s", xres, yres, units == IPP_RES_PER_INCH ? "dpi" : "dpcm");
	break;
  }

  return (buffer);
}


//
// 'print_option()' - Print the supported and default value for an option.
//

static void
print_option(ipp_t      *response,	// I - Get-Printer-Attributes response
             const char *name)		// I - Attribute name
{
  char		defname[256],		// xxx-default/xxx-configured name
		supname[256];		// xxx-supported name
  ipp_attribute_t *defattr,		// xxx-default/xxx-configured attribute
		*supattr;		// xxx-supported attribute
  int		i,			// Looping var
		count;			// Number of values
  char		defvalue[256],		// xxx-default/xxx-configured value
		supvalue[256];		// xxx-supported value


  // Get the default and supported attributes...
  snprintf(supname, sizeof(supname), "%s-supported", name);
  if ((supattr = ippFindAttribute(response, supname, IPP_TAG_ZERO)) == NULL)
    return;

  if (!strncmp(name, "media-", 6))
    snprintf(defname, sizeof(defname), "media-col-default/%s", name);
  else
    snprintf(defname, sizeof(defname), "%s-default", name);
  if ((defattr = ippFindAttribute(response, defname, IPP_TAG_ZERO)) == NULL)
  {
    snprintf(defname, sizeof(defname), "%s-configured", name);
    defattr = ippFindAttribute(response, defname, IPP_TAG_ZERO);
  }
  get_value(defattr, name, 0, defvalue, sizeof(defvalue));

  // Show the option with its values...
  if (defvalue[0])
    printf("  -o %s=%s (default)\n", name, defvalue);

  for (i = 0, count = ippGetCount(supattr); i < count; i ++)
  {
    if (strcmp(defvalue, get_value(supattr, name, i, supvalue, sizeof(supvalue))))
      printf("  -o %s=%s\n", name, supvalue);
  }
}

