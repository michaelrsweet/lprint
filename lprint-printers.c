//
// Printers sub-command for LPrint, a Label Printer Application
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
// 'lprintDoPrinters()' - Do the printers sub-command.
//

int					// O - Exit status
lprintDoPrinters(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Current attribute


  // Connect to/start up the server and get the list of printers...
  http = lprintConnect(1);

  request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  response = cupsDoRequest(http, request, "/ipp/system");

  for (attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME); attr; attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME))
    puts(ippGetString(attr, 0, NULL));

  ippDelete(response);
  httpClose(http);

  return (0);
}
