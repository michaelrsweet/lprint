//
// ZPL driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2025 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2001-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


// Define to 1 to use run-length encoding, 0 for uncompressed
#define ZPL_COMPRESSION 1

// Error and warning bits
#define ZPL_ERROR_MEDIA_OUT		0x00000001
#define ZPL_ERROR_RIBBON_OUT		0x00000002
#define ZPL_ERROR_HEAD_OPEN		0x00000004
#define ZPL_ERROR_CUTTER_FAULT		0x00000008
#define ZPL_ERROR_CLEAR_PP_FAILED	0x00008000
#define ZPL_ERROR_PAPER_FEED		0x00004000
#define ZPL_ERROR_PRESENTER		0x00002000
#define ZPL_ERROR_PAPER_JAM		0x00001000
#define ZPL_ERROR_MARK_NOT_FOUND	0x00080000
#define ZPL_ERROR_MARK_CALIBRATE	0x00040000
#define ZPL_ERROR_RETRACT_TIMEOUT	0x00020000
#define ZPL_ERROR_PAUSED		0x00010000

#define ZPL_WARNING_PAPER_ALMOST_OUT	0x00000008
#define ZPL_WARNING_REPLACE_PRINTHEAD	0x00000004
#define ZPL_WARNING_CLEAN_PRINTHEAD	0x00000002
#define ZPL_WARNING_CALIBRATE_MEDIA	0x00000001
#define ZPL_WARNING_PAPER_BEFORE_HEAD	0x00000010
#define ZPL_WARNING_BLACK_MARK		0x00000020
#define ZPL_WARNING_PAPER_AFTER_HEAD	0x00000040
#define ZPL_WARNING_LOOP_READY		0x00000080
#define ZPL_WARNING_PRESENTER		0x00000100
#define ZPL_WARNING_RETRACT_READY	0x00000200
#define ZPL_WARNING_IN_RETRACT		0x00000400
#define ZPL_WARNING_AT_BIN		0x00000800


//
// Local types...
//

typedef struct lprint_zpl_s		// ZPL driver data
{
  lprint_dither_t dither;		// Dither buffer
  unsigned char	*comp_buffer;		// Compression buffer
  unsigned char *last_buffer;		// Last line
  int		last_buffer_set;	// Is the last line set?
} lprint_zpl_t;


//
// Local globals...
//

static const char * const lprint_zpl_2inch_media[] =
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

  "roll_max_2x39.6in",
  "roll_min_0.75x0.25in"
};
static const char * const lprint_zpl_4inch_media[] =
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
  "oe_4x7.83-label_4x7.83in",
  "oe_4x8-label_4x8in",
  "oe_4x13-label_4x13in",

  "roll_max_4x39.6in",
  "roll_min_0.75x0.25in"

/*
  "oe_6x1-label_6x1in",
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
  "oe_8x13-label_8x13in",

  "roll_max_8x39.6in",
  "roll_min_1.25x0.25in"
*/
};


//
// Local functions...
//

#if ZPL_COMPRESSION
static bool	lprint_zpl_compress(pappl_device_t *device, unsigned char ch, unsigned count);
#endif // ZPL_COMPRESSION
static bool	lprint_zpl_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zpl_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zpl_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_zpl_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zpl_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_zpl_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_zpl_status(pappl_printer_t *printer);
static bool	lprint_zpl_update_reasons(pappl_printer_t *printer, pappl_job_t *job, pappl_device_t *device);


//
// 'lprintZPL()' - Initialize the ZPL driver.
//

