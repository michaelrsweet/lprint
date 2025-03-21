//
// Experimental Brother driver for LPrint, a Label Printer Application
//
// Copyright © 2023-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"
#include "util.h"
#ifdef LPRINT_EXPERIMENTAL


//
// Local types...
//

typedef struct lprint_brother_s		// Brother driver data
{
  bool		is_pt_series;		// Is this a PT-series printer?
  bool		is_ql_800;		// Is this the QL-800 printer?
  lprint_dither_t dither;		// Dither buffer
  int		count;			// Output count for print info
  size_t	alloc_bytes,		// Allocated bytes for output buffer
		num_bytes;		// Number of bytes in output buffer
  unsigned char	*buffer;		// Output buffer
} lprint_brother_t;


//
// Local globals...
//

static const char * const lprint_brother_ql_media[] =
{					// Supported QL-* media sizes
  "oe_dk1219-round_0.47x0.47in",
  "oe_dk1204-multi-purpose_0.66x2.1in",
  "oe_dk1203-file-folder_0.66x3.4in",
  "oe_dk1209-small-address_1.1x2.4in",
  "oe_dk1201-address_1.1x3.5in",
  "oe_dk1208-large-address_1.4x3.5in",
  "oe_dk1240-large-multi-purpose_1.9x4in",
  "oe_dk1207-cd-dvd_2.2x2.2in",
  "oe_dk1202-shipping_2.4x3.9in",

  "na_index-4x6_4x6in",				// DK1241/1247

  "roll_dk2113-continuous-film_2.4x600in",	// Black/Clear
  "roll_dk2205-continuous_2.4x1200in",		// Black on White
  "roll_dk2210-continuous_1.1x1200in",
  "roll_dk2211-continuous-film_1.1x600in",
  "roll_dk2212-continuous-film_2.4x600in",
  "roll_dk2214-continuous_0.47x1200in",
  "roll_dk2243-continuous_4x1200in",		// Black on White
  "roll_dk2246-continuous_4.07x1200in",		// Black on White
  "roll_dk2251-continuous_2.4x600in",		// Black/Red on White
  "roll_dk2606-continuous-film_2.4x600in",	// Black/Yellow
  "roll_dk4205-continuous-removable_2.4x1200in",// Black on White
  "roll_dk4605-continuous-removable_2.4x1200in",// Black/Yellow on White

  "roll_max_2.5x3600in",
  "roll_min_0.25x1in"
};

