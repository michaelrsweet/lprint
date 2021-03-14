//
// Dymo driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2001-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// Local types...
//

typedef struct lprint_dymo_s		// DYMO driver data
{
  unsigned	ystart,			// First line
		yend;			// Last line
  int		feed;			// Accumulated feed
} lprint_dymo_t;


//
// Local globals...
//

static const char * const lprint_dymo_media[] =
{					// Supported media sizes
  "oe_thin-multipurpose-label_0.375x2.8125in",
  "oe_library-barcode-label_0.5x1.875in",
  "oe_hanging-file-tab-insert_0.5625x2in",
  "oe_file-folder-label_0.5625x3.4375in",
  "oe_return-address-label_0.75x2in",
  "oe_barcode-label_0.75x2.5in",
  "oe_video-spine-label_0.75x5.875in",
  "oe_price-tag-label_0.9375x0.875in",
  "oe_square-multipurpose-label_1x1in",
  "oe_book-spine-label_1x1.5in",
  "oe_sm-multipurpose-label_1x2.125in",
  "oe_2-up-file-folder-label_1.125x3.4375in",
  "oe_internet-postage-label_1.25x1.625in",
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_video-top-label_1.8x3.1in",
  "oe_multipurpose-label_2x2.3125in",
  "oe_md-appointment-card_2x3.5in",
  "oe_lg-multipurpose-label_2.125x.75in",
  "oe_shipping-label_2.125x4in",
  "oe_continuous-label_2.125x3600in",
  "oe_md-multipurpose-label_2.25x1.25in",
  "oe_media-label_2.25x2.25in",
  "oe_2-up-address-label_2.25x3.5in",
  "oe_name-badge-label_2.25x4in",
  "oe_3-part-postage-label_2.25x7in",
  "oe_2-part-internet-postage-label_2.25x7.5in",
  "oe_shipping-label_2.3125x4in",
  "oe_internet-postage-label_2.3125x7in",
  "oe_internet-postage-confirmation-label_2.3125x10.5in",

  "roll_max_2.3125x3600in",
  "roll_min_0.25x0.25in"
};


//
// Local functions...
//

static int	lprint_dymo_print(pappl_job_t *job, pappl_pr_options_t *options);
static int	lprint_dymo_rendjob(pappl_job_t *job, pappl_pr_options_t *options);
static int	lprint_dymo_rendpage(pappl_job_t *job, pappl_pr_options_t *options, unsigned page);
static int	lprint_dymo_rstartjob(pappl_job_t *job, pappl_pr_options_t *options);
static int	lprint_dymo_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, unsigned page);
static int	lprint_dymo_rwrite(pappl_job_t *job, pappl_pr_options_t *options, unsigned y, const unsigned char *line);
static int	lprint_dymo_status(lprint_printer_t *printer);


//
// 'lprintDYMO()' - Initialize the DYMO driver.
//

bool					// O - `true` on success, `false` on error
lprintDYMO(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  int	i;				// Looping var


  data->print      = lprint_dymo_print;
  data->rendjob    = lprint_dymo_rendjob;
  data->rendpage   = lprint_dymo_rendpage;
  data->rstartjob  = lprint_dymo_rstartjob;
  data->rstartpage = lprint_dymo_rstartpage;
  data->rwrite     = lprint_dymo_rwrite;
  data->status     = lprint_dymo_status;
  data->format     = "application/vnd.dymo-lw";

  data->num_resolution  = 1;
  data->x_resolution[0] = 300;
  data->y_resolution[0] = 300;

  data->left_right = 100;
  data->bottom_top = 525;

  data->num_media = (int)(sizeof(lprint_dymo_media) / sizeof(lprint_dymo_media[0]));
  memcpy(data->media, lprint_dymo_media, sizeof(lprint_dymo_media));

  if (strstr(data->name, "-duo") || strstr(data->name, "-twin"))
  {
    data->num_source = 2;
    data->source[0]  = "main-roll";
    data->source[1]  = "alternate-roll";

    strlcpy(data->media_ready[0].size_name, "oe_multipurpose-label_2x2.3125in", sizeof(data->media_ready[0].size_name));
    strlcpy(data->media_ready[1].size_name, "oe_address-label_1.25x3.5in", sizeof(data->media_ready[1].size_name));
  }
  else
  {
    data->num_source = 1;
    data->source[0]  = "main-roll";
    strlcpy(data->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(data->media_ready[0].size_name));
  }

  data->tracking_supported = PAPPL_MEDIA_TRACKING_WEB;

  data->num_type = 1;
  data->type[0]  = "labels";

  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;
  data->media_default.size_width    = 3175;
  data->media_default.size_length   = 8890;
  data->media_default.top_margin    = data->bottom_top;
  data->media_default.tracking      = PAPPL_MEDIA_TRACKING_WEB;
  strlcpy(data->media_default.size_name, "oe_address-label_1.25x3.5in", sizeof(data->media_default.size_name));
  strlcpy(data->media_default.source, data->source[0], sizeof(data->media_default.source));
  strlcpy(data->media_default.type, data->type[0], sizeof(data->media_default.type));

  for (i = 0; i < data->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(data->media_ready[i].size_name);

    data->media_ready[i].bottom_margin = data->bottom_top;
    data->media_ready[i].left_margin   = data->left_right;
    data->media_ready[i].right_margin  = data->left_right;
    data->media_ready[i].size_width    = pwg->width;
    data->media_ready[i].size_length   = pwg->length;
    data->media_ready[i].top_margin    = data->bottom_top;
    data->media_ready[i].tracking      = PAPPL_MEDIA_TRACKING_WEB;
    strlcpy(data->media_ready[i].source, data->source[i], sizeof(data->media_ready[i].source));
    strlcpy(data->media_ready[i].type, data->type[0], sizeof(data->media_ready[i].type));
  }

  data->darkness_configured = 50;
  data->darkness_supported  = 4;

  data->num_supply = 0;

  return (true);
}


