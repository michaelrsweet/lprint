//
// Delete printer sub-command for LPrint, a Label Printer Application
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
// 'lprintDoDelete()' - Do the delete printer sub-command.
//

int					// O - Exit status
lprintDoDelete(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		resource[1024];		// Resource path
  int		printer_id;		// printer-id value


  // Connect to/start up the server and get the destination printer...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote server...
    if ((http = lprintConnectURI(printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = lprintConnect(1)) == NULL)
    return (1);

  if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    fputs("lprint: No printer specified.\n", stderr);
    httpClose(http);
    return (1);
  }

  // Get the printer-id for the printer we are deleting...
  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
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

  // Now that we have the printer-id, delete it from the system service...
  request = ippNewRequest(IPP_OP_DELETE_PRINTER);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "printer-id", printer_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to delete printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
