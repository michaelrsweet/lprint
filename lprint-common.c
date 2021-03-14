//
// Common driver code for LPrint, a Label Printer Application
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
  if (!strncmp(driver_name, "dymo_", 5))
    return (lprintDYMO(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else if (!strncmp(driver_name, "zpl_", 4))
    return (lprintZPL(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else
    return (false);
}
