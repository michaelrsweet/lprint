//
// ZPL driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2021 by Michael R Sweet.
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


#define ZPL_COMPRESSION 1

//
// Local types...
//

typedef struct lprint_zpl_s		// ZPL driver data
{
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
  data->format        = "application/vnd.zebra-zpl";

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

  if (!strncmp(driver_name, "zpl_2inch-", 16))
  {
    // 2 inch printer...
    data->num_media = (int)(sizeof(lprint_zpl_2inch_media) / sizeof(lprint_zpl_2inch_media[0]));
    memcpy(data->media, lprint_zpl_2inch_media, sizeof(lprint_zpl_2inch_media));

    strlcpy(data->media_default.size_name, "oe_2x3-label_2x3in", sizeof(data->media_default.size_name));
  }
  else
  {
    // 4 inch printer...
    data->num_media = (int)(sizeof(lprint_zpl_4inch_media) / sizeof(lprint_zpl_4inch_media[0]));
    memcpy(data->media, lprint_zpl_4inch_media, sizeof(lprint_zpl_4inch_media));

    strlcpy(data->media_default.size_name, "oe_4x6-label_4x6in", sizeof(data->media_default.size_name));
  }

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->top_offset_supported[0] = -1500;
  data->top_offset_supported[1] = 1500;
  data->tracking_supported      = PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_WEB | PAPPL_MEDIA_TRACKING_CONTINUOUS;

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

  papplDevicePrintf(device, "^XA\n^POI\n^PW%u\n^LH0,0\n^LT%d\n", options->header.cupsWidth, options->media.top_offset * options->printer_resolution[1] / 2540);

  if (options->media.type[0] && strcmp(options->media.type, "labels"))
  {
    // Continuous media, so always set tracking to continuous...
    options->media.tracking = PAPPL_MEDIA_TRACKING_CONTINUOUS;
  }

  if (options->media.tracking)
  {
    if (options->media.tracking == PAPPL_MEDIA_TRACKING_CONTINUOUS)
      papplDevicePrintf(device, "^LL%d\n^MNN\n", options->header.cupsHeight);
    else if (options->media.tracking == PAPPL_MEDIA_TRACKING_WEB)
      papplDevicePuts(device, "^MNY\n");
    else
      papplDevicePuts(device, "^MNM\n");
  }

  if (strstr(papplPrinterGetDriverName(papplJobGetPrinter(job)), "-tt"))
    papplDevicePuts(device, "^MTT\n");	// Thermal transfer
  else
    papplDevicePuts(device, "^MTD\n");	// Direct thermal

  papplDevicePrintf(device, "^PQ%d, 0, 0, N\n", options->copies);
  papplDevicePuts(device, "^FO0,0^XGR:LPRINT.GRF,1,1^FS\n^XZ\n");
  papplDevicePuts(device, "^XA\n^IDR:LPRINT.GRF^FS\n^XZ\n");

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
  lprint_zpl_t	*zpl = (lprint_zpl_t *)calloc(1, sizeof(lprint_zpl_t));
					// ZPL driver data


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

  // printer-darkness
  papplDevicePrintf(device, "~SD%02u\n", 30 * data.darkness_configured / 100);

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


  (void)page;

  // print-darkness
  papplDevicePrintf(device, "~MD%d\n", 30 * options->print_darkness / 100);

  // print-speed
  if ((ips = options->print_speed / 2540) > 0)
    papplDevicePrintf(device, "^PR%d,%d,%d\n", ips, ips, ips);

  // Download bitmap...
  papplDevicePrintf(device, "~DGR:LPRINT.GRF,%u,%u,\n", options->header.cupsHeight * options->header.cupsBytesPerLine, options->header.cupsBytesPerLine);

  // Allocate memory for writing the bitmap...
  zpl->comp_buffer     = malloc(2 * options->header.cupsBytesPerLine + 1);
  zpl->last_buffer     = malloc(options->header.cupsBytesPerLine);
  zpl->last_buffer_set = 0;

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

  (void)y;

  // Determine whether this row is the same as the previous line.
  // If so, output a ':' and return...
  if (zpl->last_buffer_set && !memcmp(line, zpl->last_buffer, options->header.cupsBytesPerLine))
  {
    papplDeviceWrite(device, ":", 1);
    return (true);
  }

  // Convert the line to hex digits...
  for (ptr = line, compptr = zpl->comp_buffer, i = options->header.cupsBytesPerLine; i > 0; i --, ptr ++)
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
  memcpy(zpl->last_buffer, line, options->header.cupsBytesPerLine);
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
  (void)printer;

// ZPL Auto-configuration:
//
// ~HI returns model, firmware version, and dots-per-millimeter
// ~HQES returns status information
// ~HS returns other status and mode information

  return (true);
}
