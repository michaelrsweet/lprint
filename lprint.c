//
// Main entry for LPrint, a Label Printer Application
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
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
// Local types...
//

typedef struct lprint_device_s
{
  char	*device_info;			// Device description
  char	*device_uri;			// Device URI
  char	*device_id;			// Device ID
} lprint_device_t;


//
// Local functions...
//

static const char	*autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata);
static lprint_device_t	*copy_cb(lprint_device_t *src);
static bool		driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
static void		free_cb(lprint_device_t *src);
static int		match_id(int num_did, cups_option_t *did, const char *match_id);
static const char	*mime_cb(const unsigned char *header, size_t headersize, void *data);
static bool		printer_cb(const char *device_info, const char *device_uri, const char *device_id, cups_array_t *devices);
static pappl_system_t	*system_cb(int num_options, cups_option_t *options, void *data);


//
// Local globals...
//

static pappl_pr_driver_t	lprint_drivers[] =
{					// Driver list
#include "lprint-brother.h"
#include "lprint-dymo.h"
#include "lprint-epl2.h"
#include "lprint-sii.h"
#include "lprint-tspl.h"
#include "lprint-zpl.h"
};
static char			lprint_spooldir[1024],
					// Spool directory
				lprint_statefile[1024];
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
                        "Copyright &copy; 2019-2023 by Michael R Sweet. All Rights Reserved.",
                        (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])),
                        lprint_drivers, autoadd_cb, driver_cb,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        system_cb,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}


//
// 'autoadd_cb()' - Determine the proper driver for a given printer.
//

static const char *			// O - Driver name or `NULL` for none
autoadd_cb(const char *device_info,	// I - Device information/name (not used)
           const char *device_uri,	// I - Device URI
           const char *device_id,	// I - IEEE-1284 device ID
           void       *cbdata)		// I - Callback data (System)
{
  int		i,			// Looping var
		score,			// Current driver match score
	    	best_score = 0,		// Best score
		num_did;		// Number of device ID key/value pairs
  cups_option_t	*did;			// Device ID key/value pairs
  const char	*make,			// Manufacturer name
		*best_name = NULL;	// Best driver
  char		name[1024] = "";	// Driver name to match


  (void)device_info;

  // First parse the device ID and get any potential driver name to match...
  num_did = papplDeviceParseID(device_id, &did);

  if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
    if ((make = cupsGetOption("MANU", num_did, did)) == NULL)
      make = cupsGetOption("MFG", num_did, did);

  if (make && !strncasecmp(make, "Zebra", 5))
    lprintZPLQueryDriver((pappl_system_t *)cbdata, device_uri, name, sizeof(name));

  // Then loop through the driver list to find the best match...
  for (i = 0; i < (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])); i ++)
  {
    if (!strcmp(name, lprint_drivers[i].name))
    {
      // Matching driver name always the best match...
      best_name = lprint_drivers[i].name;
      break;
    }

    if (lprint_drivers[i].device_id)
    {
      // See if we have a matching device ID...
      score = match_id(num_did, did, lprint_drivers[i].device_id);
      if (score > best_score)
      {
        best_score = score;
        best_name  = lprint_drivers[i].name;
      }
    }
  }

  // Clean up and return...
  cupsFreeOptions(num_did, did);

  return (best_name);
}


//
// 'copy_cb()' - Copy a device entry.
//

