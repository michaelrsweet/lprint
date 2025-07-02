//
// Experimental CPCL driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"
#ifdef LPRINT_EXPERIMENTAL


//
// Local types...
//

typedef struct lprint_cpcl_s		// CPCL driver data
{
  lprint_dither_t dither;		// Dither buffer
} lprint_cpcl_t;


//
// Local globals...
//

static const char * const lprint_cpcl_media[] =
{					// Supported media sizes
  "oe_1.25x0.25-label_1.25x0.25in",
  "oe_1.25x2.25-label_1.25x2.25in",

  "oe_1.5x0.25-label_1.5x0.25in",
  "oe_1.5x0.5-label_1.5x0.5in",
  "oe_1.5x1-label_1.5x1in",
  "oe_1.5x2-label_1.5x2in",

  "oe_2x0.37-label_2x0.37in",
  "oe_2x0.5-label_2x0.5in",
  "oe_2x1-label_2x1in",
  "oe_2x1.25-label_2x1.25in",
  "oe_2x2-label_2x2in",
  "oe_2x3-label_2x3in",
  "oe_2x4-label_2x4in",
  "oe_2x5.5-label_2x5.5in",

  "oe_2.25x0.5-label_2.25x0.5in",
  "oe_2.25x1.25-label_2.25x1.25in",
  "oe_30859-paint-can-label_2.25x3.125in",
  "oe_2.25x4-label_2.25x4in",
  "oe_2.25x5.5-label_2.25x5.5in",

  "oe_2.38x5.5-label_2.38x5.5in",

  "oe_2.5x1-label_2.5x1in",
  "oe_2.5x2-label_2.5x2in",

  "oe_2.75x1.25-label_2.75x1.25in",

  "oe_2.9x1-label_2.9x1in",

  "oe_3x1-label_3x1in",
  "oe_3x1.25-label_3x1.25in",
  "oe_3x2-label_3x2in",
  "oe_3x3-label_3x3in",
  "oe_3x5-label_3x5in",

  "oe_3.25x2-label_3.25x2in",
  "oe_3.25x5-label_3.25x5in",
  "oe_3.25x5.5-label_3.25x5.5in",
  "oe_3.25x5.83-label_3.25x5.83in",
  "oe_3.25x7.83-label_3.25x7.83in",

  "oe_3.5x1-label_3.5x1in",

  "oe_4x1-label_4x1in",
  "oe_4x2-label_4x2in",
  "oe_4x3-label_4x3in",
  "oe_4x4-label_4x4in",
  "oe_4x5-label_4x5in",
  "oe_4x6-label_4x6in",
  "oe_4x6.5-label_4x6.5in",
  "oe_4x7.83-label_4x7.83in",
  "oe_4x13-label_4x13in",

  "roll_max_4x50in",
  "roll_min_1.25x0.25in"
};

static const char * const lprint_hma300e_media[] =
{					// Supported media sizes
  "roll_max_80x1000mm",
  "roll_min_25x25mm"
};


//
// Local functions...
//

static bool	lprint_cpcl_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_cpcl_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_cpcl_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_cpcl_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_cpcl_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_cpcl_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_cpcl_status(pappl_printer_t *printer);


//
// 'lprintInit()' - Initialize the driver.
//