//
// 'lprint_dymo_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_print(
    pappl_job_t     *job,		// I - Job
    pappl_pr_options_t *options)		// I - Job options
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  int		infd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  // Reset the printer...
  papplDevicePuts(device, "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033@");

  // Copy the raw file...
  job->impressions = 1;

  infd  = open(job->filename, O_RDONLY);

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(infd);
      return (0);
    }
  }
  close(infd);

  job->impcompleted = 1;

  return (1);
}


//
// 'lprint_dymo_rend()' - End a job.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rendjob(
    pappl_job_t     *job,		// I - Job
    pappl_pr_options_t *options)		// I - Job options
{
  lprint_driver_t	*driver = job->printer->driver;
					// Driver data


  (void)options;

  free(driver->job_data);
  driver->job_data = NULL;

  return (1);
}


//
// 'lprint_dymo_rendpage()' - End a page.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rendpage(
    pappl_job_t     *job,		// I - Job
    pappl_pr_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device


  (void)options;
  (void)page;

  papplDevicePuts(device, "\033E");

  return (1);
}


//
// 'lprint_dymo_rstartjob()' - Start a job.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rstartjob(
    pappl_job_t     *job,		// I - Job
    pappl_pr_options_t *options)		// I - Job options
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)calloc(1, sizeof(lprint_dymo_t));
					// DYMO driver data


  (void)options;

  job->printer->driver->job_data = dymo;

  papplDevicePuts(device, "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033@");

  return (1);
}


//
// 'lprint_dymo_rstartpage()' - Start a page.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rstartpage(
    pappl_job_t     *job,		// I - Job
    pappl_pr_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_driver_t	*driver = job->printer->driver;
					// Driver
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)job->printer->driver->job_data;
					// DYMO driver data
  int			darkness = job->printer->driver->darkness_configured + options->print_darkness;
  const char		*density = "cdeg";
					// Density codes


  (void)page;

  if (options->header.cupsBytesPerLine > 256)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Raster data too large for printer.");
    return (0);
  }

  papplDevicePrintf(device, "\033Q%c%c", 0, 0);
  papplDevicePrintf(device, "\033B%c", 0);
  papplDevicePrintf(device, "\033L%c%c", options->header.cupsHeight >> 8, options->header.cupsHeight);
  papplDevicePrintf(device, "\033D%c", options->header.cupsBytesPerLine - 1);
  papplDevicePrintf(device, "\033q%d", !strcmp(options->media.source, "alternate-roll") ? 2 : 1);

  if (darkness < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  papplDevicePrintf(device, "\033%c", density[3 * darkness / 100]);

  dymo->feed   = 0;
  dymo->ystart = driver->bottom_top * options->printer_resolution[1] / 2540;
  dymo->yend   = options->header.cupsHeight - dymo->ystart;

  return (1);
}


//
// 'lprint_dymo_rwrite()' - Write a raster line.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rwrite(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t    *options,	// I - Job options
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)job->printer->driver->job_data;
					// DYMO driver data
  unsigned char		buffer[256];	// Write buffer


  if (y < dymo->ystart || y >= dymo->yend)
    return (1);

  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    // Not a blank line, feed for any prior blank lines...
    if (dymo->feed)
    {
      while (dymo->feed > 255)
      {
	papplDevicePrintf(device, "\033f\001%c", 255);
	dymo->feed -= 255;
      }

      papplDevicePrintf(device, "\033f\001%c", dymo->feed);
      dymo->feed = 0;
    }

    // Then write the non-blank line...
    buffer[0] = 0x16;
    memcpy(buffer + 1, line + 1, options->header.cupsBytesPerLine - 1);
    papplDeviceWrite(device, buffer, options->header.cupsBytesPerLine);
  }
  else
  {
    // Blank line, accumulate the feed...
    dymo->feed ++;
  }

  return (1);
}


//
// 'lprint_dymo_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
