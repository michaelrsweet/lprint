//
// Cancel sub-command for LPrint, a Label Printer Application
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
// 'lprintDoCancel()' - Do the cancel sub-command.
//

int					// O - Exit status
lprintDoCancel(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256],	// Default printer
		resource[1024];		// Resource path
  http_t	*http;			// Server connection
  ipp_t		*request;		// IPP request
  const char	*value;			// Option value
  int		job_id = 0;		// job-id


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

  // Figure out which job(s) to cancel...
  if (cupsGetOption("cancel-all", num_options, options))
  {
    request = ippNewRequest(IPP_OP_CANCEL_MY_JOBS);
  }
  else if ((value = cupsGetOption("job-id", num_options, options)) != NULL)
  {
    request = ippNewRequest(IPP_OP_CANCEL_JOB);
    job_id  = atoi(value);
  }
  else
    request = ippNewRequest(IPP_OP_CANCEL_CURRENT_JOB);

  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  if (job_id)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, resource));
  httpClose(http);

  if (cupsLastError() != IPP_STATUS_OK)
  {
    fprintf(stderr, "lprint: Unable to cancel - %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}
