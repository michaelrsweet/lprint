//
// ESC/POS driver for LPrint, a Label Printer Application
//
// Copyright © 2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


//
// Local types...
//

typedef struct lprint_escpos_s		// ESC/POS driver data
{
  int		max_width;		// Roll width in hundredths of millimeters
  lprint_dither_t dither;		// Dithering buffer
  bool		marked;			// Did we print anything yet?
  int		feed;			// Accumulated feed
  int		num_lines,		// Number of lines in buffer
		max_lines;		// Maximum number of lines that can fit
  unsigned char	buffer[32768];		// Graphics buffer
} lprint_escpos_t;


//
// Local globals...
//

static const char * const lprint_escpos_58mm[] =
{					// Supported media sizes for receipts
  "oe_2.25-receipt_58x1000mm",
  "roll_max_58x656000mm",
  "roll_min_58x10mm"
};

static const char * const lprint_escpos_80mm[] =
{					// Supported media sizes for receipts
  "oe_2.25-receipt_58x1000mm",
  "oe_3.125-receipt_80x1000mm",
  "roll_max_80x656000mm",
  "roll_min_58x10mm"
};


//
// Local functions...
//

static void	lprint_escpos_init(pappl_job_t *job, lprint_escpos_t *escpos);
static bool	lprint_escpos_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_escpos_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_escpos_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_escpos_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_escpos_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_escpos_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_escpos_status(pappl_printer_t *printer);
static bool	lprint_escpos_update_reasons(pappl_printer_t *printer, pappl_job_t *job, pappl_device_t *device);


//
// 'lprintESCPOS()' - Initialize the ESC/POS driver.
//

bool					// O - `true` on success, `false` on error
lprintESCPOS(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = lprint_escpos_printfile;
  data->rendjob_cb    = lprint_escpos_rendjob;
  data->rendpage_cb   = lprint_escpos_rendpage;
  data->rstartjob_cb  = lprint_escpos_rstartjob;
  data->rstartpage_cb = lprint_escpos_rstartpage;
  data->rwriteline_cb = lprint_escpos_rwriteline;
  data->status_cb     = lprint_escpos_status;

  // Vendor-specific format...
  data->format = LPRINT_ESCPOS_MIMETYPE;

  // Set pages-per-minute based on 6" of receipt; we need to report something...
  data->ppm = 60;

  // ESC/POS printers operate at 203.2dpi
  data->num_resolution  = 1;
  data->x_resolution[0] = 203;
  data->y_resolution[0] = 203;

  data->x_default = data->y_default = 203;

  // Only one source/roll...
  data->num_source = 1;
  data->source[0]  = "main-roll";

  // Only one media type...
  data->num_type = 1;
  data->type[0]  = "continuous";

  // Cutter...
  data->finishings = PAPPL_FINISHINGS_TRIM;

  // Model-specific values...
  if (!strncmp(driver_name, "escpos_58mm", 11))
  {
    // 58mm wide printer
    data->left_right = 275;
    data->bottom_top = 0;

    data->num_media = (int)(sizeof(lprint_escpos_58mm) / sizeof(lprint_escpos_58mm[0]));
    memcpy(data->media, lprint_escpos_58mm, sizeof(lprint_escpos_58mm));

    papplCopyString(data->media_ready[0].size_name, "oe_2.25-receipt_58x1000mm", sizeof(data->media_ready[0].size_name));
  }
  else
  {
    // 80mm wide printer
    data->left_right = 400;
    data->bottom_top = 0;

    data->num_media = (int)(sizeof(lprint_escpos_80mm) / sizeof(lprint_escpos_80mm[0]));
    memcpy(data->media, lprint_escpos_80mm, sizeof(lprint_escpos_80mm));

    papplCopyString(data->media_ready[0].size_name, "oe_3.125-receipt_80x1000mm", sizeof(data->media_ready[0].size_name));
  }

  return (true);
}


//
// 'lprint_escpos_init()' - Initialize ESC/POS driver data based on the driver name...
//

static void
lprint_escpos_init(
    pappl_job_t     *job,		// I - Job
    lprint_escpos_t *escpos)		// O - Driver data
{
  const char	*driver_name = papplPrinterGetDriverName(papplJobGetPrinter(job));
					// Driver name


  // Clear the ESC/POS data...
  memset(escpos, 0, sizeof(lprint_escpos_t));

  // Get the maximum width of the print head from the driver name ("escpos_MAXWIDTHmm")...
  escpos->max_width = (int)(100 * strtol(driver_name + 7, NULL, 10));
}


//
// 'lprint_escpos_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer
  lprint_escpos_t escpos;		// Driver data


  // Initialize driver data...
  lprint_escpos_init(job, &escpos);

  // Reset the printer...
  lprint_escpos_rstartjob(job, options, device);

  // Update status...
  lprint_escpos_update_reasons(papplJobGetPrinter(job), job, device);

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

  // Update status...
  lprint_escpos_update_reasons(papplJobGetPrinter(job), job, device);

  // Reset the printer at the end of the job...
  lprint_escpos_rendjob(job, options, device);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'lprint_escpos_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_escpos_t *escpos = (lprint_escpos_t *)papplJobGetData(job);
					// ESC/POS driver data
  bool		ret;			// Return value


  (void)options;

  free(escpos);
  papplJobSetData(job, NULL);

  // Reset the printer...
  ret = papplDevicePuts(device, "\033@") > 0;

  return (ret);
}


