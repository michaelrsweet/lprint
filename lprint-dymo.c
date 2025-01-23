//
// DYMO driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2025 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2001-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


//
// Local types...
//

typedef enum lprint_dlang_e
{
  LPRINT_DLANG_LABEL,			// Label printing
  LPRINT_DLANG_TAPE			// Tape printing
} lprint_dlang_t;

typedef struct lprint_dymo_s		// DYMO driver data
{
  lprint_dlang_t dlang;			// Printer language
  lprint_dither_t dither;		// Dithering buffer
  int		feed,			// Accumulated feed
		min_leader,		// Leader distance for cut
		normal_leader;		// Leader distance for top of label
} lprint_dymo_t;


//
// Local globals...
//

static const char * const lprint_dymo_label[] =
{					// Supported media sizes for labels
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
  "oe_address-label_1.125x3.5in",
  "oe_internet-postage-label_1.25x1.625in",
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
  "om_diskette-label_54x70mm",

  "roll_max_2.3125x3600in",
  "roll_min_0.25x0.25in"
};

static const char * const lprint_dymo_tape[] =
{					// Supported media sizes for tape
  "oe_thin-1in-tape_0.25x1in",
  "oe_thinner-1in-tape_0.375x1in",
  "oe_medium-1in-tape_0.5x1in",
  "oe_wider-1in-tape_0.75x1in",
  "oe_wide-1in-tape_1x1in",

  "oe_thin-2in-tape_0.25x2in",
  "oe_thinner-2in-tape_0.375x2in",
  "oe_medium-2in-tape_0.5x2in",
  "oe_wider-2in-tape_0.75x2in",
  "oe_wide-2in-tape_1x2in",

  "oe_thin-3in-tape_0.25x3in",
  "oe_thinner-3in-tape_0.375x3in",
  "oe_medium-3in-tape_0.5x3in",
  "oe_wider-3in-tape_0.75x3in",
  "oe_wide-3in-tape_1x3in",

  "roll_max_1x3600in",
  "roll_min_0.25x1in"
};


//
// Local functions...
//

static void	lprint_dymo_init(pappl_job_t *job, lprint_dymo_t *dymo);
static bool	lprint_dymo_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_dymo_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_dymo_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_dymo_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_dymo_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_dymo_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_dymo_status(pappl_printer_t *printer);


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
  data->printfile_cb  = lprint_dymo_printfile;
  data->rendjob_cb    = lprint_dymo_rendjob;
  data->rendpage_cb   = lprint_dymo_rendpage;
  data->rstartjob_cb  = lprint_dymo_rstartjob;
  data->rstartpage_cb = lprint_dymo_rstartpage;
  data->rwriteline_cb = lprint_dymo_rwriteline;
  data->status_cb     = lprint_dymo_status;

  if (!strncmp(driver_name, "dymo_lm-", 8) || strstr(driver_name, "-tape"))
  {
    // Vendor-specific format...
    data->format = "application/vnd.dymo-lm";

    // Set pages-per-minute based on 3" of tape; not exact but
    // we need to report something...
    data->ppm = 20;

    // Tape printers operate at 180dpi
    data->num_resolution  = 1;
    data->x_resolution[0] = 180;
    data->y_resolution[0] = 180;

    data->x_default = data->y_default = 180;

    data->left_right = 1;
    data->bottom_top = 1;

    data->num_media = (int)(sizeof(lprint_dymo_tape) / sizeof(lprint_dymo_tape[0]));
    memcpy(data->media, lprint_dymo_tape, sizeof(lprint_dymo_tape));

    data->num_source = 1;
    data->source[0]  = "main-roll";

    papplCopyString(data->media_ready[0].size_name, "oe_wide-2in-tape_1x2in", sizeof(data->media_ready[0].size_name));
  }
  else
  {
    // Vendor-specific format...
    data->format = "application/vnd.dymo-lw";

    // Set pages-per-minute based on 1.125x3.5" address labels; not exact but
    // we need to report something...
    if (strstr(driver_name, "-turbo"))
      data->ppm = 60;
    else
      data->ppm = 30;

    // Label printers operate at 300dpi
    data->num_resolution  = 1;
    data->x_resolution[0] = 300;
    data->y_resolution[0] = 300;

    data->x_default = data->y_default = 300;

    // Media...
    data->left_right = 100;
    data->bottom_top = 525;

    data->num_media = (int)(sizeof(lprint_dymo_label) / sizeof(lprint_dymo_label[0]));
    memcpy(data->media, lprint_dymo_label, sizeof(lprint_dymo_label));

    if (strstr(driver_name, "-twin"))
    {
      data->num_source = 2;
      data->source[0]  = "main-roll";
      data->source[1]  = "alternate-roll";

      papplCopyString(data->media_ready[0].size_name, "oe_address-label_1.125x3.5in", sizeof(data->media_ready[0].size_name));
      papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
      papplCopyString(data->media_ready[1].size_name, "oe_address-label_1.125x3.5in", sizeof(data->media_ready[1].size_name));
      papplCopyString(data->media_ready[1].type, "labels", sizeof(data->media_ready[0].type));
    }
    else
    {
      data->num_source = 1;
      data->source[0]  = "main-roll";

      papplCopyString(data->media_ready[0].size_name, "oe_address-label_1.125x3.5in", sizeof(data->media_ready[0].size_name));
      papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
    }
  }

  data->tracking_supported = PAPPL_MEDIA_TRACKING_WEB;

  data->num_type = 1;
  data->type[0]  = "labels";

  // Darkness/density support...
  data->darkness_configured = 50;
  data->darkness_supported  = 4;

  return (true);
}


