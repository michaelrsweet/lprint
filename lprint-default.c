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
  http = lprintConnect();

  if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    if ((printer_name = cupsGetDefault2(http)) == NULL)
    {
      fputs("lprint: No default printer available.\n", stderr);
      httpClose(http);
      return (1);
    }

    httpClose(http);
    puts(printer_name);
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

  // Now that we have the printer-id, set the default-printer-id attribute for
  // the system service...
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
