//
// Modify printer sub-command for LPrint, a Label Printer Utility
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
// 'lprintAddPrinterURI()' - Add the printer-uri attribute and return a resource path.
//

void
lprintAddPrinterURI(
    ipp_t      *request,		// I - IPP request
    const char *printer_name,		// I - Printer name
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char	uri[1024];			// printer-uri value


  snprintf(resource, rsize, "/ipp/print/%s", printer_name);
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", 0, resource);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
}


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
  const char	*printer_name;		// Name of printer
  char		resource[1024];		// Resource path


  // Get required values...
  printer_name  = cupsGetOption("printer-name", num_options, options);

  if (!printer_name)
  {
    if (!printer_name)
      fputs("lprint: Missing '-d PRINTER' option.\n", stderr);

    return (1);
  }

  // Open a connection to the server...
  if ((http = lprintConnect(1)) == NULL)
    return (1);

  // Send a Set-Printer-Attributes request to the server...
  request = ippNewRequest(IPP_OP_SET_PRINTER_ATTRIBUTES);
  lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  lprintAddDefaults(request, num_options, options);

  ippDelete(cupsDoRequest(http, request, resource));

  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to modify printer - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
