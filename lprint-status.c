//
// Status sub-command for LPrint, a Label Printer Utility
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
// 'lprintDoStatus()' - Do the status sub-command.
//

int					// O - Exit status
lprintDoStatus(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  http_t		*http;		// HTTP connection
  const char		*printer_name;	// Printer name
  char			resource[1024];	// Resource path
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  int			i,		// Looping var
			count,		// Number of reasons
			state;		// *-state value
  ipp_attribute_t	*state_reasons;	// *-state-reasons attribute
  time_t		state_time;	// *-state-change-time value
  const char		*reason;	// *-state-reasons value
  static const char * const states[] =	// *-state strings
  {
    "idle",
    "processing jobs",
    "stopped"
  };
  static const char * const pattrs[] =
  {					// Requested printer attributes
    "printer-state",
    "printer-state-change-date-time",
    "printer-state-reasons"
  };
  static const char * const sysattrs[] =
  {					// Requested system attributes
    "system-state",
    "system-state-change-date-time",
    "system-state-reasons"
  };


  // Try connecting to the server...
  if ((http = lprintConnect(0)) == NULL)
  {
    puts("Not running.");
    return (0);
  }

  if ((printer_name = cupsGetOption("printer-name", num_options, options)) != NULL)
  {
    request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
    lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

    response      = cupsDoRequest(http, request, resource);
    state         = ippGetInteger(ippFindAttribute(response, "printer-state", IPP_TAG_ENUM), 0);
    state_time    = ippDateToTime(ippGetDate(ippFindAttribute(response, "printer-state-change-date-time", IPP_TAG_DATE), 0));
    state_reasons = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD);
  }
  else
  {
    request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(sysattrs) / sizeof(sysattrs[0])), NULL, sysattrs);

    response      = cupsDoRequest(http, request, "/ipp/system");
    state         = ippGetInteger(ippFindAttribute(response, "system-state", IPP_TAG_ENUM), 0);
    state_time    = ippDateToTime(ippGetDate(ippFindAttribute(response, "system-state-change-date-time", IPP_TAG_DATE), 0));
    state_reasons = ippFindAttribute(response, "system-state-reasons", IPP_TAG_KEYWORD);
  }

  if (state < IPP_PSTATE_IDLE)
    state = IPP_PSTATE_IDLE;
  else if (state > IPP_PSTATE_STOPPED)
    state = IPP_PSTATE_STOPPED;

  printf("Running, %s since %s\n", states[state - IPP_PSTATE_IDLE], httpGetDateString(state_time));

  if (state_reasons)
  {
    for (i = 0, count = ippGetCount(state_reasons); i < count; i ++)
    {
      reason = ippGetString(state_reasons, i, NULL);
      if (strcmp(reason, "none"))
        puts(reason);
    }
  }

  ippDelete(response);

  return (0);
}
