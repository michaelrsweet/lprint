//
// SEIKO driver for LPrint, a Label Printer Application
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

typedef struct lprint_seiko_s		// SEIKO driver data
{
  lprint_dither_t dither;		// Dither buffer
} lprint_seiko_t;


//
// Local globals...
//

static const char * const lprint_seiko_media[] =
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

static void	lprint_seiko_init(pappl_job_t *job, lprint_seiko_t *seiko);
static bool	lprint_seiko_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_seiko_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_seiko_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_seiko_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_seiko_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_seiko_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_seiko_status(pappl_printer_t *printer);


//
// 'lprintSEIKO()' - Initialize the SEIKO driver.
//

bool					// O - `true` on success, `false` on error
lprintSEIKO(
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
  data->printfile_cb  = lprint_seiko_printfile;
  data->rendjob_cb    = lprint_seiko_rendjob;
  data->rendpage_cb   = lprint_seiko_rendpage;
  data->rstartjob_cb  = lprint_seiko_rstartjob;
  data->rstartpage_cb = lprint_seiko_rstartpage;
  data->rwriteline_cb = lprint_seiko_rwriteline;
  data->status_cb     = lprint_seiko_status;

  // Vendor-specific format...
  data->format = "application/vnd.tsc-seiko";

  // Set pages-per-minute based on 4x6 labels...
  data->ppm = 60;

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
  data->num_media = (int)(sizeof(lprint_seiko_media) / sizeof(lprint_seiko_media[0]));
  memcpy(data->media, lprint_seiko_media, sizeof(lprint_seiko_media));

  data->num_source = 1;
  data->source[0]  = "main-roll";

  papplCopyString(data->media_ready[0].size_name, "na_index-4x6_4x6in", sizeof(data->media_ready[0].size_name));

  data->num_type = 1;
  data->type[0]  = "labels";

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

  // Darkness/density settings...
  data->darkness_configured = 50;
  data->darkness_supported  = 16;

  return (true);
}


//
// 'lprint_seiko_init()' - Initialize seiko driver data based on the driver name...
//

static void
lprint_seiko_init(
    pappl_job_t   *job,			// I - Job
    lprint_seiko_t *seiko)		// O - Driver data
{
}


//
// 'lprint_seiko_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer
  lprint_seiko_t	seiko;			// Driver data


  // Initialize driver data...
  lprint_seiko_init(job, &seiko);

  // Reset the printer...
  lprint_seiko_rstartjob(job, options, device);

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

  lprint_seiko_rstartjob(job, options, device);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'lprint_seiko_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_seiko_t		*seiko = (lprint_seiko_t *)papplJobGetData(job);
					// SEIKO driver data

  (void)options;

  free(seiko);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_seiko_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_seiko_t	*seiko = (lprint_seiko_t *)papplJobGetData(job);
					// SEIKO driver data


  (void)page;

  // Write last line
  lprint_seiko_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Eject/cut

  // Free memory and return...
  lprintDitherFree(&seiko->dither);

  return (true);
}


//
// 'lprint_seiko_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_seiko_t		*seiko = (lprint_seiko_t *)calloc(1, sizeof(lprint_seiko_t));
					// SEIKO driver data


  (void)options;

  // Initialize driver data...
  lprint_seiko_init(job, seiko);

  papplJobSetData(job, seiko);

  // Reset the printer...


  return (true);
}


//
// 'lprint_seiko_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_seiko_t *seiko = (lprint_seiko_t *)papplJobGetData(job);
					// SEIKO driver data
//  int		darkness = options->darkness_configured + options->print_darkness;
					// Combined density



  (void)page;

  if (!lprintDitherAlloc(&seiko->dither, job, options, CUPS_CSPACE_K, 1.0))
    return (false);

  return (true);
}


//
// 'lprint_seiko_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_seiko_t		*seiko = (lprint_seiko_t *)papplJobGetData(job);
					// SEIKO driver data


  if (!lprintDitherLine(&seiko->dither, y, line))
    return (true);

  return (true);
}


//
// 'lprint_seiko_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_seiko_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