bool					// O - `true` on success, `false` on error
lprintZPL(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = lprint_zpl_printfile;
  data->rendjob_cb    = lprint_zpl_rendjob;
  data->rendpage_cb   = lprint_zpl_rendpage;
  data->rstartjob_cb  = lprint_zpl_rstartjob;
  data->rstartpage_cb = lprint_zpl_rstartpage;
  data->rwriteline_cb = lprint_zpl_rwriteline;
  data->status_cb     = lprint_zpl_status;
  data->format        = LPRINT_ZPL_MIMETYPE;

  data->num_resolution = 1;

  if (strstr(driver_name, "-203dpi"))
  {
    data->x_resolution[0] = 203;
    data->y_resolution[0] = 203;
  }
  else if (strstr(driver_name, "-300dpi"))
  {
    data->x_resolution[0] = 300;
    data->y_resolution[0] = 300;
  }
  else
  {
    data->x_resolution[0] = 600;
    data->y_resolution[0] = 600;
  }

  data->x_default = data->y_default = data->x_resolution[0];

  if (strstr(driver_name, "-cutter"))
    data->finishings |= PAPPL_FINISHINGS_TRIM;

  if (!strncmp(driver_name, "zpl_2inch-", 16))
  {
    // 2 inch printer...
    data->num_media = (int)(sizeof(lprint_zpl_2inch_media) / sizeof(lprint_zpl_2inch_media[0]));
    memcpy(data->media, lprint_zpl_2inch_media, sizeof(lprint_zpl_2inch_media));

    papplCopyString(data->media_ready[0].size_name, "oe_2x3-label_2x3in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
  }
  else
  {
    // 4 inch printer...
    data->num_media = (int)(sizeof(lprint_zpl_4inch_media) / sizeof(lprint_zpl_4inch_media[0]));
    memcpy(data->media, lprint_zpl_4inch_media, sizeof(lprint_zpl_4inch_media));

    papplCopyString(data->media_ready[0].size_name, "na_index-4x6_4x6in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
  }

  data->media_ready[0].tracking = PAPPL_MEDIA_TRACKING_GAP;

  data->bottom_top = data->left_right = 1;

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->top_offset_supported[0] = -1500;
  data->top_offset_supported[1] = 1500;
  data->tracking_supported      = PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_WEB | PAPPL_MEDIA_TRACKING_GAP | PAPPL_MEDIA_TRACKING_CONTINUOUS;

  data->num_type = 3;
  data->type[0]  = "continuous";
  data->type[1]  = "labels";
  data->type[2]  = "labels-continuous";

  data->mode_configured = PAPPL_LABEL_MODE_TEAR_OFF;
  data->mode_supported = PAPPL_LABEL_MODE_APPLICATOR | PAPPL_LABEL_MODE_CUTTER | PAPPL_LABEL_MODE_CUTTER_DELAYED | PAPPL_LABEL_MODE_KIOSK | PAPPL_LABEL_MODE_PEEL_OFF | PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL | PAPPL_LABEL_MODE_REWIND | PAPPL_LABEL_MODE_RFID | PAPPL_LABEL_MODE_TEAR_OFF;

  data->tear_offset_configured   = 0;
  data->tear_offset_supported[0] = -1500;
  data->tear_offset_supported[1] = 1500;

  data->speed_default      = 0;
  data->speed_supported[0] = 2540;
  data->speed_supported[1] = 12 * 2540;

  data->darkness_configured = 50;
  data->darkness_supported  = 30;

  return (true);
}


//
// 'lprintZPLQueryDriver()' - Query the printer to determine the proper driver.
//

void
lprintZPLQueryDriver(
    pappl_system_t *system,		// I - System
    const char     *device_uri,		// I - Device URI
    char           *name,		// I - Name buffer
    size_t         namesize)		// I - Size of name buffer
{
  pappl_device_t	*device;	// Device connection
  char			line[1025];	// Line from device
  ssize_t		bytes;		// Bytes read
  char			model[256],	// Model name
			*modelptr;	// Pointer into model name
  int			dpmm = 0;	// Dots per millimeter


  // Make sure name buffer is initialized...
  *name = '\0';

  // Connect and send Host Information command...
  if ((device = papplDeviceOpen(device_uri, "query", papplLogDevice, system)) == NULL)
    return;

  if (papplDevicePuts(device, "~HI\n") < 0)
    goto done;

  // Read and parse response:
  //
  // <stx>MODEL,VERSION,DPMM,MEMORY,OPTIONS<etx><cr><lf>
  if ((bytes = papplDeviceRead(device, line, sizeof(line) - 1)) <= 0)
    goto done;

  line[bytes] = '\0';

  papplLog(system, PAPPL_LOGLEVEL_DEBUG, "HI response for '%s' was '%s'.", device_uri, line);

  if (line[0] == 0x02 && sscanf(line + 1, "%255[^,],%*[^,],%d", model, &dpmm) > 1 && (dpmm == 8 || dpmm == 12 || dpmm == 24))
  {
    // Got model and dots-per-millimeter values, create a driver name from it...
    // Note: We currently assume a 4" print head for auto-detection...
    const char	*type = "tt";		// Type of printing (direct/transfer)
    int		dpi;			// Print resolution

    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "model='%s', dpmm=%d", model, dpmm);

    if ((modelptr = strchr(model, '-')) != NULL && modelptr > model)
    {
      if (modelptr[-1] == 'd')
        type = "dt";
    }

    if (dpmm == 8)
      dpi = 203;			// 203.2
    else if (dpmm == 12)
      dpi = 300;			// Technically should be 304.8...
    else
      dpi = 600;			// Technically should be 609.6...

    snprintf(name, namesize, "zpl_4inch-%ddpi-%s", dpi, type);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "auto driver-name='%s'", name);
  }

  done:

  papplDeviceClose(device);
  return;
}


