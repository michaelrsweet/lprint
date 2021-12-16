//
// EPL2 driver for LPrint, a Label Printer Application
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

static const char * const lprint_epl2_2inch_media[] =
{					// Supported 2 inch media sizes
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

  "roll_max_2x100in",
  "roll_min_0.75x0.25in"
};
static const char * const lprint_epl2_4inch_media[] =
{					// Supported 4 inch media sizes
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

  "oe_2.25x0.5-label_2.25xin",
  "oe_2.25x1.25-label_2.25xin",
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
  "oe_4x13-label_4x13in",

/*  "oe_6x1-label_6x1in",
  "oe_6x2-label_6x2in",
  "oe_6x3-label_6x3in",
  "oe_6x4-label_6x4in",
  "oe_6x5-label_6x5in",
  "oe_6x6-label_6x6in",
  "oe_6x6.5-label_6x6.5in",
  "oe_6x13-label_6x13in",

  "oe_8x1-label_8x1in",
  "oe_8x2-label_8x2in",
  "oe_8x3-label_8x3in",
  "oe_8x4-label_8x4in",
  "oe_8x5-label_8x5in",
  "oe_8x6-label_8x6in",
  "oe_8x6.5-label_8x6.5in",
  "oe_8x13-label_8x13in",*/

  "roll_max_4x100in",
  "roll_min_0.75x0.25in"
};


//
// Local functions...
//

static bool	lprint_epl2_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_epl2_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_epl2_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_epl2_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_epl2_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_epl2_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_epl2_status(pappl_printer_t *printer);


//
// 'lprintInitEPL2()' - Initialize the driver.
//

bool					// O - `true` on success, `false` on error
lprintEPL2(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = lprint_epl2_printfile;
  data->rendjob_cb    = lprint_epl2_rendjob;
  data->rendpage_cb   = lprint_epl2_rendpage;
  data->rstartjob_cb  = lprint_epl2_rstartjob;
  data->rstartpage_cb = lprint_epl2_rstartpage;
  data->rwriteline_cb = lprint_epl2_rwriteline;
  data->status_cb     = lprint_epl2_status;
  data->format        = "application/vnd.eltron-epl";

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

  if (strstr(driver_name, "-cutter"))
    data->finishings |= PAPPL_FINISHINGS_TRIM;

  if (!strncmp(driver_name, "epl2_2inch-", 16))
  {
    // 2 inch printer...
    data->num_media = (int)(sizeof(lprint_epl2_2inch_media) / sizeof(lprint_epl2_2inch_media[0]));
    memcpy(data->media, lprint_epl2_2inch_media, sizeof(lprint_epl2_2inch_media));

    strlcpy(data->media_default.size_name, "oe_2x3-label_2x3in", sizeof(data->media_default.size_name));
  }
  else
  {
    // 4 inch printer...
    data->num_media = (int)(sizeof(lprint_epl2_4inch_media) / sizeof(lprint_epl2_4inch_media[0]));
    memcpy(data->media, lprint_epl2_4inch_media, sizeof(lprint_epl2_4inch_media));

    strlcpy(data->media_default.size_name, "oe_4x6-label_4x6in", sizeof(data->media_default.size_name));
  }

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->num_type = 3;
  data->type[0]  = "continuous";
  data->type[1]  = "labels";
  data->type[2]  = "labels-continuous";

  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;
  strlcpy(data->media_default.source, "main-roll", sizeof(data->media_default.source));
  data->media_default.top_margin = data->bottom_top;
  data->media_default.top_offset = 0;
  data->media_default.tracking   = PAPPL_MEDIA_TRACKING_MARK;
  strlcpy(data->media_default.type, "labels", sizeof(data->media_default.type));

  data->media_ready[0] = data->media_default;

  data->mode_configured = PAPPL_LABEL_MODE_TEAR_OFF;
  data->mode_configured = PAPPL_LABEL_MODE_APPLICATOR | PAPPL_LABEL_MODE_CUTTER | PAPPL_LABEL_MODE_CUTTER_DELAYED | PAPPL_LABEL_MODE_KIOSK | PAPPL_LABEL_MODE_PEEL_OFF | PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL | PAPPL_LABEL_MODE_REWIND | PAPPL_LABEL_MODE_RFID | PAPPL_LABEL_MODE_TEAR_OFF;

  data->speed_default      = 0;
  data->speed_supported[0] = 2540;
  data->speed_supported[1] = 6 * 2540;

  data->darkness_configured = 50;
  data->darkness_supported  = 30;

  return (true);
}


//
// 'lprint_epl2_print()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_printfile(
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
// 'lprint_epl2_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}


//
// 'lprint_epl2_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  (void)job;
  (void)page;

  papplDevicePuts(device, "P1\n");

  if (options->finishings & PAPPL_FINISHINGS_TRIM)
    papplDevicePuts(device, "C\n");

  return (true);
}


//
// 'lprint_epl2_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}


//
// 'lprint_epl2_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  int		ips;			// Inches per second


  (void)job;
  (void)page;

  papplDevicePuts(device, "\nN\n");

  // print-darkness
  papplDevicePrintf(device, "D%d\n", 15 * options->print_darkness / 100);

  // print-speed
  if ((ips = options->print_speed / 2540) > 0)
    papplDevicePrintf(device, "S%d\n", ips);

  // Set label size...
  papplDevicePrintf(device, "q%u\n", (options->header.cupsWidth + 7) & ~7U);

  return (true);
}


//
// 'lprint_epl2_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    unsigned		i;		// Looping var
    const unsigned char	*lineptr;	// Pointer into line
    unsigned char	buffer[300],	// Buffer (big enough for 8" at 300dpi)
			*bufptr;	// Pointer into buffer

    for (i = options->header.cupsBytesPerLine, lineptr = line, bufptr = buffer; i > 0; i --)
      *bufptr++ = ~*lineptr++;

    papplDevicePrintf(device, "GW0,%u,%u,1\n", y, options->header.cupsBytesPerLine);
    papplDeviceWrite(device, buffer, options->header.cupsBytesPerLine);
    papplDevicePuts(device, "\n");
  }

  return (true);
}


//
// 'lprint_epl2_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_epl2_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