static lprint_device_t *		// O - New device entry
copy_cb(lprint_device_t *src)		// I - Original device entry
{
  lprint_device_t	*dst;		// New device entry


  if ((dst = (lprint_device_t *)calloc(1, sizeof(lprint_device_t))) != NULL)
  {
    dst->device_info = strdup(src->device_info);
    dst->device_uri  = strdup(src->device_uri);
    dst->device_id   = strdup(src->device_id);

    if (!dst->device_info || !dst->device_uri || !dst->device_id)
    {
      free_cb(dst);
      dst = NULL;
    }
  }

  return (dst);
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
  bool	ret = false;			// Return value
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
  data->color_supported   = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_BI_LEVEL;
  data->color_default     = PAPPL_COLOR_MODE_MONOCHROME;
  data->raster_types      = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8;

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

  // Test page callback...
  data->testpage_cb = lprintTestPageCB;

  // Use the corresponding sub-driver callback to set things up...
  if (!strncmp(driver_name, "brother_", 8))
    ret = lprintBrother(system, driver_name, device_uri, device_id, data, attrs, cbdata);
  else if (!strncmp(driver_name, "dymo_", 5))
    ret = lprintDYMO(system, driver_name, device_uri, device_id, data, attrs, cbdata);
  else if (!strncmp(driver_name, "epl2_", 5))
    ret = lprintEPL2(system, driver_name, device_uri, device_id, data, attrs, cbdata);
  else if (!strncmp(driver_name, "sii_", 4))
    ret = lprintSII(system, driver_name, device_uri, device_id, data, attrs, cbdata);
  else if (!strncmp(driver_name, "tspl_", 5))
    ret = lprintTSPL(system, driver_name, device_uri, device_id, data, attrs, cbdata);
  else if (!strncmp(driver_name, "zpl_", 4))
    ret = lprintZPL(system, driver_name, device_uri, device_id, data, attrs, cbdata);

  // Update the ready media...
  for (i = 0; i < data->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(data->media_ready[i].size_name);

    data->media_ready[i].bottom_margin = data->bottom_top;
    data->media_ready[i].left_margin   = data->left_right;
    data->media_ready[i].right_margin  = data->left_right;
    data->media_ready[i].size_width    = pwg->width;
    data->media_ready[i].size_length   = pwg->length;
    data->media_ready[i].top_margin    = data->bottom_top;
    papplCopyString(data->media_ready[i].source, data->source[i], sizeof(data->media_ready[i].source));
    papplCopyString(data->media_ready[i].type, data->type[0], sizeof(data->media_ready[i].type));
  }

  // By default use media from the main source...
  data->media_default = data->media_ready[0];

  // Return...
  return (ret);
}


//
// 'free_cb()' - Free a device entry.
//

static void
free_cb(lprint_device_t *dev)		// I - Device entry
{
  free(dev->device_info);
  free(dev->device_uri);
  free(dev->device_id);
  free(dev);
}


//
// 'match_id()' - Compare two IEEE-1284 device IDs and return a score.
//
// The score is 2 for each exact match and 1 for a partial match in a comma-
// delimited field.  Any non-match results in a score of 0.
//

static int				// O - Score
match_id(int           num_did,		// I - Number of device ID key/value pairs
         cups_option_t *did,		// I - Device ID key/value pairs
         const char    *match_id)	// I - Driver's device ID match string
{
  int		i,			// Looping var
		score = 0,		// Score
		num_mid;		// Number of match ID key/value pairs
  cups_option_t	*mid,			// Match ID key/value pairs
		*current;		// Current key/value pair
  const char	*value,			// Device ID value
		*valptr;		// Pointer into value


  // Parse the matching device ID into key/value pairs...
  if ((num_mid = papplDeviceParseID(match_id, &mid)) == 0)
    return (0);

  // Loop through the match pairs to find matches (or not)
  for (i = num_mid, current = mid; i > 0; i --, current ++)
  {
    if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
    {
      // No match
      score = 0;
      break;
    }

    if (!strcasecmp(current->value, value))
    {
      // Full match!
      score += 2;
    }
    else if ((valptr = strstr(value, current->value)) != NULL)
    {
      // Possible substring match, check
      size_t mlen = strlen(current->value);
					// Length of match value
      if ((valptr == value || valptr[-1] == ',') && (!valptr[mlen] || valptr[mlen] == ','))
      {
        // Partial match!
        score ++;
      }
      else
      {
        // No match
        score = 0;
        break;
      }
    }
    else
    {
      // No match
      score = 0;
      break;
    }
  }

  cupsFreeOptions(num_mid, mid);

  return (score);
}


//
// 'mime_cb()' - MIME typing callback...
//

static const char *			// O - MIME media type or `NULL` if none
mime_cb(const unsigned char *header,	// I - Header data
        size_t              headersize,	// I - Size of header data
        void                *cbdata)	// I - Callback data (not used)
{
  char	testpage[] = LPRINT_TESTPAGE_HEADER;
					// Test page file header


  if (headersize >= sizeof(testpage) && !memcmp(header, testpage, sizeof(testpage)))
    return (LPRINT_TESTPAGE_MIMETYPE);
  else if (headersize >= 2 && header[0] == '^' && isupper(header[1] & 255))
    return (LPRINT_ZPL_MIMETYPE);
  else if (headersize >= 3 && !memcmp(header, "\nN\n", 3))
    return (LPRINT_EPL2_MIMETYPE);
  else
    return (NULL);
}


//
// 'printer_cb()' - Try auto-adding printers.
//