#if ZPL_COMPRESSION
//
// 'lprint_zpl_compress()' - Output a RLE run...
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_compress(
    pappl_device_t *device,		// I - Output device
    unsigned char  ch,			// I - Repeat character
    unsigned       count)		// I - Repeat count
{
  unsigned char	buffer[8192],		// Output buffer
		*bufptr = buffer;	// Pointer into buffer


  if (count > 1)
  {
    // Print as many z's as possible - they are the largest denomination
    // representing 400 characters (zC stands for 400 adjacent C's)
    while (count >= 400)
    {
      count -= 400;
      *bufptr++ = 'z';

      if (bufptr >= (buffer + sizeof(buffer)))
      {
        if (papplDeviceWrite(device, buffer, sizeof(buffer)) < 0)
          return (0);

	bufptr = buffer;
      }
    }

    // Then print 'g' through 'y' as multiples of 20 characters...
    if (count >= 20)
    {
      *bufptr++ = 'f' + count / 20;
      count %= 20;
    }

    // Finally, print 'G' through 'Y' as 1 through 19 characters...
    if (count > 0)
      *bufptr++ = 'F' + count;
  }

  // Then the character to be repeated...
  *bufptr++ = ch;

  return (papplDeviceWrite(device, buffer, bufptr - buffer) > 0);
}
#endif // ZPL_COMPRESSION


//
// 'lprint_zpl_print()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_printfile(
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

  // Update status...
  lprint_zpl_update_reasons(papplJobGetPrinter(job), job, device);

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

  // Update status...
  lprint_zpl_update_reasons(papplJobGetPrinter(job), job, device);

  return (true);
}


//
// 'lprint_zpl_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_zpl_t	*zpl = (lprint_zpl_t *)papplJobGetData(job);
					// ZPL driver data


  (void)options;

  free(zpl);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_zpl_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_zpl_t	*zpl = (lprint_zpl_t *)papplJobGetData(job);
					// ZPL driver data


  (void)page;

  lprint_zpl_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  papplDevicePrintf(device, "^XA\n^POI\n^PW%u\n^LH0,0\n^LT%d\n", options->header.cupsWidth, options->media.top_offset * options->printer_resolution[1] / 2540);

  if (options->media.type[0] && strcmp(options->media.type, "labels"))
  {
    // Continuous media, so always set tracking to continuous...
    options->media.tracking = PAPPL_MEDIA_TRACKING_CONTINUOUS;
  }

  if (options->media.tracking)
  {
    if (options->media.tracking == PAPPL_MEDIA_TRACKING_CONTINUOUS)
      papplDevicePrintf(device, "^LL%u\n^MNN\n", options->header.cupsHeight);
    else if (options->media.tracking == PAPPL_MEDIA_TRACKING_WEB)
      papplDevicePuts(device, "^MNY\n");
    else
      papplDevicePuts(device, "^MNM\n");
  }

  if (strstr(papplPrinterGetDriverName(papplJobGetPrinter(job)), "-tt"))
    papplDevicePuts(device, "^MTT\n");	// Thermal transfer
  else
    papplDevicePuts(device, "^MTD\n");	// Direct thermal

  papplDevicePrintf(device, "^PQ1, 0, 0, N\n");
  papplDevicePuts(device, "^FO0,0^XGR:LPRINT.GRF,1,1^FS\n^XZ\n");
  papplDevicePuts(device, "^XA\n^IDR:LPRINT.GRF^FS\n^XZ\n");

  if (options->finishings & PAPPL_FINISHINGS_TRIM)
    papplDevicePuts(device, "^CN1\n");

  // Update status...
  lprint_zpl_update_reasons(papplJobGetPrinter(job), job, device);

  // Free memory and return...
  lprintDitherFree(&zpl->dither);

  free(zpl->comp_buffer);
  free(zpl->last_buffer);

  return (true);
}