bool					// O - `true` on success, `false` on error
lprintCPCL(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = lprint_cpcl_printfile;
  data->rendjob_cb    = lprint_cpcl_rendjob;
  data->rendpage_cb   = lprint_cpcl_rendpage;
  data->rstartjob_cb  = lprint_cpcl_rstartjob;
  data->rstartpage_cb = lprint_cpcl_rstartpage;
  data->rwriteline_cb = lprint_cpcl_rwriteline;
  data->status_cb     = lprint_cpcl_status;
  data->format        = LPRINT_CPCL_MIMETYPE;

  data->num_resolution = 1;

  if (strstr(driver_name, "-203dpi"))
  {
    data->x_resolution[0] = 203;
    data->y_resolution[0] = 203;
  }
  else
  {
    data->x_resolution[0] = 300;
    data->y_resolution[0] = 300;
  }

  data->x_default = data->y_default = data->x_resolution[0];

  data->finishings |= PAPPL_FINISHINGS_TRIM;

  if (!strncmp(driver_name, "cpcl_hma300e-", 13))
  {
    data->num_media = (int)(sizeof(lprint_hma300e_media) / sizeof(lprint_hma300e_media[0]));
    memcpy(data->media, lprint_hma300e_media, sizeof(lprint_hma300e_media));

    papplCopyString(data->media_ready[0].size_name, "roll_main_80x150mm", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "continuous", sizeof(data->media_ready[0].type));
  }
  else
  {
    data->num_media = (int)(sizeof(lprint_cpcl_media) / sizeof(lprint_cpcl_media[0]));
    memcpy(data->media, lprint_cpcl_media, sizeof(lprint_cpcl_media));

    papplCopyString(data->media_ready[0].size_name, "na_index-4x6_4x6in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
  }

  data->bottom_top = data->left_right = 1;

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->tracking_supported = PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_WEB | PAPPL_MEDIA_TRACKING_CONTINUOUS;

  data->num_type = 3;
  data->type[0]  = "continuous";
  data->type[1]  = "labels";
  data->type[2]  = "labels-continuous";

  data->tear_offset_configured   = 0;
  data->tear_offset_supported[0] = -1500;
  data->tear_offset_supported[1] = 1500;

  data->speed_default      = 0;
  data->speed_supported[0] = 2540;
  data->speed_supported[1] = 4 * 2540;

  data->darkness_configured = 50;
  data->darkness_supported  = 30;

  return (true);
}


//
// 'lprint_cpcl_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{

  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  // Copy the raw file...
  papplJobSetImpressions(job, 1);

  if ((fd  = open(papplJobGetFilename(job), O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open print file '%s': %s", papplJobGetFilename(job), strerror(errno));
    return (false);
  }

  // Copy print data...
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
// 'lprint_cpcl_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_cpcl_t		*cpcl = (lprint_cpcl_t *)papplJobGetData(job);
					// CPCL driver data

  (void)options;
  (void)device;

  free(cpcl);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_cpcl_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_cpcl_t	*cpcl = (lprint_cpcl_t *)papplJobGetData(job);
					// CPCL driver data
  int	darkness;			// Composite darkness value


  (void)page;

  // Write last line
  lprint_cpcl_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Set options
  papplDevicePrintf(device, "PRESENT-AT %d 4\r\n", options->media.top_offset * options->printer_resolution[1] / 2540);

  if ((darkness = options->print_darkness + options->darkness_configured) < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  papplDevicePrintf(device, "TONE %d\r\n", 2 * darkness);

  if (options->print_speed > 0)
    papplDevicePrintf(device, "SPEED %d\r\n", 5 * options->print_speed / (4 * 2540));

  if (options->finishings & PAPPL_FINISHINGS_TRIM)
    papplDevicePuts(device, "CUT\r\n");

  if (options->media.type[0] && strcmp(options->media.type, "labels"))
  {
    // Continuous media, so always set tracking to continuous...
    options->media.tracking = PAPPL_MEDIA_TRACKING_CONTINUOUS;
  }

  if (options->media.tracking != PAPPL_MEDIA_TRACKING_CONTINUOUS)
    papplDevicePuts(device, "FORM\r\n");

  // Eject
  papplDevicePuts(device, "PRINT\r\n");
  papplDeviceFlush(device);

  // Free memory and return...
  lprintDitherFree(&cpcl->dither);

  return (true);
}


//
// 'lprint_cpcl_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_cpcl_t		*cpcl = (lprint_cpcl_t *)calloc(1, sizeof(lprint_cpcl_t));
					// CPCL driver data


  (void)options;
  (void)device;

  // Save driver data...
  papplJobSetData(job, cpcl);

  return (true);
}


//
// 'lprint_cpcl_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_cpcl_t	*cpcl = (lprint_cpcl_t *)papplJobGetData(job);
					// CPCL driver data


  (void)page;

  // Initialize the dither buffer...
  if (!lprintDitherAlloc(&cpcl->dither, job, options, CUPS_CSPACE_W, options->header.HWResolution[0] == 300 ? 1.2 : 1.0))
    return (false);

  // Initialize the printer...
  papplDevicePrintf(device, "! 0 %u %u %u %u\r\n", options->header.HWResolution[0], options->header.HWResolution[1], options->header.cupsHeight, options->header.NumCopies ? options->header.NumCopies : 1);
  papplDevicePrintf(device, "PAGE-WIDTH %u\r\n", options->header.cupsWidth);
  papplDevicePrintf(device, "PAGE-HEIGHT %u\r\n", options->header.cupsHeight);


  // Start the page image...
  papplDevicePuts(device, "CLS\n");
  papplDevicePrintf(device, "BITMAP 0,0,%u,%u,1,", cpcl->dither.out_width, options->header.cupsHeight);

  return (true);
}


//
// 'lprint_cpcl_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_cpcl_t		*cpcl = (lprint_cpcl_t *)papplJobGetData(job);
					// CPCL driver data


  (void)options;

  // Dither and write the line...
  if (lprintDitherLine(&cpcl->dither, y, line))
  {
    if (cpcl->dither.output[0] || memcmp(cpcl->dither.output, cpcl->dither.output + 1, cpcl->dither.out_width - 1))
    {
      papplDevicePrintf(device, "CG %u 1 0 %d ", (unsigned)cpcl->dither.out_width, y);
      papplDeviceWrite(device, cpcl->dither.output, cpcl->dither.out_width);
      papplDevicePuts(device, "\r\n");
      papplDeviceFlush(device);
    }
  }

  return (true);
}


//
// 'lprint_cpcl_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_cpcl_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
#endif // LPRINT_EXPERIMENTAL