static const char * const lprint_brother_pt_media[] =
{					// Supported PT-* media sizes
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

static bool	lprint_brother_get_status(pappl_printer_t *printer, pappl_device_t *device);
static bool	lprint_brother_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_brother_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_brother_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_brother_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_brother_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_brother_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_brother_status(pappl_printer_t *printer);


//
// 'lprintBrother()' - Initialize the Brother driver.
//

bool					// O - `true` on success, `false` on error
lprintBrother(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  // Print callbacks...
  data->printfile_cb  = lprint_brother_printfile;
  data->rendjob_cb    = lprint_brother_rendjob;
  data->rendpage_cb   = lprint_brother_rendpage;
  data->rstartjob_cb  = lprint_brother_rstartjob;
  data->rstartpage_cb = lprint_brother_rstartpage;
  data->rwriteline_cb = lprint_brother_rwriteline;
  data->status_cb     = lprint_brother_status;

  // Vendor-specific format...
  data->format = LPRINT_BROTHER_PT_CBP_MIMETYPE;

  if (!strncmp(driver_name, "brother_ql-", 11))
  {
    // QL-series...

    // Set resolution...
    // TODO: Add support for 300x600dpi mode for QL-570/580N/700/8xx
    data->num_resolution  = 1;
    data->x_resolution[0] = data->y_resolution[0] = 300;
    data->x_default       = data->y_default = data->x_resolution[0];

    // Basically borderless...
    data->left_right = 1;
    data->bottom_top = 1;

    // Supported media...
    data->num_media = ARRAY_SIZE(lprint_brother_ql_media);
    memcpy(data->media, lprint_brother_ql_media, sizeof(lprint_brother_ql_media));

    papplCopyString(data->media_ready[0].size_name, "roll_dk2205-continuous_2.4x3.9in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "continuous", sizeof(data->media_ready[0].type));

    data->num_type = 2;
    data->type[0]  = "labels";
    data->type[1]  = "continuous";
  }
  else
  {
    // PT-series...

    // Set resolution...
    data->num_resolution  = 1;
    data->x_resolution[0] = data->y_resolution[0] = 180;
    data->x_default       = data->y_default = data->x_resolution[0];

    // Basically borderless...
    data->left_right = 1;
    data->bottom_top = 1;

    // Supported media...
    data->num_media = ARRAY_SIZE(lprint_brother_pt_media);
    memcpy(data->media, lprint_brother_pt_media, sizeof(lprint_brother_pt_media));

    data->num_source = 1;
    data->source[0]  = "main-roll";

    papplCopyString(data->media_ready[0].size_name, "oe_wide-2in-tape_1x2in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "continuous", sizeof(data->media_ready[0].type));

    data->num_type = 2;
    data->type[0]  = "continuous";
    data->type[1]  = "continuous-film";
    data->type[2]  = "continuous-removable";
  }

  data->num_source = 1;
  data->source[0]  = "main-roll";

  // 5 darkness/density settings
  data->darkness_configured = 50;
  data->darkness_supported  = 5;

  return (true);
}


//
// 'lprint_brother_get_status()' - Query the printer status information...
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_get_status(
    pappl_printer_t *printer,		// I - Printer
    pappl_device_t  *device)		// I - Device
{
  unsigned char		buffer[32];	// Status buffer
  pappl_preason_t	preasons;	// "printer-state-reasons" values
  const char		*media;		// "media-ready" value


  // Request status...
  if (!papplDevicePuts(device, "\033iS"))
    return (false);

  // Read status buffer...
  if (papplDeviceRead(device, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
    return (false);

  LPRINT_DEBUG("lprint_brother_get_status: Print Head Mark = %02x\n", buffer[0]);
  LPRINT_DEBUG("lprint_brother_get_status: Size = %02x\n", buffer[1]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[2]);
  LPRINT_DEBUG("lprint_brother_get_status: Series Code = %02x\n", buffer[3]);
  LPRINT_DEBUG("lprint_brother_get_status: Model Code = %02x %02x\n", buffer[4], buffer[5]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[6]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[7]);
  LPRINT_DEBUG("lprint_brother_get_status: Error Info 1 = %02x\n", buffer[8]);
  LPRINT_DEBUG("lprint_brother_get_status: Error Info 2 = %02x\n", buffer[9]);
  LPRINT_DEBUG("lprint_brother_get_status: Media Width = %02x\n", buffer[10]);
  LPRINT_DEBUG("lprint_brother_get_status: Media Type = %02x\n", buffer[11]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[12]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[13]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[14]);
  LPRINT_DEBUG("lprint_brother_get_status: Mode = %02x\n", buffer[15]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[16]);
  LPRINT_DEBUG("lprint_brother_get_status: Media Length = %02x\n", buffer[17]);
  LPRINT_DEBUG("lprint_brother_get_status: Status Type = %02x\n", buffer[18]);
  LPRINT_DEBUG("lprint_brother_get_status: Phase Type = %02x\n", buffer[19]);
  LPRINT_DEBUG("lprint_brother_get_status: Phase Number = %02x %02x\n", buffer[20], buffer[21]);
  LPRINT_DEBUG("lprint_brother_get_status: Notification # = %02x\n", buffer[22]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x\n", buffer[23]);
  LPRINT_DEBUG("lprint_brother_get_status: Tape Color = %02x\n", buffer[24]);
  LPRINT_DEBUG("lprint_brother_get_status: Text Color = %02x\n", buffer[25]);
  LPRINT_DEBUG("lprint_brother_get_status: Hardware Info = %02x %02x %02x %02x\n", buffer[26], buffer[27], buffer[28], buffer[29]);
  LPRINT_DEBUG("lprint_brother_get_status: Reserved = %02x %02x\n", buffer[30], buffer[31]);

  // Match ready media...
  if ((media = lprintMediaMatch(printer, 0, 100 * buffer[10], 100 * buffer[17])) != NULL)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Detected media is '%s'.", media);

  // Convert error info to "printer-state-reasons" bits...
  preasons = PAPPL_PREASON_NONE;
  if (buffer[8] & 0x03)
    preasons |= PAPPL_PREASON_MEDIA_EMPTY;
  if (buffer[8] & 0xfc)
    preasons |= PAPPL_PREASON_OTHER;
  if (buffer[9] & 0x01)
    preasons |= PAPPL_PREASON_MEDIA_NEEDED;
  if (buffer[9] & 0x10)
    preasons |= PAPPL_PREASON_COVER_OPEN;
  if (buffer[9] & 0x40)
    preasons |= PAPPL_PREASON_MEDIA_JAM;
  if (buffer[9] & 0xae)
    preasons |= PAPPL_PREASON_OTHER;

  papplPrinterSetReasons(printer, preasons, ~preasons);

  return (true);
}


//
// 'lprint_brother_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer
//  lprint_brother_t	brother;			// Driver data


  // Reset the printer...
  lprint_brother_rstartjob(job, options, device);

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

  lprint_brother_rstartjob(job, options, device);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'lprint_brother_rend()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_brother_t		*brother = (lprint_brother_t *)papplJobGetData(job);
					// Brother driver data

  (void)options;

  papplDevicePuts(device, "\032");	// Eject the last page

  free(brother->buffer);
  free(brother);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_brother_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_brother_t	*brother = (lprint_brother_t *)papplJobGetData(job);
					// Brother driver data
  unsigned char	buffer[13];		// Print Information command buffer


  // Write last line
  lprint_brother_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Send print information...
  buffer[ 0] = 0x1b;
  buffer[ 1] = 'i';
  buffer[ 2] = 'z';
  buffer[ 3] = !strncmp(options->media.type, "continuous", 10) ? 0x04 : 0x0c;
  buffer[ 4] = 0;
  buffer[ 5] = options->media.size_width / 100;
  buffer[ 6] = options->media.size_length / 100;
#if 1
  buffer[ 7] = options->header.cupsHeight & 255;
  buffer[ 8] = (options->header.cupsHeight >> 8) & 255;
  buffer[ 9] = (options->header.cupsHeight >> 16) & 255;
  buffer[10] = (options->header.cupsHeight >> 24) & 255;
#else
  buffer[ 7] = brother->count & 255;
  buffer[ 8] = (brother->count >> 8) & 255;
  buffer[ 9] = (brother->count >> 16) & 255;
  buffer[10] = (brother->count >> 24) & 255;
#endif // 1
  buffer[11] = page == 0 ? 0 : 1;
  buffer[12] = 0;

  if (!papplDeviceWrite(device, buffer, sizeof(buffer)))
    return (false);

  // Send label data...
  if (brother->num_bytes > 0 && !papplDeviceWrite(device, brother->buffer, brother->num_bytes))
    return (false);

  // Eject/cut
  papplDevicePrintf(device, "\033iM%c", !strncmp(options->media.type, "continuous", 10) ? 64 : 0);
  papplDeviceFlush(device);

  // Free memory and return...
  lprintDitherFree(&brother->dither);

  return (true);
}


//
// 'lprint_brother_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_brother_t *brother = (lprint_brother_t *)calloc(1, sizeof(lprint_brother_t));
					// Brother driver data
  const char	*driver_name = papplPrinterGetDriverName(papplJobGetPrinter(job));
					// Driver name
  char		buffer[400];		// Reset buffer
  int		darkness;		// Combined darkness


  (void)options;

  // Save driver data...
  papplJobSetData(job, brother);

  // Reset the printer...
  memset(buffer, 0, sizeof(buffer));
  if (driver_name && !strncmp(driver_name, "brother_pt-", 11))
  {
    // Send short reset sequence for PT-series tape printers
    papplDeviceWrite(device, buffer, 100);
    brother->is_pt_series = true;
  }
  else
  {
    // Send long reset sequence for QL-series label printers
    papplDeviceWrite(device, buffer, sizeof(buffer));

    brother->is_ql_800 = driver_name && !strcmp(driver_name, "brother_ql-800");
  }

  // Get status information...
  lprint_brother_get_status(papplJobGetPrinter(job), device);
//  if (!lprint_brother_get_status(papplJobGetPrinter(job), device))
//    return (false);

  // Reset and set raster mode...
  if (!papplDevicePuts(device, "\033@\033ia\001"))
    return (false);

  // print-darkness / printer-darkness-configured
  if ((darkness = options->print_darkness + options->darkness_configured) < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  return (papplDevicePrintf(device, "\033iD%c", 4 * darkness / 100 + 1));
}


//
// 'lprint_brother_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_brother_t *brother = (lprint_brother_t *)papplJobGetData(job);
					// Brother driver data


  if (page > 0)
    papplDevicePuts(device, "\014");	// Eject the previous page

  if (!lprintDitherAlloc(&brother->dither, job, options, CUPS_CSPACE_K, options->header.HWResolution[0] == 300 ? 1.2 : 1.0))
    return (false);

  brother->count     = 0;
  brother->num_bytes = 0;

  return (true);
}


//
// 'lprint_brother_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_brother_t	*brother = (lprint_brother_t *)papplJobGetData(job);
					// Brother driver data
  unsigned char		*bufptr;	// Pointer into page buffer


  if (!lprintDitherLine(&brother->dither, y, line))
    return (true);

  if ((brother->alloc_bytes - brother->num_bytes) < (3 + brother->dither.out_width))
  {
    size_t temp_alloc = brother->alloc_bytes + brother->dither.out_width + 4096;
				      // New allocated size
    unsigned char *temp = realloc(brother->buffer, temp_alloc);
				      // New buffer

    if (!temp)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate %lu bytes of memory memory.", (unsigned long)temp_alloc);
      return (false);
    }

    brother->alloc_bytes = temp_alloc;
    brother->buffer      = temp;
  }

  bufptr = brother->buffer + brother->num_bytes;

  if (brother->is_ql_800 || brother->dither.output[0] || memcmp(brother->dither.output, brother->dither.output + 1, brother->dither.out_width - 1))
  {
    // Non-blank line...
    // TODO: Add PackBits compression support
    brother->count += 3 + brother->dither.out_width;

    if (brother->is_pt_series)
    {
      *bufptr++ = 'G';
      *bufptr++ = brother->dither.out_width & 255;
      *bufptr++ = (brother->dither.out_width >> 8) & 255;
    }
    else
    {
      *bufptr++ = 'g';
      *bufptr++ = 0;
      *bufptr++ = brother->dither.out_width;
    }

    memcpy(bufptr, brother->dither.output, brother->dither.out_width);
    brother->num_bytes += 3 + brother->dither.out_width;
  }
  else
  {
    // Blank line
    brother->count ++;

    *bufptr = 'Z';
    brother->num_bytes ++;
  }

  return (true);
}


//
// 'lprint_brother_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_brother_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
#endif // LPRINT_EXPERIMENTAL