//
// 'lprint_zpl_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  pappl_pr_driver_data_t data;		// Driver data
  int			darkness;	// Composite darkness value
  lprint_zpl_t	*zpl = (lprint_zpl_t *)calloc(1, sizeof(lprint_zpl_t));
					// ZPL driver data


  // Initialize driver data...
  papplJobSetData(job, zpl);

  papplPrinterGetDriverData(papplJobGetPrinter(job), &data);

  // label-mode-configured
  switch (data.mode_configured)
  {
    case PAPPL_LABEL_MODE_APPLICATOR :
        papplDevicePuts(device, "^MMA,Y\n");
        break;
    case PAPPL_LABEL_MODE_CUTTER :
        papplDevicePuts(device, "^MMC,Y\n");
        break;
    case PAPPL_LABEL_MODE_CUTTER_DELAYED :
        papplDevicePuts(device, "^MMD,Y\n");
        break;
    case PAPPL_LABEL_MODE_KIOSK :
        papplDevicePuts(device, "^MMK,Y\n");
        break;
    case PAPPL_LABEL_MODE_PEEL_OFF :
        papplDevicePuts(device, "^MMP,N\n");
        break;
    case PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL :
        papplDevicePuts(device, "^MMP,Y\n");
        break;
    case PAPPL_LABEL_MODE_REWIND :
        papplDevicePuts(device, "^MMR,Y\n");
        break;
    case PAPPL_LABEL_MODE_RFID :
        papplDevicePuts(device, "^MMF,Y\n");
        break;
    case PAPPL_LABEL_MODE_TEAR_OFF :
    default :
        papplDevicePuts(device, "^MMT,Y\n");
        break;
  }

  // label-tear-offset-configured
  if (data.tear_offset_configured < 0)
    papplDevicePrintf(device, "~TA%04d\n", data.tear_offset_configured);
  else if (data.tear_offset_configured > 0)
    papplDevicePrintf(device, "~TA%03d\n", data.tear_offset_configured);

  // print-darkness / printer-darkness-configured
  if ((darkness = options->print_darkness + options->darkness_configured) < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  papplDevicePrintf(device, "~SD%02u\n", 30 * darkness / 100);

  return (true);
}


//
// 'lprint_zpl_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_zpl_t	*zpl = (lprint_zpl_t *)papplJobGetData(job);
					// ZPL driver data
  int		ips;			// Inches per second
  double	out_gamma = 1.0;	// Output gamma correction


  (void)page;

  // Update status...
  lprint_zpl_update_reasons(papplJobGetPrinter(job), job, device);

  // Setup dither buffer...
  if (options->header.HWResolution[0] == 300)
    out_gamma = 1.2;
  else if (options->header.HWResolution[0] == 600)
    out_gamma = 1.44;

  if (!lprintDitherAlloc(&zpl->dither, job, options, CUPS_CSPACE_K, out_gamma))
    return (false);

  // print-speed
  if ((ips = options->print_speed / 2540) > 0)
    papplDevicePrintf(device, "^PR%d,%d,%d\n", ips, ips, ips);

  // Download bitmap...
  papplDevicePrintf(device, "~DGR:LPRINT.GRF,%u,%u,\n", zpl->dither.in_height * zpl->dither.out_width, zpl->dither.out_width);

  // Allocate memory for writing the bitmap...
  zpl->comp_buffer     = malloc(2 * zpl->dither.out_width + 1);
  zpl->last_buffer     = malloc(zpl->dither.out_width);
  zpl->last_buffer_set = 0;

  if (!zpl->comp_buffer || !zpl->last_buffer)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate compression buffers.");
    return (false);
  }

  return (true);
}


//
// 'lprint_zpl_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_zpl_t	*zpl = (lprint_zpl_t *)papplJobGetData(job);
					// ZPL driver data
  unsigned		i;		// Looping var
  const unsigned char	*ptr;		// Pointer into buffer
  unsigned char		*compptr;	// Pointer into compression buffer
