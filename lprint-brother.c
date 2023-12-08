//
// Brother driver for LPrint, a Label Printer Application
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

typedef struct lprint_brother_s		// Brother driver data
{
  lprint_dither_t dither;		// Dither buffer
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
    data->num_media = (int)(sizeof(lprint_brother_ql_media) / sizeof(lprint_brother_ql_media[0]));
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
    data->num_media = (int)(sizeof(lprint_brother_pt_media) / sizeof(lprint_brother_pt_media[0]));
    memcpy(data->media, lprint_brother_pt_media, sizeof(lprint_brother_pt_media));

    data->num_source = 1;
    data->source[0]  = "main-roll";

    papplCopyString(data->media_ready[0].size_name, "oe_wide-2in-tape_1x2in", sizeof(data->media_ready[0].size_name));
    papplCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));

    data->num_type = 2;
    data->type[0]  = "continuous";
    data->type[1]  = "continuous-film";
    data->type[2]  = "continuous-removable";
  }

  data->num_source = 1;
  data->source[0]  = "main-roll";


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
  unsigned char	buffer[32];		// Status buffer


  if (!papplDevicePuts(device, "\033iS"))
    return (false);

  if (papplDeviceRead(device, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
    return (false);

  fprintf(stderr, "lprint_brother_get_status: Print Head Mark = %02x\n", buffer[0]);
  fprintf(stderr, "lprint_brother_get_status: Size = %02x\n", buffer[1]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[2]);
  fprintf(stderr, "lprint_brother_get_status: Series Code = %02x\n", buffer[3]);
  fprintf(stderr, "lprint_brother_get_status: Model Code = %02x %02x\n", buffer[4], buffer[5]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[6]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[7]);
  fprintf(stderr, "lprint_brother_get_status: Error Info 1 = %02x\n", buffer[8]);
  fprintf(stderr, "lprint_brother_get_status: Error Info 2 = %02x\n", buffer[9]);
  fprintf(stderr, "lprint_brother_get_status: Media Width = %02x\n", buffer[10]);
  fprintf(stderr, "lprint_brother_get_status: Media Type = %02x\n", buffer[11]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[12]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[13]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[14]);
  fprintf(stderr, "lprint_brother_get_status: Mode = %02x\n", buffer[15]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[16]);
  fprintf(stderr, "lprint_brother_get_status: Media Length = %02x\n", buffer[17]);
  fprintf(stderr, "lprint_brother_get_status: Status Type = %02x\n", buffer[18]);
  fprintf(stderr, "lprint_brother_get_status: Phase Type = %02x\n", buffer[19]);
  fprintf(stderr, "lprint_brother_get_status: Phase Number = %02x %02x\n", buffer[20], buffer[21]);
  fprintf(stderr, "lprint_brother_get_status: Notification # = %02x\n", buffer[22]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x\n", buffer[23]);
  fprintf(stderr, "lprint_brother_get_status: Tape Color = %02x\n", buffer[24]);
  fprintf(stderr, "lprint_brother_get_status: Text Color = %02x\n", buffer[25]);
  fprintf(stderr, "lprint_brother_get_status: Hardware Info = %02x %02x %02x %02x\n", buffer[26], buffer[27], buffer[28], buffer[29]);
  fprintf(stderr, "lprint_brother_get_status: Reserved = %02x %02x\n", buffer[30], buffer[31]);

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


  (void)page;

  // Write last line
  lprint_brother_rwriteline(job, options, device, options->header.cupsHeight, NULL);

  // Eject/cut

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

  (void)options;

  // Save driver data...
  papplJobSetData(job, brother);

  // Reset the printer...
  memset(buffer, 0, sizeof(buffer));
  if (driver_name && !strncmp(driver_name, "brother_qt-", 11))
    papplDeviceWrite(device, buffer, 100);
  else
    papplDeviceWrite(device, buffer, sizeof(buffer));

  // Get status information...
  if (!lprint_brother_get_status(papplJobGetPrinter(job), device))
    return (false);

  // Reset and set raster mode...
  return (papplDevicePuts(device, "\033@\033ia\001"));
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
//  int		darkness = options->darkness_configured + options->print_darkness;
					// Combined density



  (void)page;

  if (!lprintDitherAlloc(&brother->dither, job, options, CUPS_CSPACE_K, options->header.HWResolution[0] == 300 ? 1.2 : 1.0))
    return (false);

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
  lprint_brother_t		*brother = (lprint_brother_t *)papplJobGetData(job);
					// Brother driver data


  if (!lprintDitherLine(&brother->dither, y, line))
    return (true);

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
