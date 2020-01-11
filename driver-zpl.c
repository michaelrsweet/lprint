//
// ZPL driver for LPrint, a Label Printer Utility
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

static int	lprint_zpl_compress(lprint_device_t *device, unsigned char ch, unsigned count);
static int	lprint_zpl_print(lprint_job_t *job, lprint_options_t *options);
static int	lprint_zpl_rendjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_zpl_rendpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_zpl_rstartjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_zpl_rstartpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_zpl_rwrite(lprint_job_t *job, lprint_options_t *options, unsigned y, const unsigned char *line);
static int	lprint_zpl_status(lprint_printer_t *printer);


//
// 'lprintInitZPL()' - Initialize the driver.
//

void
lprintInitZPL(
    lprint_driver_t *driver)		// I - Driver
{
  pthread_rwlock_wrlock(&driver->rwlock);

  driver->print      = lprint_zpl_print;
  driver->rendjob    = lprint_zpl_rendjob;
  driver->rendpage   = lprint_zpl_rendpage;
  driver->rstartjob  = lprint_zpl_rstartjob;
  driver->rstartpage = lprint_zpl_rstartpage;
  driver->rwrite     = lprint_zpl_rwrite;
  driver->status     = lprint_zpl_status;
  driver->format     = "application/vnd.zebra-zpl";

  driver->num_resolution  = 1;

  if (strstr(driver->name, "-203dpi"))
  {
    driver->x_resolution[0] = 203;
    driver->y_resolution[0] = 203;
  }
  else
  {
    driver->x_resolution[0] = 300;
    driver->y_resolution[0] = 300;
  }

  if (!strncmp(driver->name, "zebra_zpl-2inch-", 16))
  {
    // 2 inch printer...
    driver->num_media = (int)(sizeof(lprint_zpl_2inch_media) / sizeof(lprint_zpl_2inch_media[0]));
    memcpy(driver->media, lprint_zpl_2inch_media, sizeof(lprint_zpl_2inch_media));

    strlcpy(driver->max_media, "roll_max_2x39.6in", sizeof(driver->max_media));
    strlcpy(driver->min_media, "roll_min_0.75x0.25in", sizeof(driver->min_media));
    strlcpy(driver->media_default, "oe_2x3-label_2x3in", sizeof(driver->media_default));
  }
  else
  {
    // 4 inch printer...
    driver->num_media = (int)(sizeof(lprint_zpl_4inch_media) / sizeof(lprint_zpl_4inch_media[0]));
    memcpy(driver->media, lprint_zpl_4inch_media, sizeof(lprint_zpl_4inch_media));

    strlcpy(driver->max_media, "roll_max_4x39.6in", sizeof(driver->max_media));
    strlcpy(driver->min_media, "roll_min_0.75x0.25in", sizeof(driver->min_media));
    strlcpy(driver->media_default, "oe_4x6-label_4x6in", sizeof(driver->media_default));
  }

  driver->num_source = 1;
  driver->source[0]  = "main-roll";

  strlcpy(driver->media_ready[0], driver->media_default, sizeof(driver->media_ready[0]));
  strlcpy(driver->source_default, "main-roll", sizeof(driver->source_default));

  driver->top_offset_default      = 0;
  driver->top_offset_supported[0] = -1500;
  driver->top_offset_supported[1] = 1500;

  driver->tracking_default   = LPRINT_MEDIA_TRACKING_MARK;
  driver->tracking_supported = LPRINT_MEDIA_TRACKING_MARK | LPRINT_MEDIA_TRACKING_WEB | LPRINT_MEDIA_TRACKING_CONTINUOUS;

  driver->num_type = 2;
  driver->type[0]  = "labels";
  driver->type[1]  = "continuous";
  strlcpy(driver->type_default, "labels", sizeof(driver->type_default));

  driver->mode_configured = LPRINT_LABEL_MODE_TEAR_OFF;
  driver->mode_configured = LPRINT_LABEL_MODE_APPLICATOR | LPRINT_LABEL_MODE_CUTTER | LPRINT_LABEL_MODE_CUTTER_DELAYED | LPRINT_LABEL_MODE_KIOSK | LPRINT_LABEL_MODE_PEEL_OFF | LPRINT_LABEL_MODE_PEEL_OFF_PREPEEL | LPRINT_LABEL_MODE_REWIND | LPRINT_LABEL_MODE_RFID | LPRINT_LABEL_MODE_TEAR_OFF;

  driver->tear_offset_configured   = 0;
  driver->tear_offset_supported[0] = -1500;
  driver->tear_offset_supported[1] = 1500;

  driver->speed_default      = 0;
  driver->speed_supported[0] = 2540;
  driver->speed_supported[1] = 12 * 2540;

  driver->darkness_configured = 50;
  driver->darkness_supported  = 30;

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_zpl_compress()' - Output a RLE run...
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_compress(
    lprint_device_t *device,		// I - Output device
    unsigned char   ch,			// I - Repeat character
    unsigned        count)		// I - Repeat count
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
        if (lprintWriteDevice(device, buffer, sizeof(buffer)) < 0)
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

  return (lprintWriteDevice(device, buffer, bufptr - buffer) > 0);
}