//
// 'lprint_dymo_init()' - Initialize DYMO driver data based on the driver name...
//

static void
lprint_dymo_init(
    pappl_job_t   *job,			// I - Job
    lprint_dymo_t *dymo)		// O - Driver data
{
  const char	*driver_name;		// Driver name


  driver_name = papplPrinterGetDriverName(papplJobGetPrinter(job));

  if (!strncmp(driver_name, "dymo_lm-", 8) || strstr(driver_name, "-tape"))
  {
    dymo->dlang = LPRINT_DLANG_TAPE;

    if (!strcmp(driver_name, "dymo_lw-duo-tape") || !strcmp(driver_name, "dymo_lw-duo-tape-128") || !strcmp(driver_name, "dymo_lw-450-duo-tape"))
    {
      dymo->min_leader    = 61;
      dymo->normal_leader = 14;
    }
    else if (!strcmp(driver_name, "dymo_lm-pnp"))
    {
      dymo->min_leader    = 58;
      dymo->normal_leader = 17;
    }
    else
    {
      dymo->min_leader    = 55;
      dymo->normal_leader = 20;
    }
  }
  else
  {
    dymo->dlang = LPRINT_DLANG_LABEL;
  }
}


//
// 'lprint_dymo_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer
  lprint_dymo_t	dymo;			// Driver data


  // Initialize driver data...
  lprint_dymo_init(job, &dymo);

  // Reset the printer...
  lprint_dymo_rstartjob(job, options, device);

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

  lprint_dymo_rstartjob(job, options, device);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'lprint_dymo_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_dymo_t		*dymo = (lprint_dymo_t *)papplJobGetData(job);
					// DYMO driver data

  (void)options;

  free(dymo);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_dymo_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_dymo_t	*dymo = (lprint_dymo_t *)papplJobGetData(job);
					// DYMO driver data
  char		buffer[256];		// Command buffer


  (void)page;

  lprint_dymo_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  switch (dymo->dlang)
  {
    case LPRINT_DLANG_LABEL :
        break;

    case LPRINT_DLANG_TAPE :
	// Skip and cut...
        papplDevicePrintf(device, "\033D%c", 0);
        memset(buffer, 0x16, dymo->min_leader);
        papplDeviceWrite(device, buffer, dymo->min_leader);
        break;
  }

  // Eject/cut
  papplDevicePuts(device, "\033E");
  papplDeviceFlush(device);

  // Free memory and return...
  lprintDitherFree(&dymo->dither);

  return (true);
}


//
// 'lprint_dymo_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_dymo_t		*dymo = (lprint_dymo_t *)calloc(1, sizeof(lprint_dymo_t));
					// DYMO driver data
  char			buffer[23];	// Buffer for reset command


  (void)options;

  // Initialize driver data...
  lprint_dymo_init(job, dymo);

  papplJobSetData(job, dymo);

  // Reset the printer...
  switch (dymo->dlang)
  {
    case LPRINT_DLANG_LABEL :
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
        break;

    case LPRINT_DLANG_TAPE :
        // Send nul bytes to clear input buffer...
        memset(buffer, 0, sizeof(buffer));
        papplDeviceWrite(device, buffer, sizeof(buffer));

        // Set tape color to black on white...
        papplDevicePrintf(device, "\033C%c", 0);
        break;
  }


  return (true);
}


