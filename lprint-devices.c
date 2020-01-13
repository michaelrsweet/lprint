//
// Devices sub-command for LPrint, a Label Printer Application
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
// Local functions...
//

static int	lprint_print_device(const char *device_uri, const void *user_data);


//
// 'lprintDoDevices()' - Do the devices sub-command.
//

int					// O - Exit status
lprintDoDevices(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  (void)num_options;
  (void)options;

  lprintListDevices(lprint_print_device, NULL);

  return (0);
}


//
// 'lprint_print_device()' - Print a device URI.
//

static int				// O - 0 to keep going
lprint_print_device(
    const char *device_uri,		// I - Device URI
    const void *user_data)		// I - User data (not used)
{
  (void)user_data;

  puts(device_uri);

  return (0);
}
