//
// Driver list sub-command for LPrint, a Label Printer Utility
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
lprintDoDrivers(int  argc,		// I - Number of command-line arguments
                char *argv[])		// I - Command-line arguments
{
  int		i,			// Looping var
		num_drivers;		// Number of drivers
  const char	* const *drivers;	// Array of drivers


  (void)argv;

  drivers = lprintGetDrivers(&num_drivers);

  for (i = 0; i < num_drivers; i ++)
    printf("%-32s%s\n", drivers[i], lprintGetMakeAndModel(drivers[i]));

  return (0);
}
