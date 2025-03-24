//
// Seiko Instruments, Inc. driver for LPrint, a Label Printer Application
//
// Copyright © 2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"
#include "util.h"


//
// Local types...
//

enum lprint_slp_cmd_e
{
  LPRINT_SLP_CMD_NOP = 0x00,
  LPRINT_SLP_CMD_STATUS,
  LPRINT_SLP_CMD_VERSION,
  LPRINT_SLP_CMD_BAUDRATE,
  LPRINT_SLP_CMD_PRINT,
  LPRINT_SLP_CMD_PRINTRLE,
  LPRINT_SLP_CMD_MARGIN,
  LPRINT_SLP_CMD_REPEAT,
  LPRINT_SLP_CMD_TAB = 0x09,
  LPRINT_SLP_CMD_LINEFEED,
  LPRINT_SLP_CMD_VERTTAB,
  LPRINT_SLP_CMD_FORMFEED,
  LPRINT_SLP_CMD_SETSPEED,
  LPRINT_SLP_CMD_DENSITY,
  LPRINT_SLP_CMD_RESET,
  LPRINT_SLP_CMD_MODEL = 0x12,
  LPRINT_SLP_CMD_INDENT = 0x16,
  LPRINT_SLP_CMD_FINEMODE,
  LPRINT_SLP_CMD_SETSERIALNUM = 0x1B,
  LPRINT_SLP_CMD_CHECK = 0xA5
};

typedef struct lprint_sii_s		// SII driver data
{
  unsigned	max_width;		// Maximum width in dots
  int		blanks;			// Blank lines
  lprint_dither_t dither;		// Dither buffer
} lprint_sii_t;


//
// Local globals...
//

static const char * const lprint_sii_media[] =
{					// Supported media sizes for labels
  "om_35mm-slide_9x37mm",
  "om_8mm-spine_9x66mm",
  "om_file-folder_13x81mm",
  "om_return_16x43mm",
  "om_vhs-spine_18x141mm",
  "om_round_22x24mm",
  "om_multi-purpose_24x44mm",
  "om_address-small_24x83mm",
  "om_jewelry_27x48mm",
  "om_retail-label_32x37mm",
  "om_cut-hanging-15_32x40mm",
  "om_cut-hanging-13_32x78mm",
  "om_address-large_35x83mm",
  "om_euro-name-badge_38x67mm",
  "om_euro-folder-narrow_38x186mm",
  "om_euro-file-folder_39x49mm",
  "om_vhs-face_45x72mm",
  "om_zip-disk_48x55mm",
  "om_diskette_48x64mm",
  "om_media-badge_48x64mm",
  "om_euro-name-badge-large_48x79mm",
  "om_4-part-label_48x96mm",
  "om_shipping_48x96mm",
  "om_top-coated-paper_48x140mm",
  "om_euro-folder-wide_48x186mm",
  "roll_max_48x186mm",
  "roll_min_9x13mm"
};


//
// Local functions...
//

static unsigned	lprint_sii_get_max_width(const char *driver_name);
static void	lprint_sii_init(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, lprint_sii_t *siidata);
static bool	lprint_sii_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_sii_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_sii_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_sii_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_sii_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_sii_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);


//
// 'lprintSII()' - Initialize the SII driver.
//

bool					// O - `true` on success, `false` on error
lprintSII(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  // Print callbacks...
  data->printfile_cb  = lprint_sii_printfile;
  data->rendjob_cb    = lprint_sii_rendjob;
  data->rendpage_cb   = lprint_sii_rendpage;
  data->rstartjob_cb  = lprint_sii_rstartjob;
  data->rstartpage_cb = lprint_sii_rstartpage;
  data->rwriteline_cb = lprint_sii_rwriteline;

  // Vendor-specific format...
  data->format = LPRINT_SLP_MIMETYPE;

  // Set resolution...
  data->num_resolution = 1;

  if (strstr(driver_name, "_203dpi") != NULL)
    data->x_resolution[0] = data->y_resolution[0] = 203;
  else
    data->x_resolution[0] = data->y_resolution[0] = 300;

  data->x_default = data->y_default = data->x_resolution[0];

  // Basically borderless...
  data->left_right = 1;
  data->bottom_top = 1;

  // Supported media...
  data->num_media = ARRAY_SIZE(lprint_sii_media);
  memcpy(data->media, lprint_sii_media, sizeof(lprint_sii_media));

  data->num_source = 1;
  data->source[0]  = "main-roll";

  papplCopyString(data->media_ready[0].size_name, "om_address-small_24x83mm", sizeof(data->media_ready[0].size_name));
  papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));

  data->num_type = 1;
  data->type[0]  = "labels";

  // Darkness/density settings...
  data->darkness_configured = 50;
  data->darkness_supported  = 3;

  return (true);
}


//
// 'lprint_sii_get_max_width()' - Get the maximum width in dots.
//