static bool				// O - `false` to continue
printer_cb(const char   *device_info,	// I - Device information
	   const char   *device_uri,	// I - Device URI
	   const char   *device_id,	// I - IEEE-1284 device ID
	   cups_array_t *devices)	// I - Device array
{
  lprint_device_t	dev;		// New device


  dev.device_info = (char *)device_info;
  dev.device_uri  = (char *)device_uri;
  dev.device_id   = (char *)device_id;

  cupsArrayAdd(devices, &dev);

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
			*listenhost,	// Listen hostname, if any
			*logfile,	// Log file, if any
			*spooldir,	// Spool directory, if any
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

  if ((val = cupsGetOption("server-options", num_options, options)) != NULL)
  {
    const char	*valptr;		// Pointer into value

    for (valptr = val; valptr && *valptr;)
    {
      if (!strcmp(valptr, "none") || !strncmp(valptr, "none,", 5))
        soptions = PAPPL_SOPTIONS_NONE;
      else if (!strcmp(valptr, "dnssd-host") || !strncmp(valptr, "dnssd-host,", 11))
        soptions |= PAPPL_SOPTIONS_DNSSD_HOST;
      else if (!strcmp(valptr, "no-multi-queue") || !strncmp(valptr, "no-multi-queue,", 15))
        soptions &= (pappl_soptions_t)~PAPPL_SOPTIONS_MULTI_QUEUE;
      else if (!strcmp(valptr, "raw-socket") || !strncmp(valptr, "raw-socket,", 11))
        soptions |= PAPPL_SOPTIONS_RAW_SOCKET;
      else if (!strcmp(valptr, "usb-printer") || !strncmp(valptr, "usb-printer,", 12))
        soptions |= PAPPL_SOPTIONS_USB_PRINTER;
      else if (!strcmp(valptr, "no-web-interface") || !strncmp(valptr, "no-web-interface,", 17))
        soptions &= (pappl_soptions_t)~PAPPL_SOPTIONS_WEB_INTERFACE;
      else if (!strcmp(valptr, "web-log") || !strncmp(valptr, "web-log,", 8))
        soptions |= PAPPL_SOPTIONS_WEB_LOG;
      else if (!strcmp(valptr, "web-network") || !strncmp(valptr, "web-network,", 12))
        soptions |= PAPPL_SOPTIONS_WEB_NETWORK;
      else if (!strcmp(valptr, "web-remote") || !strncmp(valptr, "web-remote,", 11))
        soptions |= PAPPL_SOPTIONS_WEB_REMOTE;
      else if (!strcmp(valptr, "web-security") || !strncmp(valptr, "web-security,", 13))
        soptions |= PAPPL_SOPTIONS_WEB_SECURITY;
      else if (!strcmp(valptr, "no-tls") || !strncmp(valptr, "no-tls,", 7))
        soptions |= PAPPL_SOPTIONS_NO_TLS;

      if ((valptr = strchr(valptr, ',')) != NULL)
        valptr ++;
    }
  }

  hostname    = cupsGetOption("server-hostname", num_options, options);
  listenhost  = cupsGetOption("listen-hostname", num_options, options);
  logfile     = cupsGetOption("log-file", num_options, options);
  spooldir    = cupsGetOption("spool-directory", num_options, options);
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

  // Spool directory and state file...
  if ((val = getenv("SNAP_DATA")) != NULL)
  {
    snprintf(lprint_spooldir, sizeof(lprint_spooldir), "%s/lprint.d", val);
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/lprint.conf", val);

    if (!spooldir)
      spooldir = lprint_spooldir;
  }
  else if ((val = getenv("XDG_DATA_HOME")) != NULL)
  {
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/.lprint.conf", val);
  }
#ifdef _WIN32
  else if ((val = getenv("USERPROFILE")) != NULL)
  {
    snprintf(lprint_spooldir, sizeof(lprint_spooldir), "%s/AppData/Local/lprint.d", val);
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/AppData/Local/lprint.ini", val);

    if (!spooldir)
      spooldir = lprint_spooldir;
  }
  else
  {
    papplCopyString(lprint_statefile, "/lprint.ini", sizeof(lprint_statefile));
  }
#else
  else if ((val = getenv("HOME")) != NULL)
  {
#  ifdef __APPLE__
    snprintf(lprint_spooldir, sizeof(lprint_spooldir), "%s/Library/Application Support/lprint.d", val);
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/Library/Application Support/lprint.state", val);

    if (!spooldir)
      spooldir = lprint_spooldir;
#  else
    snprintf(lprint_statefile, sizeof(lprint_statefile), "%s/.lprint.conf", val);
#  endif // __APPLE__
  }
  else
  {
    papplCopyString(lprint_statefile, "/etc/lprint.conf", sizeof(lprint_statefile));
  }
#endif // _WIN32

  if (spooldir && access(spooldir, 0))
  {
    if (mkdir(spooldir, 0777))
    {
      perror(spooldir);
      return (NULL);
    }
  }

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "LPrint", port, "_print,_universal", spooldir, logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  if (!cupsGetOption("private-server", num_options, options))
  {
    // Listen for TCP/IP connections...
    papplSystemAddListeners(system, listenhost);
  }

  papplSystemSetHostName(system, hostname);

  if ((val = cupsGetOption("admin-group", num_options, options)) != NULL)
    papplSystemSetAdminGroup(system, val);

  papplSystemSetMIMECallback(system, mime_cb, NULL);
  papplSystemAddMIMEFilter(system, LPRINT_TESTPAGE_MIMETYPE, "image/pwg-raster", lprintTestFilterCB, NULL);

  papplSystemSetPrinterDrivers(system, (int)(sizeof(lprint_drivers) / sizeof(lprint_drivers[0])), lprint_drivers, autoadd_cb, /*create_cb*/NULL, driver_cb, system);

  papplSystemAddResourceData(system, "/favicon.png", "image/png", lprint_small_png, sizeof(lprint_small_png));
  papplSystemAddResourceData(system, "/navicon.png", "image/png", lprint_png, sizeof(lprint_png));
  papplSystemAddResourceString(system, "/de.strings", "text/strings", lprint_de_strings);
  papplSystemAddResourceString(system, "/en.strings", "text/strings", lprint_en_strings);
  papplSystemAddResourceString(system, "/es.strings", "text/strings", lprint_es_strings);
  papplSystemAddResourceString(system, "/fr.strings", "text/strings", lprint_fr_strings);
  papplSystemAddResourceString(system, "/it.strings", "text/strings", lprint_it_strings);

  papplSystemSetFooterHTML(system, "Copyright &copy; 2019-2022 by Michael R Sweet. All rights reserved.");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)lprint_statefile);
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  if (!papplSystemLoadState(system, lprint_statefile))
  {
    // No old state, use defaults and auto-add printers...
    cups_array_t	*devices;	// Device array
    lprint_device_t	*dev;		// Current device

    papplSystemSetDNSSDName(system, system_name ? system_name : "LPrint");

    papplLog(system, PAPPL_LOGLEVEL_INFO, "Auto-adding printers...");
    devices = cupsArrayNew(NULL, NULL, NULL, 0, (cups_acopy_cb_t)copy_cb, (cups_afree_cb_t)free_cb);
    papplDeviceList(PAPPL_DEVTYPE_USB, (pappl_device_cb_t)printer_cb, devices, papplLogDevice, system);

    for (dev = (lprint_device_t *)cupsArrayGetFirst(devices); dev; dev = (lprint_device_t *)cupsArrayGetNext(devices))
    {
      const char *driver_name = autoadd_cb(dev->device_info, dev->device_uri, dev->device_id, system);
					// Driver name, if any

      printf("%s (%s;%s) -> %s\n", dev->device_info, dev->device_uri, dev->device_id, driver_name);

      if (driver_name)
      {
	char	name[128],		// Printer name
		*nameptr;		// Pointer in name

	// Zebra puts "Zebra Technologies ZTC" on the front of their printer names,
	// which is a bit, um, wordy.  Clean up the device info string to use as a
	// printer name and drop any trailing "(ID)" nonsense if we don't need it.
	if (!strncasecmp(dev->device_info, "Zebra Technologies ZTC ", 23))
	  snprintf(name, sizeof(name), "Zebra %s", dev->device_info + 23);
	else
	  papplCopyString(name, dev->device_info, sizeof(name));

	if ((nameptr = strstr(name, " (")) != NULL)
	  *nameptr = '\0';

	if (!papplPrinterCreate(system, 0, name, driver_name, dev->device_id, dev->device_uri))
	{
	  // Printer already exists with this name, so try adding a number to the
	  // name...
	  int	i;			// Looping var
	  char	newname[128],		// New name
		number[4];		// Number string
	  size_t namelen = strlen(name),// Length of original name string
		numberlen;		// Length of number string

	  for (i = 2; i < 100; i ++)
	  {
	    // Append " NNN" to the name, truncating the existing name as needed to
	    // include the number at the end...
	    snprintf(number, sizeof(number), " %d", i);
	    numberlen = strlen(number);

	    papplCopyString(newname, name, sizeof(newname));
	    if ((namelen + numberlen) < sizeof(newname))
	      memcpy(newname + namelen, number, numberlen + 1);
	    else
	      memcpy(newname + sizeof(newname) - numberlen - 1, number, numberlen + 1);

	    // Try creating with this name...
	    if (papplPrinterCreate(system, 0, newname, driver_name, dev->device_id, dev->device_uri))
	      break;
	  }
	}
      }
    }

    cupsArrayDelete(devices);
  }

  return (system);
}


