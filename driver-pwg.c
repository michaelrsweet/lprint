//
// PWG Raster test driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// local typs...
//

typedef struct lprint_pwg_s
{
  int		fd;			// Output file descriptor
  cups_raster_t	*ras;			// PWG raster file
} lprint_pwg_t;


//
// Local globals...
//

static const char * const lprint_pwg_2inch_media[] =
{					// Supported media sizes
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",

  "roll_max_2x3600in",
  "roll_min_0.25x0.25in"
};

static const char * const lprint_pwg_4inch_media[] =
{					// Supported media sizes
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_multipurpose-label_2x2.3125in",
  "na_index-3x5_3x5in",
  "na_index-4x6_4x6in",

  "roll_max_4x3600in",
  "roll_min_0.25x0.25in"
};


//
// Local functions...
//

static int	lprint_pwg_print(lprint_job_t *job, lprint_options_t *options);
static int	lprint_pwg_rendjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_pwg_rendpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_pwg_rstartjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_pwg_rstartpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_pwg_rwrite(lprint_job_t *job, lprint_options_t *options, unsigned y, const unsigned char *line);
static int	lprint_pwg_status(lprint_printer_t *printer);


//
// 'lprintInitPWG()' - Initialize the driver.
//

void
lprintInitPWG(
    lprint_driver_t *driver)		// I - Driver
{
  int	i;				// Looping var


  pthread_rwlock_wrlock(&driver->rwlock);

  driver->print      = lprint_pwg_print;
  driver->rendjob    = lprint_pwg_rendjob;
  driver->rendpage   = lprint_pwg_rendpage;
  driver->rstartjob  = lprint_pwg_rstartjob;
  driver->rstartpage = lprint_pwg_rstartpage;
  driver->rwrite     = lprint_pwg_rwrite;
  driver->status     = lprint_pwg_status;
  driver->format     = "image/pwg-raster";

  driver->num_resolution  = 2;
  driver->x_resolution[0] = 203;
  driver->y_resolution[0] = 203;
  driver->x_resolution[1] = 300;
  driver->y_resolution[1] = 300;

  driver->left_right = 312;		// 1/16" left and right
  driver->bottom_top = 625;		// 1/8" top and bottom

  if (!strcmp(driver->name, "pwg_2inch"))
  {
    driver->num_media = (int)(sizeof(lprint_pwg_2inch_media) / sizeof(lprint_pwg_2inch_media[0]));
    memcpy(driver->media, lprint_pwg_2inch_media, sizeof(lprint_pwg_2inch_media));

    driver->num_source = 1;
    driver->source[0]  = "main-roll";

    strlcpy(driver->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_ready[0].size_name));
  }
  else
  {
    driver->num_media = (int)(sizeof(lprint_pwg_4inch_media) / sizeof(lprint_pwg_4inch_media[0]));
    memcpy(driver->media, lprint_pwg_4inch_media, sizeof(lprint_pwg_4inch_media));

    driver->num_source = 2;
    driver->source[0]  = "main-roll";
    driver->source[1]  = "alternate-roll";

    strlcpy(driver->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_ready[0].size_name));
    strlcpy(driver->media_ready[1].size_name, "na_index-4x6_4x6in", sizeof(driver->media_ready[1].size_name));
  }

  driver->media_default.bottom_margin = driver->bottom_top;
  driver->media_default.left_margin   = driver->left_right;
  driver->media_default.right_margin  = driver->left_right;
  driver->media_default.size_width    = 3175;
  driver->media_default.size_length   = 8890;
  driver->media_default.top_margin    = driver->bottom_top;
  driver->media_default.tracking      = LPRINT_MEDIA_TRACKING_MARK;
  strlcpy(driver->media_default.size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_default.size_name));
  strlcpy(driver->media_default.source, "main-roll", sizeof(driver->media_default.source));
  strlcpy(driver->media_default.type, "labels", sizeof(driver->media_default.type));

  for (i = 0; i < driver->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(driver->media_ready[i].size_name);

    driver->media_ready[i].bottom_margin = driver->bottom_top;
    driver->media_ready[i].left_margin   = driver->left_right;
    driver->media_ready[i].right_margin  = driver->left_right;
    driver->media_ready[i].size_width    = pwg->width;
    driver->media_ready[i].size_length   = pwg->length;
    driver->media_ready[i].top_margin    = driver->bottom_top;
    driver->media_ready[i].tracking      = LPRINT_MEDIA_TRACKING_MARK;
    strlcpy(driver->media_ready[i].source, driver->source[i], sizeof(driver->media_ready[i].source));
    strlcpy(driver->media_ready[i].type, "labels", sizeof(driver->media_ready[i].type));
  }

  driver->tracking_supported = LPRINT_MEDIA_TRACKING_MARK | LPRINT_MEDIA_TRACKING_CONTINUOUS;

  driver->num_type = 3;
  driver->type[0]  = "continuous";
  driver->type[1]  = "labels";
  driver->type[2]  = "labels-continuous";

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_pwg_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_print(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  int		infd,			// Input file
		outfd;			// Output file
  char		outname[1024];		// Output filename
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  job->impressions = 1;

  infd  = open(job->filename, O_RDONLY);
  outfd = lprintCreateJobFile(job, outname, sizeof(outname), job->system->directory, "out.pwg");

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
    write(outfd, buffer, (size_t)bytes);

  close(infd);
  close(outfd);

  job->impcompleted = 1;

  return (1);
}


//
// 'lprint_pwg_rendjob()' - End a job.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rendjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_pwg_t	*pwg = (lprint_pwg_t *)job->printer->driver->job_data;

  (void)options;

  cupsRasterClose(pwg->ras);
  close(pwg->fd);

  free(pwg);
  job->printer->driver->job_data = NULL;

  return (1);
}


//
// 'lprint_pwg_rendpage()' - End a page.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rendpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  (void)job;
  (void)options;
  (void)page;

  return (1);
}


//
// 'lprint_pwg_rstartjob()' - Start a job.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rstartjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_pwg_t	*pwg = (lprint_pwg_t *)calloc(1, sizeof(lprint_pwg_t));
					// PWG driver data
  char		outname[1024];		// Output filename


  (void)options;

  job->printer->driver->job_data = pwg;

  pwg->fd  = lprintCreateJobFile(job, outname, sizeof(outname), job->system->directory, "out.pwg");
  pwg->ras = cupsRasterOpen(pwg->fd, CUPS_RASTER_WRITE_PWG);

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "Writing PWG output to '%s'.", outname);

  return (1);
}


//
// 'lprint_pwg_rstartpage()' - Start a page.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rstartpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_pwg_t	*pwg = (lprint_pwg_t *)job->printer->driver->job_data;
					// PWG driver data

  (void)page;

  return (cupsRasterWriteHeader2(pwg->ras, &options->header));
}


//
// 'lprint_pwg_rwrite()' - Write a raster line.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_rwrite(
    lprint_job_t        *job,		// I - Job
    lprint_options_t    *options,	// I - Job options
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_pwg_t	*pwg = (lprint_pwg_t *)job->printer->driver->job_data;
					// PWG driver data

  (void)y;

  return (cupsRasterWritePixels(pwg->ras, (unsigned char *)line, options->header.cupsBytesPerLine));
}


//
// 'lprint_pwg_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_pwg_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
