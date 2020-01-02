//
// Set default printer sub-command for LPrint, a Label Printer Utility
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
// 'lprintDoDefault()' - Do the default printer sub-command.
//

int					// O - Exit status
lprintDoDefault(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_name;		// Printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path
  int		printer_id;		// printer-id value


  // Connect to/start up the server and get the destination printer...
  http = lprintConnect(1);

  if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    char	default_printer[256];	// Default printer

    if (lprintGetDefaultPrinter(http, default_printer, sizeof(default_printer)))
      puts(default_printer);

    httpClose(http);
    return (0);
  }

  // OK, setting the default printer so get the printer-id for it...
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-id");

  response   = cupsDoRequest(http, request, resource);
  printer_id = ippGetInteger(ippFindAttribute(response, "printer-id", IPP_TAG_INTEGER), 0);
  ippDelete(response);

  if (printer_id == 0)
  {
    fprintf(stderr, "lprint: Unable to get information for '%s' - %s\n", printer_name, cupsLastErrorString());
    httpClose(http);
    return (1);
  }

  // Now that we have the printer-id, set the system-default-printer-id
  // attribute for the system service...
  request = ippNewRequest(IPP_OP_SET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddInteger(request, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", printer_id);

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to set default printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


//
// 'lprintGetDefaultPrinter()' - Get the default printer.
//
// Note: We can't use cupsGetDefault2() because it uses the defaults set via
// CUPS environment variables and the lpoptions command, too.
//

char *					// O - Default printer or `NULL` for none
lprintGetDefaultPrinter(
    http_t *http,			// I - HTTP connection
    char   *buffer,			// I - Buffer for printer name
    size_t bufsize)			// I - Size of buffer
{
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  const char	*printer_name;		// Printer name


  // Ask the server for its default printer; can't use cupsGetDefault2
  // because it will return a default printer for CUPS...
  request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-name");

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((printer_name = ippGetString(ippFindAttribute(response, "printer-name", IPP_TAG_NAME), 0, NULL)) != NULL)
    strlcpy(buffer, printer_name, bufsize);
  else
    *buffer = '\0';

  ippDelete(response);

  return (*buffer ? buffer : NULL);
}
