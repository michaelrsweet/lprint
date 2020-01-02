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
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  ipp_attribute_t	*attr;		// Current attribute
  int			i,		// Looping var
			count,		// Number of reasons
			state;		// system-state value
  time_t		state_time;	// system-state-change-time value
  const char		*reason;	// system-state-reasons value
  static const char * const states[] =	// system-state values
  {
    "idle",
    "processing jobs",
    "stopped"
  };
  static const char * const requested[] =
  {					// Requested attributes
    "system-state",
    "system-state-change-date-time",
    "system-state-reasons",
    "system-up-time"
  };


  // Try connecting to the server...
  if ((http = lprintConnect(0)) == NULL)
  {
    puts("Not running.");
    return (0);
  }

  request = ippNewRequest(IPP_OP_GET_SYSTEM_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, "ipp://localhost/ipp/system");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(requested) / sizeof(requested[0])), NULL, requested);

  response = cupsDoRequest(http, request, "/ipp/system");

  if ((state = ippGetInteger(ippFindAttribute(response, "system-state", IPP_TAG_ENUM), 0)) < IPP_PSTATE_IDLE)
    state = IPP_PSTATE_IDLE;
  else if (state > IPP_PSTATE_STOPPED)
    state = IPP_PSTATE_STOPPED;

  state_time = ippDateToTime(ippGetDate(ippFindAttribute(response, "system-state-change-date-time", IPP_TAG_DATE), 0));

  printf("Running, %s since %s\n", states[state - IPP_PSTATE_IDLE], httpGetDateString(state_time));

  if ((attr = ippFindAttribute(response, "system-state-reasons", IPP_TAG_KEYWORD)) != NULL)
  {
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      reason = ippGetString(attr, i, NULL);
      if (strcmp(reason, "none"))
        puts(reason);
    }
  }

  return (0);
}
