//
// Modify printer sub-command for LPrint, a Label Printer Application
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
// 'lprintDoModify()' - Do the modify printer sub-command.
//

int					// O - Exit status
lprintDoModify(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Connection to server
  ipp_t		*request;		// Create-Printer request
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Open a connection to the server...
  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote server...
    if ((http = lprintConnectURI(printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else if ((http = lprintConnect(1)) == NULL)
  {
    return (1);
  }
  else if ((printer_name  = cupsGetOption("printer-name", num_options, options)) == NULL)
  {
    fputs("lprint: Missing '-d PRINTER' option.\n", stderr);

    return (1);
  }

  // Send a Set-Printer-Attributes request to the server...
  request = ippNewRequest(IPP_OP_SET_PRINTER_ATTRIBUTES);
  if (printer_uri)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  lprintAddOptions(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to modify printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
