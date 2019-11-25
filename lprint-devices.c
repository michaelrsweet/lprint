//
// Devices sub-command for LPrint, a Label Printer Utility
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
lprintDoDevices(int  argc,		// I - Number of command-line arguments
	        char *argv[])		// I - Command-line arguments
{
  // TODO: Show errors for back command-line options
  (void)argc;
  (void)argv;

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
