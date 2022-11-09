//
// ZIJIANG driver for LPrint, a Label Printer Application
//
// Copyright © 2022 by Abdelhakim Qbaich.
// Copyright © 2019-2022 by Michael R Sweet.
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


// Size in bytes of the bitmap header sent to the device
#define ZJ_HEADER_SIZE 8


//
// Local types...
//

typedef struct lprint_zj_s		// ZIJIANG driver data
{
  unsigned	feed,			// Accumulated feed lines
		lines;			// Accumulated raster lines
  unsigned char	*buffer;		// Print buffer
} lprint_zj_t;


//
// Local globals...
//

static const char * const lprint_zj_58mm_media[] =
{					// Supported 58 mm media sizes
  "roll_min_48x65mm",
  "roll_max_48x3276mm"
};
static const char * const lprint_zj_80mm_media[] =
{					// Supported 80 mm media sizes
  "roll_min_72x65mm",
  "roll_max_72x3276mm"
};


//
// Local functions...
//

static bool	lprint_zj_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zj_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zj_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_zj_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	lprint_zj_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	lprint_zj_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool	lprint_zj_status(pappl_printer_t *printer);


//
// 'lprintZJ()' - Initialize the ZIJIANG driver.
//

bool					// O - `true` on success, `false` on error
lprintZJ(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = lprint_zj_printfile;
  data->rendjob_cb    = lprint_zj_rendjob;
  data->rendpage_cb   = lprint_zj_rendpage;
  data->rstartjob_cb  = lprint_zj_rstartjob;
  data->rstartpage_cb = lprint_zj_rstartpage;
  data->rwriteline_cb = lprint_zj_rwriteline;
  data->status_cb     = lprint_zj_status;
  data->format        = "application/vnd.zijiang-escpos";

  // Only Zijiang receipt printers supported for now
  data->kind = PAPPL_KIND_RECEIPT;

  data->input_face_up  = false;
  data->output_face_up = false;

  data->num_resolution  = 1;
  data->x_resolution[0] = 203;
  data->y_resolution[0] = 203;

  data->x_default = data->y_default = 203;

  if (!strncmp(driver_name, "zj_58", 5))
  {
    // 58 mm printer...
    data->num_media = (int)(sizeof(lprint_zj_58mm_media) / sizeof(lprint_zj_58mm_media[0]));
    memcpy(data->media, lprint_zj_58mm_media, sizeof(lprint_zj_58mm_media));

    papplCopyString(data->media_default.size_name, "roll_current_48x65mm", sizeof(data->media_default.size_name));
    data->media_default.size_width  = 4800;
    data->media_default.size_length = 6500;
  }
  else
  {
    // 80 mm printer...
    data->ppm = 240;

    data->finishings |= PAPPL_FINISHINGS_TRIM;

    data->num_media = (int)(sizeof(lprint_zj_80mm_media) / sizeof(lprint_zj_80mm_media[0]));
    memcpy(data->media, lprint_zj_80mm_media, sizeof(lprint_zj_80mm_media));

    papplCopyString(data->media_default.size_name, "roll_current_72x65mm", sizeof(data->media_default.size_name));
    data->media_default.size_width  = 7200;
    data->media_default.size_length = 6500;
  }

  data->bottom_top = data->left_right = 1;

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->tracking_supported |= PAPPL_MEDIA_TRACKING_CONTINUOUS;

  data->num_type = 1;
  data->type[0]  = "continuous";

  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;
  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.top_margin    = data->bottom_top;
  papplCopyString(data->media_default.source, "main-roll", sizeof(data->media_default.source));
  data->media_default.top_offset    = 0;
  data->media_default.tracking      = PAPPL_MEDIA_TRACKING_CONTINUOUS;
  papplCopyString(data->media_default.type, "continuous", sizeof(data->media_default.type));

  data->media_ready[0] = data->media_default;

  data->tear_offset_configured   = 1500;
  data->tear_offset_supported[0] = 0;
  data->tear_offset_supported[1] = 3000;

  return (true);
}


//
// 'lprintZJQueryDriver()' - Query the printer to determine the proper driver.
//

