//
// Main entry for LPrint, a Label Printer Application
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
// Local globals...
//

static pappl_pr_driver_t	lprint_drivers[] =
{
#include "lprint-dymo.h"
#include "lprint-zpl.h"
};


//
// 'main()' - Main entry for LPrint.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  return (papplMainloop(argc, argv,
                        LPRINT_VERSION,
                        "Copyright &copy; 2019-2021 by Michael R Sweet. All Rights Reserved.",
                        (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])),
                        lprint_drivers, /*autoadd_cb*/NULL, lprintDriverCallback,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        /*system_cb*/NULL,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}
