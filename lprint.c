//
// Main entry for LPrint, a Label Printer Application
//
// Copyright © 2019-2021 by Michael R Sweet.
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


//
// 'lprintDriverCallback()' - Main driver callback.
//

bool					// O - `true` on success, `false` on error
lprintDriverCallback(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  int	i;				// Looping var


  for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
  {
    if (!strcmp(driver_name, lprint_drivers[i].name))
    {
      strncpy(data->make_and_model, lprint_drivers[i].description, sizeof(data->make_and_model) - 1);
      break;
    }
  }

  if (!strncmp(driver_name, "dymo_", 5))
    return (lprintDYMO(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else if (!strncmp(driver_name, "zpl_", 4))
    return (lprintZPL(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else
    return (false);
}
