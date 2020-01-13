//
// Driver list sub-command for LPrint, a Label Printer Application
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
// 'lprintDoDrivers()' - Do the drivers sub-command.
//

int					// O - Exit status
lprintDoDrivers(
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  (void)num_options;
  (void)options;

  int		i,			// Looping var
		num_drivers;		// Number of drivers
  const char	* const *drivers;	// Array of drivers


  drivers = lprintGetDrivers(&num_drivers);

  for (i = 0; i < num_drivers; i ++)
    printf("%-32s%s\n", drivers[i], lprintGetMakeAndModel(drivers[i]));

  return (0);
}
