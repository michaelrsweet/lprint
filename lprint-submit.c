//
// Submit sub-command for LPrint, a Label Printer Utility
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
// 'lprintDoSubmit()' - Do the submit sub-command.
//

int					// O - Exit status
lprintDoSubmit(
    int           num_files,		// I - Number of files
    char          **files,		// I - Files
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  (void)num_files;
  (void)files;
  (void)num_options;
  (void)options;

  return (1);
}