static unsigned				// O - Maximum width in dots
lprint_sii_get_max_width(
    const char *driver_name)		// I - Driver name
{
  switch (atoi(driver_name + 7))
  {
    case 100 :
    case 410 :
	return (192);
	break;

    case 200 :
    case 240 :
    case 420 :
    case 430 :
        return (384);
        break;

    default :
        return (576);
        break;
  }
}


//
// 'lprint_sii_init()' - Initialize siidata driver data based on the driver name...
//

static void
lprint_sii_init(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    lprint_sii_t       *siidata)	// O - Driver data
{
  const char	*driver_name = papplPrinterGetDriverName(papplJobGetPrinter(job));
					// Driver name


  (void)options;

  // Initialize driver data and save it...
  siidata->max_width = lprint_sii_get_max_width(driver_name);
  switch (atoi(driver_name + 7))
  {
    case 100 :
    case 410 :
	// Reset printer...
	papplDevicePrintf(device, "%c", LPRINT_SLP_CMD_RESET);
	papplDeviceFlush(device);
	sleep(3);
	break;

    default :
        // Nothing else to do...
        break;
  }

  papplJobSetData(job, siidata);
}


//
// 'lprint_sii_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer
  lprint_sii_t	siidata;		// Driver data


  // Initialize driver data...
  lprint_sii_init(job, options, device, &siidata);

  // Copy the raw file...
  papplJobSetImpressions(job, 1);

  if ((fd = open(papplJobGetFilename(job), O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open print file \"%s\": %s", papplJobGetFilename(job), strerror(errno));
    return (false);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(fd);
      return (false);
    }
  }
  close(fd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'lprint_sii_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_sii_t		*siidata = (lprint_sii_t *)papplJobGetData(job);
					// SII driver data

  (void)options;

  free(siidata);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_sii_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_sii_t	*siidata = (lprint_sii_t *)papplJobGetData(job);
					// SII driver data


  (void)page;

  // Write last line
  lprint_sii_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Eject
  papplDevicePrintf(device, "%c", LPRINT_SLP_CMD_FORMFEED);
  papplDeviceFlush(device);

  // Free memory and return...
  lprintDitherFree(&siidata->dither);

  return (true);
}


//
// 'lprint_sii_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_sii_t		*siidata = (lprint_sii_t *)calloc(1, sizeof(lprint_sii_t));
					// SII driver data


  (void)options;

  // Initialize driver data...
  lprint_sii_init(job, options, device, siidata);

  return (true);
}


//
// 'lprint_sii_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_sii_t *siidata = (lprint_sii_t *)papplJobGetData(job);
					// SII driver data
  const char	*driver_name = papplPrinterGetDriverName(papplJobGetPrinter(job));
					// Driver name
  int		darkness;		// Combined density



  (void)page;

  // Initialize the dither buffer and blanks count...
  if (!lprintDitherAlloc(&siidata->dither, job, options, CUPS_CSPACE_K, options->header.HWResolution[0] == 300 ? 1.2 : 1.0))
    return (false);

  papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_MARGIN, (int)(12.7 * (lprint_sii_get_max_width(driver_name) - options->header.cupsWidth) / options->header.HWResolution[0]));

  siidata->blanks = 0;

  // Set darkness...
  if ((darkness = options->darkness_configured + options->print_darkness) < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_DENSITY, 3 * darkness / 100);

  // Set quality...
  switch (atoi(driver_name + 7))
  {
    case 100 :
    case 200 :
    case 240 :
    case 410 :
    case 420 :
    case 430 :
        papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_FINEMODE, options->print_quality == IPP_QUALITY_HIGH ? 0x01 : 0x00);
        break;

    default :
        papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_SETSPEED, options->print_quality == IPP_QUALITY_HIGH ? 0x02 : 0x00);
        break;
  }

  return (true);
}


//
// 'lprint_sii_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_sii_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_sii_t		*siidata = (lprint_sii_t *)papplJobGetData(job);
					// SII driver data


  // Dither...
  if (!lprintDitherLine(&siidata->dither, y, line))
    return (true);

  if (!siidata->dither.output[0] && !memcmp(siidata->dither.output, siidata->dither.output + 1, siidata->dither.out_width - 1))
  {
    // Skip blank lines...
    siidata->blanks ++;
    return (true);
  }

  // Feed past any blank lines...
  while (siidata->blanks > 0)
  {
    if (siidata->blanks == 1)
    {
      papplDevicePuts(device, "\n");
      siidata->blanks = 0;
    }
    else if (siidata->blanks < 255)
    {
      papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_VERTTAB, (char)siidata->blanks);
      siidata->blanks = 0;
    }
    else
    {
      papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_VERTTAB, (char)255);
      siidata->blanks -= 255;
    }
  }

  // Output bitmap data...
  papplDevicePrintf(device, "%c%c", LPRINT_SLP_CMD_PRINT, (char)siidata->dither.out_width);
  papplDeviceWrite(device, siidata->dither.output, siidata->dither.out_width);

  return (true);
}