#if ZPL_COMPRESSION
  unsigned char		repeat_char;	// Repeated character
  unsigned		repeat_count;	// Number of repeated characters
#endif // ZPL_COMPRESSION
  static const unsigned char *hex = (const unsigned char *)"0123456789ABCDEF";
					// Hex digits


  if (!lprintDitherLine(&zpl->dither, y, line))
    return (true);

  // Determine whether this row is the same as the previous line.
  // If so, output a ':' and return...
  if (zpl->last_buffer_set && !memcmp(zpl->dither.output, zpl->last_buffer, zpl->dither.out_width))
  {
    papplDeviceWrite(device, ":", 1);
    return (true);
  }

  // Convert the line to hex digits...
  for (ptr = zpl->dither.output, compptr = zpl->comp_buffer, i = zpl->dither.out_width; i > 0; i --, ptr ++)
  {
    *compptr++ = hex[*ptr >> 4];
    *compptr++ = hex[*ptr & 15];
  }

#if ZPL_COMPRESSION
  // Send run-length compressed HEX data...
  *compptr = '\0';

  // Run-length compress the graphics...
  for (compptr = zpl->comp_buffer + 1, repeat_char = zpl->comp_buffer[0], repeat_count = 1; *compptr; compptr ++)
  {
    if (*compptr == repeat_char)
    {
      repeat_count ++;
    }
    else
    {
      lprint_zpl_compress(device, repeat_char, repeat_count);
      repeat_char  = *compptr;
      repeat_count = 1;
    }
  }

  if (repeat_char == '0')
  {
    // Handle 0's on the end of the line...
    if (repeat_count & 1)
    {
      repeat_count --;
      papplDevicePuts(device, "0");
    }

    if (repeat_count > 0)
      papplDevicePuts(device, ",");
  }
  else
    lprint_zpl_compress(device, repeat_char, repeat_count);

#else
  // Send uncompressed HEX data...
  papplDeviceWrite(device, zpl->comp_buffer, compptr - zpl->comp_buffer);
#endif // ZPL_COMPRESSION

  // Save this line for the next round...
  memcpy(zpl->last_buffer, zpl->dither.output, zpl->dither.out_width);
  zpl->last_buffer_set = 1;

  return (true);
}


//
// 'lprint_zpl_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_status(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_device_t	*device;	// Connection to printer
  char			line[1025];	// Line from printer
  ssize_t		bytes;		// Bytes read
  int			length = 0;	// Label length
  bool			ret = false;	// Return value


  if ((device = papplPrinterOpenDevice(printer)) == NULL)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to open device for status.");
    return (false);
  }

  // Get the printer status...
  if (!lprint_zpl_update_reasons(printer, NULL, device))
    goto done;

  // Query host status...
  if (papplDevicePuts(device, "~HS\n") < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to send HS status command.");
    goto done;
  }

  if ((bytes = papplDeviceRead(device, line, sizeof(line) - 1)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to read HS status response.");
    goto done;
  }

  line[bytes] = '\0';

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "HS returned '%s'.", line);

  if (sscanf(line + 1, "%*d,%*d,%*d,%d", &length) == 1 && length > 11)
  {
    // Auto-detect label length for ready media...
    pappl_pr_driver_data_t	data;	// Driver data

    papplPrinterGetDriverData(printer, &data);

    // Round the length to the nearest dot and snap to the nearest 1/4"...
    length = (2540 * length + data.y_resolution[0] / 2) / data.y_resolution[0];

    if ((length % 635) <= 100)
      length += length % 635;
    else if ((length % 635) >= 535)
      length += 635 - (length % 635);

    // Lookup size
    lprintMediaMatch(printer, 0, 0, length);
  }

  ret = true;

  done:

  papplPrinterCloseDevice(printer);

  return (ret);
}


//
// 'lprint_zpl_update_reasons()' - Update "printer-state-reasons" values.
//