void
lprintZJQueryDriver(
    pappl_system_t *system,		// I - System
    const char     *device_uri,		// I - Device URI
    char           *name,		// I - Name buffer
    size_t         namesize)		// I - Size of name buffer
{
  // Make sure name buffer is initialized...
  *name = '\0';

  if (strstr(device_uri, "POS58%20Printer") != NULL)
    papplCopyString(name, "zj_58mm", namesize);
  if (strstr(device_uri, "POS80%20Printer") != NULL)
    papplCopyString(name, "zj_80mm", namesize);

  if (*name != '\0')
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "auto driver-name='%s'", name);

  return;
}


//
// 'lprint_zj_printfile()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_printfile(
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
// 'lprint_zj_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  pappl_pr_driver_data_t data;		// Driver data
  lprint_zj_t	*zj = (lprint_zj_t *)papplJobGetData(job);
					// ZIJIANG driver data


  papplPrinterGetDriverData(papplJobGetPrinter(job), &data);

  if (data.tear_offset_configured)
  {
    // Each unit is 0.125 mm
    char feed = data.tear_offset_configured / 12.5 + 0.5;
    papplDevicePrintf(device, "\033J%c", feed);
  }
  if (options->finishings & PAPPL_FINISHINGS_TRIM)
    papplDevicePuts(device, "\035V\001");
  papplDevicePuts(device, "\033@");

  free(zj);
  papplJobSetData(job, NULL);

  return (true);
}


//
// 'lprint_zj_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_zj_t	*zj = (lprint_zj_t *)papplJobGetData(job);
					// ZIJIANG driver data


  (void)page;

  if (zj->lines)
  {
    zj->buffer[6] = zj->lines & 0xff;
    zj->buffer[7] = (zj->lines >> 8) & 0xff;
    papplDeviceWrite(device, zj->buffer, ZJ_HEADER_SIZE + zj->lines * options->header.cupsBytesPerLine);
    papplDevicePrintf(device, "\033J%c", 0);
  }

  if (zj->feed)
  {
    while (zj->feed > 24)
    {
      papplDevicePrintf(device, "\033J%c", 24);
      zj->feed -= 24;
    }

    papplDevicePrintf(device, "\033J%c", zj->feed);
  }

  free(zj->buffer);

  return (true);
}


//
// 'lprint_zj_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_zj_t	*zj = (lprint_zj_t *)calloc(1, sizeof(lprint_zj_t));
					// ZIJIANG driver data


  (void)options;

  papplJobSetData(job, zj);

  papplDevicePuts(device, "\033@");

  return (true);
}


//
// 'lprint_zj_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_zj_t	*zj = (lprint_zj_t *)papplJobGetData(job);
					// ZIJIANG driver data


  (void)page;

  zj->feed  = 0;
  zj->lines = 0;

  // Each unit is 0.125 mm
  int lines = options->media.size_length / 12.5 + 0.5;
  zj->buffer = malloc(ZJ_HEADER_SIZE + lines * options->header.cupsBytesPerLine);

  zj->buffer[0] = 0x1d;
  zj->buffer[1] = 0x76;
  zj->buffer[2] = 0x30;
  zj->buffer[3] = 0x00;
  zj->buffer[4] = options->header.cupsBytesPerLine & 0xff;
  zj->buffer[5] = (options->header.cupsBytesPerLine >> 8) & 0xff;

  return (true);
}


//
// 'lprint_zj_rwriteline()' - Write a raster line.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_zj_t		*zj = (lprint_zj_t *)papplJobGetData(job);
					// ZIJIANG driver data


  (void)y;

  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    if (zj->feed)
    {
      while (zj->feed > 24)
      {
        papplDevicePrintf(device, "\033J%c", 24);
        zj->feed -= 24;
      }

      papplDevicePrintf(device, "\033J%c", zj->feed);
      zj->feed = 0;
    }

    memcpy(zj->buffer + ZJ_HEADER_SIZE + zj->lines * options->header.cupsBytesPerLine, line, options->header.cupsBytesPerLine);
    zj->lines ++;
  }
  else
  {
    if (zj->lines)
    {
      zj->buffer[6] = zj->lines & 0xff;
      zj->buffer[7] = (zj->lines >> 8) & 0xff;
      papplDeviceWrite(device, zj->buffer, ZJ_HEADER_SIZE + zj->lines * options->header.cupsBytesPerLine);
      papplDevicePrintf(device, "\033J%c", 0);
      zj->lines = 0;
    }

    zj->feed ++;
  }

  return (true);
}


//
// 'lprint_zj_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
lprint_zj_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