//
// 'lprint_escpos_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_escpos_t *escpos = (lprint_escpos_t *)papplJobGetData(job);
					// ESC/POS driver data


  (void)page;

  lprint_escpos_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Feed 1"...
  papplDevicePrintf(device, "\033J%c", 203);

  if (options->finishings & PAPPL_FINISHINGS_TRIM)
  {
    // Cut...
    papplDevicePuts(device, "\033i");
  }

  // Free memory and return...
  lprintDitherFree(&escpos->dither);

  // Update status...
  lprint_escpos_update_reasons(papplJobGetPrinter(job), job, device);

  return (true);
}


//
// 'lprint_escpos_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_escpos_t *escpos = (lprint_escpos_t *)calloc(1, sizeof(lprint_escpos_t));
					// ESC/POS driver data
  int		left_margin;		// Left margin in dots


  (void)options;

  // Initialize driver data...
  lprint_escpos_init(job, escpos);

  papplJobSetData(job, escpos);

  // Reset the printer...
  if (papplDevicePuts(device, "\033@") < 0)
    return (false);

  // Set the left margin based on the difference in the media width and the
  // maximum supported by the printer (the roll is centered)...
  left_margin = options->printer_resolution[0] * (escpos->max_width - options->media.size_width) / 2540 / 2;
  if (left_margin < 0)
    left_margin = 0;

  return (papplDevicePrintf(device, "\035L%c%c", left_margin & 255, left_margin >> 8) > 0);
}


//
// 'lprint_escpos_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_escpos_t	*escpos = (lprint_escpos_t *)papplJobGetData(job);
					// ESC/POS driver data


  (void)page;

  // Update status...
  lprint_escpos_update_reasons(papplJobGetPrinter(job), job, device);

  // Setup dithering buffer...
  if (!lprintDitherAlloc(&escpos->dither, job, options, CUPS_CSPACE_K, 1.0))
    return (false);

  escpos->marked    = false;
  escpos->feed      = 0;
  escpos->max_lines = (int)(sizeof(escpos->buffer) / escpos->dither.out_width);
  escpos->num_lines = 0;

  return (true);
}


//
// 'lprint_escpos_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_escpos_t	*escpos = (lprint_escpos_t *)papplJobGetData(job);
					// ESC/POS driver data
  bool			ret = true;	// Return value
  bool			blank;		// Is the current line blank?


  if (!lprintDitherLine(&escpos->dither, y, line))
    blank = true;
  else
    blank = !escpos->dither.output[0] && !memcmp(escpos->dither.output, escpos->dither.output + 1, escpos->dither.out_width - 1);

  if (!blank)
  {
    // Not a blank line, print it...
    if (!escpos->marked)
    {
      // Don't keep whitespace at the "top" of the receipt...
      escpos->marked = true;
      escpos->feed   = 0;
    }

    memcpy(escpos->buffer + escpos->num_lines * escpos->dither.out_width, escpos->dither.output, escpos->dither.out_width);
    escpos->num_lines ++;
  }

  if ((blank && escpos->num_lines > 0) || escpos->num_lines >= escpos->max_lines || (y == options->header.cupsHeight && escpos->num_lines > 0))
  {
    // Output current block of graphics...
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Writing %ux%d block with feed %d.", escpos->dither.out_width, escpos->num_lines, escpos->feed);

    while (escpos->feed > 0)
    {
      if (escpos->feed > 255)
      {
        ret &= papplDevicePrintf(device, "\033J%c", 255) > 0;
        escpos->feed -= 255;
      }
      else
      {
        ret &= papplDevicePrintf(device, "\033J%c", escpos->feed) > 0;
        escpos->feed = 0;
      }
    }

    ret &= papplDevicePrintf(device, "\035v00%c%c%c%c", escpos->dither.out_width & 255, (escpos->dither.out_width >> 8) & 255, escpos->num_lines & 255, (escpos->num_lines >> 8) & 255) > 0;

    ret &= papplDeviceWrite(device, escpos->buffer, escpos->num_lines * escpos->dither.out_width) > 0;

    escpos->num_lines = 0;
  }

  if (blank)
  {
    // Blank line, accumulate the feed...
    escpos->feed ++;
  }

  return (ret);
}


//
// 'lprint_escpos_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_status(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_device_t	*device;	// Connection to printer
  bool			ret;		// Return value


  if ((device = papplPrinterOpenDevice(printer)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to open device for status.");
    return (false);
  }

  // Get the printer status...
  ret = lprint_escpos_update_reasons(printer, NULL, device);

  papplPrinterCloseDevice(printer);

  return (ret);
}


//
// 'lprint_escpos_update_reasons()' - Update "printer-state-reasons" values.
//

static bool				// O - `true` on success, `false` on failure
lprint_escpos_update_reasons(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_t     *job,		// I - Current job or `NULL` if none
    pappl_device_t  *device)		// I - Connection to device
{
  unsigned char		status;		// Status byte
  pappl_preason_t	reasons;	// "printer-state-reasons" values


  // Get the printer status...
  if (papplDevicePuts(device, "\033v") < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to send paper sensor status command.");
    return (false);
  }

  if (papplDeviceRead(device, &status, 1) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to read paper sensor status response.");
    return (false);
  }

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Paper sensor status is 0x%02X.", status);

  reasons = PAPPL_PREASON_NONE;

  if ((status & 0x03) == 0x03)
    reasons |= PAPPL_PREASON_MEDIA_LOW;
  if ((status & 0x0c) == 0x00c)
    reasons |= PAPPL_PREASON_MEDIA_EMPTY;

  if (job && (reasons & PAPPL_PREASON_MEDIA_EMPTY))
    reasons |= PAPPL_PREASON_MEDIA_NEEDED;

  papplPrinterSetReasons(printer, reasons, PAPPL_PREASON_MEDIA_EMPTY | PAPPL_PREASON_MEDIA_LOW | PAPPL_PREASON_MEDIA_NEEDED);

  return (true);
}
