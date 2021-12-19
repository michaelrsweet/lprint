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
#include "static-resources/lprint-de-strings.h"
#include "static-resources/lprint-en-strings.h"
#include "static-resources/lprint-es-strings.h"
#include "static-resources/lprint-fr-strings.h"
#include "static-resources/lprint-it-strings.h"
#include "static-resources/lprint-png.h"
#include "static-resources/lprint-large-png.h"
#include "static-resources/lprint-small-png.h"


//
// Local functions...
//

static bool	driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
static pappl_system_t *system_cb(int num_options, cups_option_t *options, void *data);


//
// Local globals...
//

static pappl_pr_driver_t	lprint_drivers[] =
{					// Driver list
#include "lprint-dymo.h"
#include "lprint-epl2.h"
#include "lprint-zpl.h"
};
static char			lprint_statefile[1024];
					// State file


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
                        lprint_drivers, /*autoadd_cb*/NULL, driver_cb,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        system_cb,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}


//
// 'driver_cb()' - Main driver callback.
//

static bool				// O - `true` on success, `false` on error
driver_cb(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  int	i;				// Looping var


  // Copy make/model info...
  for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
  {
    if (!strcmp(driver_name, lprint_drivers[i].name))
    {
      papplCopyString(data->make_and_model, lprint_drivers[i].description, sizeof(data->make_and_model));
      break;
    }
  }

  // AirPrint version...
  data->num_features = 1;
  data->features[0]  = "airprint-2.1";

  // Pages per minute (interpret as "labels per minute")
  data->ppm = 60;

  // "printer-kind" values...
  data->kind = PAPPL_KIND_LABEL;

  // Color values...
  data->color_supported   = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
  data->color_default     = PAPPL_COLOR_MODE_MONOCHROME;
  data->raster_types      = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;
  data->force_raster_type = PAPPL_PWG_RASTER_TYPE_BLACK_1;

  // "print-quality-default" value...
  data->quality_default = IPP_QUALITY_NORMAL;

  // "sides" values...
  data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  // "orientation-requested-default" value...
  data->orient_default = IPP_ORIENT_NONE;

  // Media capabilities...
  data->input_face_up  = true;
  data->output_face_up = true;

  // Standard icons...
  data->icons[0].data    = lprint_small_png;
  data->icons[0].datalen = sizeof(lprint_small_png);
  data->icons[1].data    = lprint_png;
  data->icons[1].datalen = sizeof(lprint_png);
  data->icons[2].data    = lprint_large_png;
  data->icons[2].datalen = sizeof(lprint_large_png);

  // Use the corresponding sub-driver callback to set things up...
  if (!strncmp(driver_name, "dymo_", 5))
    return (lprintDYMO(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else if (!strncmp(driver_name, "epl2_", 5))
    return (lprintEPL2(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else if (!strncmp(driver_name, "zpl_", 4))
    return (lprintZPL(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else
    return (false);
}


//
// 'system_cb()' - Setup the system object.
//

static pappl_system_t *			// O - System object
system_cb(
    int           num_options,		// I - Number options
    cups_option_t *options,		// I - Options
    void          *data)		// I - Callback data (unused)
{
  pappl_system_t	*system;	// System object
  const char		*val,		// Current option value
			*hostname,	// Hostname, if any
			*logfile,	// Log file, if any
			*system_name;	// System name, if any
  pappl_loglevel_t	loglevel;	// Log level
  int			port = 0;	// Port number, if any
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_SECURITY;
					// System options
  static pappl_version_t versions[1] =	// Software versions
  {
    { "LPrint", "", LPRINT_VERSION, { LPRINT_MAJOR_VERSION, LPRINT_MINOR_VERSION, LPRINT_PATCH_VERSION, 0 } }
  };


  (void)data;

  // Enable/disable TLS...
  if ((val = cupsGetOption("tls", num_options, options)) != NULL && !strcmp(val, "no"))
    soptions |= PAPPL_SOPTIONS_NO_TLS;

  // Parse standard log and server options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "lprint: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile     = cupsGetOption("log-file", num_options, options);
  hostname    = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "lprint: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // State file...
  if ((val = getenv("SNAP_DATA")) != NULL)
  {
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/lprintrc", val);
  }
  else if ((val = getenv("XDG_DATA_HOME")) != NULL)
  {
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/.lprintrc", val);
  }
#ifdef _WIN32
  else if ((val = getenv("USERPROFILE")) != NULL)
  {
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/AppData/Local/lprint.ini", val);
  }
  else
  {
    papplCopyString(lprint_statefile, "/lprint.ini", sizeof(lprint_statefile));
  }
#else
  else if ((val = getenv("HOME")) != NULL)
  {
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/.lprintrc", val);
  }
  else
  {
    papplCopyString(lprint_statefile, "/etc/lprintrc", sizeof(lprint_statefile));
  }
#endif // _WIN32

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "LPrint", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostName(system, hostname);

  papplSystemSetPrinterDrivers(system, (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])), lprint_drivers, /*autoadd_cb*/NULL, /*create_cb*/NULL, driver_cb, NULL);

  papplSystemAddResourceData(system, "/favicon.png", "image/png", lprint_small_png, sizeof(lprint_small_png));
  papplSystemAddResourceData(system, "/navicon.png", "image/png", lprint_png, sizeof(lprint_png));
  papplSystemAddResourceString(system, "/de.strings", "text/strings", lprint_de_strings);
  papplSystemAddResourceString(system, "/en.strings", "text/strings", lprint_en_strings);
  papplSystemAddResourceString(system, "/es.strings", "text/strings", lprint_es_strings);
  papplSystemAddResourceString(system, "/fr.strings", "text/strings", lprint_fr_strings);
  papplSystemAddResourceString(system, "/it.strings", "text/strings", lprint_it_strings);

  papplSystemSetFooterHTML(system, "Copyright &copy; 2019-2021 by Michael R Sweet. All rights reserved.");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)lprint_statefile);
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  if (!papplSystemLoadState(system, lprint_statefile))
    papplSystemSetDNSSDName(system, system_name ? system_name : "LPrint");

  return (system);
}