//
// 'lprint_dymo_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  pappl_pr_driver_data_t data;		// Generic driver data
  lprint_dymo_t	*dymo = (lprint_dymo_t *)papplJobGetData(job);
					// DYMO driver data
  int		darkness = options->darkness_configured + options->print_darkness;
					// Combined density
  const char	*density = "cdeg";	// Density codes
  int		i;			// Looping var
  char		buffer[256];		// Command buffer
  double	out_gamma = 1.0;	// Output gamma correction



  (void)page;

  if (options->header.cupsWidth > 2048)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Raster data too large for printer.");
    return (false);
  }

  if (options->header.HWResolution[0] == 300)
    out_gamma = 1.2;

  if (!lprintDitherAlloc(&dymo->dither, job, options, CUPS_CSPACE_K, out_gamma))
    return (false);

  dymo->feed = 0;

  switch (dymo->dlang)
  {
    case LPRINT_DLANG_LABEL :
	papplDevicePrintf(device, "\033Q%c%c", 0, 0);
	papplDevicePrintf(device, "\033B%c", 0);
	papplDevicePrintf(device, "\033L%c%c", options->header.cupsHeight >> 8, options->header.cupsHeight);
	papplDevicePrintf(device, "\033D%c", dymo->dither.out_width);

	papplPrinterGetDriverData(papplJobGetPrinter(job), &data);

	// Match roll number to loaded media...
	for (i = 0; i < data.num_source; i ++)
	{
	  if (data.media_ready[i].size_width == options->media.size_width && data.media_ready[i].size_length == options->media.size_length)
	    break;
	}

	if (i >= data.num_source)
	{
	  // No match, so use what the client sent...
	  i = !strcmp(options->media.source, "alternate-roll");
	}

	papplDevicePrintf(device, "\033q%d", i + 1);

	if (darkness < 0)
	  darkness = 0;
	else if (darkness > 100)
	  darkness = 100;

	papplDevicePrintf(device, "\033%c", density[3 * darkness / 100]);
	break;

    case LPRINT_DLANG_TAPE :
        // Set line width...
        papplDevicePrintf(device, "\033D%c", 0);

        // Feed for the leader...
	memset(buffer, 0x16, dymo->normal_leader);
	papplDeviceWrite(device, buffer, dymo->normal_leader);

        // Set indentation...
        papplDevicePrintf(device, "\033B%c", 0);
        break;
  }

  return (true);
}


//
// 'lprint_dymo_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_dymo_t		*dymo = (lprint_dymo_t *)papplJobGetData(job);
					// DYMO driver data
  unsigned char		byte;		// Byte to write


  if (!lprintDitherLine(&dymo->dither, y, line))
    return (true);

  if (dymo->dither.output[0] || memcmp(dymo->dither.output, dymo->dither.output + 1, dymo->dither.out_width - 1))
  {
    // Not a blank line
    switch (dymo->dlang)
    {
      case LPRINT_DLANG_LABEL :
	  // Feed for any prior blank lines...
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
	  byte = 0x16;
	  papplDeviceWrite(device, &byte, 1);
	  papplDeviceWrite(device, dymo->dither.output, dymo->dither.out_width);
	  break;

      case LPRINT_DLANG_TAPE :
	  if (dymo->feed)
	  {
	    unsigned char buffer[256];	// Write buffer

            papplDevicePrintf(device, "\033D%c", 0);
	    memset(buffer, 0x16, sizeof(buffer));
	    while (dymo->feed > 255)
	    {
	      papplDeviceWrite(device, buffer, sizeof(buffer));
	      dymo->feed -= 256;
	    }

            if (dymo->feed > 0)
            {
	      papplDeviceWrite(device, buffer, dymo->feed);
	      dymo->feed = 0;
	    }
	  }
	  papplDevicePrintf(device, "\033D%c\026", dymo->dither.out_width);
	  papplDeviceWrite(device, dymo->dither.output, dymo->dither.out_width);
          break;
    }
  }
  else
  {
    // Blank line, accumulate the feed...
    dymo->feed ++;
  }

  return (true);
}


//
// 'lprint_dymo_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_dymo_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