static bool				// O - `true` on success, `false` on failure
lprint_zpl_update_reasons(
    pappl_printer_t *printer,		// I - Printer
    pappl_job_t     *job,		// I - Current job or `NULL` if none
    pappl_device_t  *device)		// I - Connection to device
{
  char			line[1025],	// Line from printer
			*lineptr;	// Pointer into line
  ssize_t		bytes;		// Bytes read
  unsigned		errors = 0,	// Detected errors
			warnings = 0;	// Detected warnings
  pappl_preason_t	reasons;	// "printer-state-reasons" values


  // Get the printer status...
  if (papplDevicePuts(device, "~HQES\n") < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to send HQES status command.");
    return (false);
  }

  if ((bytes = papplDeviceRead(device, line, sizeof(line) - 1)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Unable to read HQES status response.");
    return (false);
  }

  line[bytes] = '\0';

  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "HQES returned '%s'.", line);

  if ((lineptr = strstr(line, "ERRORS:")) != NULL)
  {
    if (sscanf(lineptr + 7, "%*d%*x%x", &errors) != 1)
      errors = 0;
  }

  if ((lineptr = strstr(line, "WARNINGS:")) != NULL)
  {
    if (sscanf(lineptr + 9, "%*d%*x%x", &warnings) != 1)
      warnings = 0;
  }

  reasons = PAPPL_PREASON_NONE;

  if (errors & ZPL_ERROR_MEDIA_OUT)
    reasons |= PAPPL_PREASON_MEDIA_EMPTY;

  if (errors & ZPL_ERROR_PAPER_JAM)
    reasons |= PAPPL_PREASON_MEDIA_JAM;

  if (errors & ZPL_ERROR_PAUSED)
    reasons |= PAPPL_PREASON_OFFLINE;

  if (errors & ZPL_ERROR_RIBBON_OUT)
    reasons |= PAPPL_PREASON_MARKER_SUPPLY_EMPTY;

  if (errors & ~(ZPL_ERROR_MEDIA_OUT | ZPL_ERROR_PAPER_JAM | ZPL_ERROR_PAUSED))
  {
    reasons |= PAPPL_PREASON_OTHER;

    if (errors & ZPL_ERROR_HEAD_OPEN)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Print head open.");
    if (errors & ZPL_ERROR_CUTTER_FAULT)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Cutter fault.");
    if (errors & ZPL_ERROR_CLEAR_PP_FAILED)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Clear paper path failed.");
    if (errors & ZPL_ERROR_PAPER_FEED)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Paper feed error.");
    if (errors & ZPL_ERROR_PRESENTER)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Presenter error.");
    if (errors & ZPL_ERROR_MARK_NOT_FOUND)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Mark not found.");
    if (errors & ZPL_ERROR_MARK_CALIBRATE)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Mark calibration error.");
    if (errors & ZPL_ERROR_RETRACT_TIMEOUT)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Retraction timeout.");
  }

  if (warnings & ZPL_WARNING_PAPER_ALMOST_OUT)
    reasons |= PAPPL_PREASON_MEDIA_LOW;

  if (warnings & ~ZPL_WARNING_PAPER_ALMOST_OUT)
  {
    reasons |= PAPPL_PREASON_OTHER;

    if (errors & ZPL_WARNING_REPLACE_PRINTHEAD)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Replace print head.");

    if (errors & ZPL_WARNING_CLEAN_PRINTHEAD)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Clean print head.");

    if (errors & ZPL_WARNING_CALIBRATE_MEDIA)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Calibrate media.");

    if (errors & ZPL_WARNING_PAPER_BEFORE_HEAD)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Paper before head.");

    if (errors & ZPL_WARNING_BLACK_MARK)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Black mark.");

    if (errors & ZPL_WARNING_PAPER_AFTER_HEAD)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Paper after head.");

    if (errors & ZPL_WARNING_LOOP_READY)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Loop ready.");

    if (errors & ZPL_WARNING_PRESENTER)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Presenter.");

    if (errors & ZPL_WARNING_RETRACT_READY)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "Retract ready.");

    if (errors & ZPL_WARNING_IN_RETRACT)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "In retract.");

    if (errors & ZPL_WARNING_AT_BIN)
      papplLogPrinter(printer, PAPPL_LOGLEVEL_WARN, "At bin.");
  }

  if (job && (reasons & PAPPL_PREASON_MEDIA_EMPTY))
    reasons |= PAPPL_PREASON_MEDIA_NEEDED;

  papplPrinterSetReasons(printer, reasons, ~reasons);

  return (true);
}
