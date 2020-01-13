//
// Shutdown sub-command for LPrint, a Label Printer Application
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
// 'lprintDoShutdown()' - Do the shutdown sub-command.
//

int					// O - Exit status
lprintDoShutdown(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t	*http;			// HTTP connection
  ipp_t		*request;		// IPP request


  // Try connecting to the server...
  if ((http = lprintConnect(0)) == NULL)
  {
    fputs("lprint: Server is not running.\n", stderr);
    return (1);
  }

  request = ippNewRequest(IPP_OP_SHUTDOWN_ALL_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/ipp/system"));

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to shutdown server - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
