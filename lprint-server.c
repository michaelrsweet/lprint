//
// Server sub-command for LPrint, a Label Printer Application
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
#include <ctype.h>


//
// 'lprintDoServer()' - Do the server sub-command.
//

int					// O - Exit status
lprintDoServer(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  lprint_system_t *system;		// System object
  const char	*val;			// Option value
  lprint_loglevel_t loglevel;		// Log level
  const char	*logfile,		// Log file, if any
		*hostname;		// Hostname, if any
  int		port;			// Port number, if any


  // Get config options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = LPRINT_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = LPRINT_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = LPRINT_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = LPRINT_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = LPRINT_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "lprint: Bad log-level value '%s'.\n", val);
      return (1);
    }
  }
  else
    loglevel = LPRINT_LOGLEVEL_UNSPEC;

  logfile  = cupsGetOption("log-file", num_options, options);
  hostname = cupsGetOption("server-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "lprint: Bad server-port value '%s'.\n", val);
      return (1);
    }
    else
      port = atoi(val);
  }
  else
    port = 0;

  // Create the system object and run it...
  if ((system = lprintCreateSystem(hostname, port, hostname ? "_print,_universal" : NULL, logfile, loglevel, cupsGetOption("auth-service", num_options, options), cupsGetOption("admin-group", num_options, options))) == NULL)
    return (1);

  lprintRunSystem(system);
  lprintDeleteSystem(system);

  return (0);
}