//
// 'lprint_zpl_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_print(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  int		infd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  // Copy the raw file...
  job->impressions = 1;

  infd  = open(job->filename, O_RDONLY);

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    if (lprintWriteDevice(device, buffer, (size_t)bytes) < 0)
    {
      lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(infd);
      return (0);
    }
  }
  close(infd);

  job->impcompleted = 1;

  return (1);
}


//
// 'lprint_zpl_rendjob()' - End a job.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_rendjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_zpl_t	*zpl = job->printer->driver->job_data;
					// ZPL driver data


  (void)options;

  free(zpl);
  job->printer->driver->job_data = NULL;

  return (1);
}


//
// 'lprint_zpl_rendpage()' - End a page.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_rendpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  lprint_zpl_t	*zpl = job->printer->driver->job_data;
					// ZPL driver data


  (void)page;

  lprintPrintfDevice(device, "^XA\n^POI\n^PW%u\n^LH0,0\n^LT%d\n", options->header.cupsWidth, options->media_top_offset * options->printer_resolution[1] / 2540);

  if (options->media_tracking)
  {
    if (!strcmp(options->media_tracking, "continuous"))
      lprintPrintfDevice(device, "^LL%d\n^MNN\n", options->header.cupsHeight);
    else if (!strcmp(options->media_tracking, "web"))
      lprintPutsDevice(device, "^MNY\n");
    else
      lprintPutsDevice(device, "^MNM\n");
  }

#if 0
  if (options->media_type)
  {
    if (!strcmp(options->media_type, "labels-thermal"))
      lprintPutsDevice(device, "^MTT\n");
    else if (!strcmp(options->media_type, "labels-direct-thermal"))
      lprintPutsDevice(device, "^MTD\n");
  }
#endif // 0

  lprintPrintfDevice(device, "^PQ%d, 0, 0, N\n", options->copies);
  lprintPutsDevice(device, "^FO0,0^XGR:LPRINT.GRF,1,1^FS\n^XZ\n");
  lprintPutsDevice(device, "^XA\n^IDR:LPRINT.GRF^FS\n^XZ\n");

  free(zpl->comp_buffer);
  free(zpl->last_buffer);

  return (1);
}


