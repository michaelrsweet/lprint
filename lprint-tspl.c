//
// TSPL driver for LPrint, a Label Printer Application
//
// Copyright © 2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


//
// Local types...
//

typedef struct lprint_tspl_s		// tspl driver data
{
  lprint_dither_t dither;		// Dither buffer
} lprint_tspl_t;


//
// Local globals...
//

static const char * const lprint_tspl_media[] =
{					// Supported media sizes for labels
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
  "na_index-3x5_3x5in",

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
  "na_index-4x6_4x6in",
  "oe_4x6.5-label_4x6.5in",
  "oe_4x8-label_4x8in",
  "oe_4x13-label_4x13in",

  "roll_max_4x39.6in",
  "roll_min_0.75x0.25in"
};


//
// Local functions...
//

static bool	lprint_tspl_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_tspl_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_tspl_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_tspl_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_tspl_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_tspl_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_tspl_status(pappl_printer_t *printer);


//
// 'lprintTSPL()' - Initialize the TSPL driver.
//

bool					// O - `true` on success, `false` on error
lprintTSPL(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  int	i;				// Looping var


  // Print callbacks...
  data->printfile_cb  = lprint_tspl_printfile;
  data->rendjob_cb    = lprint_tspl_rendjob;
  data->rendpage_cb   = lprint_tspl_rendpage;
  data->rstartjob_cb  = lprint_tspl_rstartjob;
  data->rstartpage_cb = lprint_tspl_rstartpage;
  data->rwriteline_cb = lprint_tspl_rwriteline;
  data->status_cb     = lprint_tspl_status;

  // Vendor-specific format...
  data->format = LPRINT_TSPL_MIMETYPE;

  // Set resolution...
  data->num_resolution = 1;

  if (strstr(driver_name, "_203dpi") != NULL)
    data->x_resolution[0] =  data->y_resolution[0] = 203;
  else
    data->x_resolution[0] =  data->y_resolution[0] = 300;

  data->x_default = data->y_default = data->x_resolution[0];

  // Basically borderless...
  data->left_right = 1;
  data->bottom_top = 1;

  // Supported media...
  data->num_media = (int)(sizeof(lprint_tspl_media) / sizeof(lprint_tspl_media[0]));
  memcpy(data->media, lprint_tspl_media, sizeof(lprint_tspl_media));

  data->num_source = 1;
  data->source[0]  = "main-roll";

  papplCopyString(data->media_ready[0].size_name, "na_index-4x6_4x6in", sizeof(data->media_ready[0].size_name));
  papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));

  data->num_type = 1;
  data->type[0]  = "labels";

  // Darkness/density settings...
  data->darkness_configured = 50;
  data->darkness_supported  = 16;

  return (true);
}


//
// 'lprint_tspl_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_printfile(
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
// 'lprint_tspl_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_tspl_t		*tspl = (lprint_tspl_t *)papplJobGetData(job);
					// TSPL driver data

  (void)options;
  (void)device;

  free(tspl);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_tspl_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_tspl_t	*tspl = (lprint_tspl_t *)papplJobGetData(job);
					// TSPL driver data


  (void)page;

  // Write last line
  lprint_tspl_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Eject
  papplDevicePrintf(device, "PRINT %u,1\n", options->header.NumCopies);
  papplDeviceFlush(device);

  // Free memory and return...
  lprintDitherFree(&tspl->dither);

  return (true);
}


//
// 'lprint_tspl_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_tspl_t		*tspl = (lprint_tspl_t *)calloc(1, sizeof(lprint_tspl_t));
					// TSPL driver data


  (void)options;
  (void)device;

  // Save driver data...
  papplJobSetData(job, tspl);

  return (true);
}


//
// 'lprint_tspl_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_tspl_t	*tspl = (lprint_tspl_t *)papplJobGetData(job);
					// TSPL driver data
  int		darkness,		// Combined density
		speed;			// Print speed


  (void)page;

  // Initialize the dither buffer...
  if (!lprintDitherAlloc(&tspl->dither, job, options, CUPS_CSPACE_W, options->header.HWResolution[0] == 300 ? 1.2 : 1.0))
    return (false);

  // Initialize the printer...
  if ((darkness = options->darkness_configured + options->print_darkness) < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  if ((speed = options->print_speed / 2540) < 1)
    speed = 1;

  papplDevicePrintf(device, "SIZE %d mm,%d mm\n", options->media.size_width / 100, options->media.size_length / 100);

  switch (options->orientation_requested)
  {
    default :
    case IPP_ORIENT_PORTRAIT :
        papplDevicePuts(device, "DIRECTION 0,0\n");
        break;
    case IPP_ORIENT_LANDSCAPE :
        papplDevicePuts(device, "DIRECTION 90,0\n");
        break;
    case IPP_ORIENT_REVERSE_PORTRAIT :
        papplDevicePuts(device, "DIRECTION 180,0\n");
        break;
    case IPP_ORIENT_REVERSE_LANDSCAPE :
        papplDevicePuts(device, "DIRECTION 270,0\n");
        break;
  }

  papplDevicePrintf(device, "DENSITY %d\n", (darkness * 15 + 50) / 100);
  papplDevicePrintf(device, "SPEED %d\n", speed);

  // Start the page image...
  papplDevicePuts(device, "CLS\n");
  papplDevicePrintf(device, "BITMAP 0,0,%u,%u,1,", tspl->dither.out_width, options->header.cupsHeight);

  return (true);
}


//
// 'lprint_tspl_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_tspl_t		*tspl = (lprint_tspl_t *)papplJobGetData(job);
					// TSPL driver data


  (void)options;

  // Dither and write the line...
  if (lprintDitherLine(&tspl->dither, y, line))
    papplDeviceWrite(device, tspl->dither.output, tspl->dither.out_width);

  return (true);
}


//
// 'lprint_tspl_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_tspl_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  // TODO: Implement status polling

  return (true);
}