//
// 'lprint_zpl_rstartjob()' - Start a job.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_rstartjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_driver_t *driver = job->printer->driver;
					// Driver
  lprint_device_t *device = driver->device;
					// Output device
  lprint_zpl_t	*zpl = (lprint_zpl_t *)calloc(1, sizeof(lprint_zpl_t));
					// ZPL driver data


  driver->job_data = zpl;

  // label-mode-configured
  switch (driver->mode_configured)
  {
    case LPRINT_LABEL_MODE_APPLICATOR :
        lprintPutsDevice(device, "^MMA,N\n");
        break;
    case LPRINT_LABEL_MODE_CUTTER :
        lprintPutsDevice(device, "^MMC,N\n");
        break;
    case LPRINT_LABEL_MODE_CUTTER_DELAYED :
        lprintPutsDevice(device, "^MMD,N\n");
        break;
    case LPRINT_LABEL_MODE_KIOSK :
        lprintPutsDevice(device, "^MMK,N\n");
        break;
    case LPRINT_LABEL_MODE_PEEL_OFF :
        lprintPutsDevice(device, "^MMP,N\n");
        break;
    case LPRINT_LABEL_MODE_PEEL_OFF_PREPEEL :
        lprintPutsDevice(device, "^MMP,Y\n");
        break;
    case LPRINT_LABEL_MODE_REWIND :
        lprintPutsDevice(device, "^MMR,N\n");
        break;
    case LPRINT_LABEL_MODE_RFID :
        lprintPutsDevice(device, "^MMF,N\n");
        break;
    case LPRINT_LABEL_MODE_TEAR_OFF :
    default :
        lprintPutsDevice(device, "^MMT,N\n");
        break;
  }

  // label-tear-offset-configured
  if (driver->tear_offset_configured < 0)
    lprintPrintfDevice(device, "~TA%04d\n", driver->tear_offset_configured);
  else
    lprintPrintfDevice(device, "~TA%03d\n", driver->tear_offset_configured);

  // printer-darkness
  lprintPrintfDevice(device, "~SD%02u\n", 30 * driver->darkness_configured / 100);

  return (1);
}


//
// 'lprint_zpl_rstartpage()' - Start a page.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_rstartpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  lprint_zpl_t	*zpl = job->printer->driver->job_data;
					// ZPL driver data
  int		ips;			// Inches per second


  (void)page;

  // print-darkness
  lprintPrintfDevice(device, "~MD%d\n", 30 * options->print_darkness / 100);

  // print-speed
  if ((ips = options->print_speed / 2540) > 0)
    lprintPrintfDevice(device, "^PR%d,%d,%d\n", ips, ips, ips);

  // Download bitmap...
  lprintPrintfDevice(device, "~DGR:LPRINT.GRF,%u,%u,\n", options->header.cupsHeight * options->header.cupsBytesPerLine, options->header.cupsBytesPerLine);

  // Allocate memory for writing the bitmap...
  zpl->comp_buffer     = malloc(2 * options->header.cupsBytesPerLine + 1);
  zpl->last_buffer     = malloc(options->header.cupsBytesPerLine);
  zpl->last_buffer_set = 0;

  return (1);
}


//
// 'lprint_zpl_rwrite()' - Write a raster line.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_rwrite(
    lprint_job_t        *job,		// I - Job
    lprint_options_t    *options,	// I - Job options
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  lprint_zpl_t	*zpl = job->printer->driver->job_data;
					// ZPL driver data
  unsigned		i;		// Looping var
  const unsigned char	*ptr;		// Pointer into buffer
  unsigned char		*compptr;	// Pointer into compression buffer
  unsigned char		repeat_char;	// Repeated character
  unsigned		repeat_count;	// Number of repeated characters
  static const unsigned char *hex = (const unsigned char *)"0123456789ABCDEF";
					// Hex digits

  (void)y;

  // Determine whether this row is the same as the previous line.
  // If so, output a ':' and return...
  if (zpl->last_buffer_set)
  {
    if (!memcmp(line, zpl->last_buffer, options->header.cupsBytesPerLine))
    {
      lprintWriteDevice(device, ":", 1);
      return (1);
    }
  }

  // Convert the line to hex digits...
  for (ptr = line, compptr = zpl->comp_buffer, i = options->header.cupsBytesPerLine; i > 0; i --, ptr ++)
  {
    *compptr++ = hex[*ptr >> 4];
    *compptr++ = hex[*ptr & 15];
  }

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
      lprintPutsDevice(device, "0");
    }

    if (repeat_count > 0)
      lprintPutsDevice(device, ",");
  }
  else
    lprint_zpl_compress(device, repeat_char, repeat_count);

  // Save this line for the next round...
  memcpy(zpl->last_buffer, line, options->header.cupsBytesPerLine);
  zpl->last_buffer_set = 1;

  return (1);
}


//
// 'lprint_zpl_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_zpl_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

// ZPL commands:
//
// print-darkness: Map 0 to 100 to ^MD command with values from -30 to 30;
//                 print-darkness-supported=61, -default=50
//
// ZPL Auto-configuration:
//
// ~HI returns model, firmware version, and dots-per-millimeter
// ~HQES returns status information
// ~HS returns other status and mode information

  return (1);
}
