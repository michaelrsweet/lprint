//
// Print functions for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"
#include "dither.h"
#ifdef HAVE_LIBPNG
#  include <png.h>
#endif // HAVE_LIBPNG


//
// Local functions...
//

static ipp_attribute_t *find_attr(lprint_job_t *job, const char *name, ipp_tag_t value_tag);
static void	prepare_options(lprint_job_t *job, lprint_options_t *options, unsigned num_pages);
#ifdef HAVE_LIBPNG
static void	process_png(lprint_job_t *job);
#endif // HAVE_LIBPNG
static void	process_raster(lprint_job_t *job);
static void	process_raw(lprint_job_t *job);


//
// 'lprintProcessJob()' - Process a print job.
//

void *					// O - Thread exit status
lprintProcessJob(lprint_job_t *job)	// I - Job
{
  int	first_open = 1;			// Is this the first time we try to open the device?


  // Move the job to the processing state...
  pthread_rwlock_wrlock(&job->rwlock);

  job->state                   = IPP_JSTATE_PROCESSING;
  job->processing              = time(NULL);
  job->printer->processing_job = job;

  pthread_rwlock_wrlock(&job->rwlock);

  // Open the output device...
  pthread_rwlock_wrlock(&job->printer->driver->rwlock);

  while (!job->printer->driver->device)
  {
    job->printer->driver->device = lprintOpenDevice(job->printer->device_uri);

    if (!job->printer->driver->device)
    {
      // Log that the printer is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        lprintLogPrinter(job->printer, LPRINT_LOGLEVEL_ERROR, "Unable to open device '%s', pausing queue until printer becomes available.", job->printer->device_uri);
        first_open = 0;

	job->printer->state      = IPP_PSTATE_STOPPED;
	job->printer->state_time = time(NULL);
      }

      sleep(5);
    }
  }

  pthread_rwlock_unlock(&job->printer->driver->rwlock);

  // Process the job...
  job->printer->state      = IPP_PSTATE_PROCESSING;
  job->printer->state_time = time(NULL);

  if (!strcmp(job->format, "image/pwg-raster") || !strcmp(job->format, "image/urf"))
  {
    process_raster(job);
  }
#ifdef HAVE_LIBPNG
  else if (!strcmp(job->format, "image/png"))
  {
    process_png(job);
  }
#endif // HAVE_LIBPNG
  else if (!strcmp(job->format, job->printer->driver->format))
  {
    process_raw(job);
  }
  else
  {
    // Abort a job we can't process...
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to process job with format '%s'.", job->format);
    job->state = IPP_JSTATE_ABORTED;
  }

  // Move the job to a completed state...
  pthread_rwlock_wrlock(&job->rwlock);

  if (job->cancel)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  job->completed               = time(NULL);
  job->printer->state          = IPP_PSTATE_IDLE;
  job->printer->state_time     = time(NULL);
  job->printer->processing_job = NULL;

  pthread_rwlock_wrlock(&job->rwlock);

  pthread_rwlock_wrlock(&job->printer->rwlock);

  cupsArrayRemove(job->printer->active_jobs, job);
  cupsArrayAdd(job->printer->completed_jobs, job);

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&job->printer->rwlock);

  if (job->printer->is_deleted)
  {
    lprintDeletePrinter(job->printer);
  }
  else if (cupsArrayCount(job->printer->active_jobs) > 0)
  {
    lprintCheckJobs(job->printer);
  }
  else
  {
    pthread_rwlock_wrlock(&job->printer->driver->rwlock);

    lprintCloseDevice(job->printer->driver->device);
    job->printer->driver->device = NULL;

    pthread_rwlock_unlock(&job->printer->driver->rwlock);
  }

  return (NULL);
}


//
// 'find_attr()' - Find a matching attribute for a job.
//

static ipp_attribute_t *		// O - Attribute
find_attr(lprint_job_t *job,		// I - Job
          const char   *name,		// I - Attribute name
          ipp_tag_t    value_tag)	// I - Value tag
{
  char			defname[256];	// xxx-default attribute
  ipp_attribute_t	*attr;		// Attribute

  if ((attr = ippFindAttribute(job->attrs, name, value_tag)) != NULL)
    return (attr);

  snprintf(defname, sizeof(defname), "%s-default", name);

  if ((attr = ippFindAttribute(job->printer->attrs, defname, value_tag)) != NULL)
    return (attr);

  return (ippFindAttribute(job->printer->driver->attrs, defname, value_tag));
}


//
// 'prepare_options()' - Prepare the job options.
//

static void
prepare_options(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options data
    unsigned         num_pages)		// I - Number of pages
{
  int			i;		// Looping var
  ipp_attribute_t	*attr;		// Attribute
  lprint_driver_t	*driver = job->printer->driver;
					// Driver info


  // Clear all options...
  memset(options, 0, sizeof(lprint_options_t));

  options->num_pages        = num_pages;
  options->media_top_offset = driver->top_offset_default;
  options->media_tracking   = lprintMediaTrackingString(driver->tracking_default);
  options->media_type       = driver->type_default;

  pthread_rwlock_rdlock(&job->printer->rwlock);
  pthread_rwlock_rdlock(&job->printer->driver->rwlock);

  // copies
  if ((attr = find_attr(job, "copies", IPP_TAG_INTEGER)) != NULL)
    options->copies = ippGetInteger(attr, 0);
  else
    options->copies = 1;

  // media-xxx
  if ((attr = find_attr(job, "media-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    ipp_t *col = ippGetCollection(attr, 0);
    ipp_attribute_t	*size_name = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO),
			*x_dimension = ippFindAttribute(col, "media-size/x-dimension", IPP_TAG_INTEGER),
			*y_dimension = ippFindAttribute(col, "media-size/y-dimension", IPP_TAG_INTEGER),
			*source = ippFindAttribute(col, "media-source", IPP_TAG_ZERO),
			*top_offset = ippFindAttribute(col, "media-top-offset", IPP_TAG_INTEGER),
			*tracking = ippFindAttribute(col, "media-tracking", IPP_TAG_ZERO),
			*type = ippFindAttribute(col, "media-type", IPP_TAG_ZERO);

    if (size_name)
    {
      options->media_size_name = ippGetString(size_name, 0, NULL);
      options->media_size      = pwgMediaForPWG(options->media_size_name);
    }
    else if (x_dimension && y_dimension)
    {
      options->media_size      = pwgMediaForSize(ippGetInteger(x_dimension, 0), ippGetInteger(y_dimension, 0));
      options->media_size_name = options->media_size->pwg;
    }
    else
    {
      options->media_size_name = job->printer->driver->media_default;
      options->media_size      = pwgMediaForPWG(options->media_size_name);
    }

    if (source)
      options->media_source = ippGetString(source, 0, NULL);
    if (top_offset)
      options->media_top_offset = ippGetInteger(top_offset, 0);
    if (tracking)
      options->media_tracking = ippGetString(tracking, 0, NULL);
    if (type)
      options->media_type = ippGetString(type, 0, NULL);
  }
  else if ((attr = find_attr(job, "media", IPP_TAG_ZERO)) != NULL)
  {
    options->media_size_name = ippGetString(attr, 0, NULL);
    options->media_size      = pwgMediaForPWG(options->media_size_name);
  }
  else
  {
    options->media_size_name = job->printer->driver->media_default;
    options->media_size      = pwgMediaForPWG(options->media_size_name);
  }

  if (!options->media_source)
  {
    for (i = 0; i < driver->num_source; i ++)
    {
      if (!strcmp(options->media_size_name, driver->media_ready[i]))
      {
        options->media_source = driver->source[i];
        break;
      }
    }

    if (!options->media_source)
      options->media_source = driver->source_default;
  }

  // orientation-requested
  if ((attr = find_attr(job, "orientation-requsted", IPP_TAG_ENUM)) != NULL)
    options->orientation_requested = (ipp_orient_t)ippGetInteger(attr, 0);
  else
    options->orientation_requested = IPP_ORIENT_PORTRAIT;

  // print-color-mode
  if ((attr = find_attr(job, "print-color-mode", IPP_TAG_KEYWORD)) != NULL)
    options->print_color_mode = ippGetString(attr, 0, NULL);
  else
    options->print_color_mode = "bi-level";

  if (!strcmp(options->print_color_mode, "bi-level"))
    options->dither = dithert;
  else
    options->dither = ditherc;

  // print-content-optimize
  if ((attr = find_attr(job, "print-content-optimize", IPP_TAG_KEYWORD)) != NULL)
    options->print_color_mode = ippGetString(attr, 0, NULL);
  else
    options->print_content_optimize = "auto";

  // print-darkness
  if ((attr = find_attr(job, "print-darkness", IPP_TAG_INTEGER)) != NULL)
    options->print_darkness = ippGetInteger(attr, 0);

  // print-quality
  if ((attr = find_attr(job, "print-quality", IPP_TAG_ENUM)) != NULL)
    options->print_quality = (ipp_quality_t)ippGetInteger(attr, 0);
  else
    options->print_quality = IPP_QUALITY_NORMAL;

  // print-speed
  if ((attr = find_attr(job, "print-speed", IPP_TAG_INTEGER)) != NULL)
    options->print_speed = ippGetInteger(attr, 0);
  else
    options->print_speed = driver->speed_default;

  // printer-resolution
  if ((attr = find_attr(job, "printer-resolution", IPP_TAG_RESOLUTION)) != NULL)
  {
    ipp_res_t	units;			// Resolution units

    options->printer_resolution[0] = ippGetResolution(attr, 0, options->printer_resolution + 1, &units);
  }
  else if (options->print_quality == IPP_QUALITY_DRAFT)
  {
    // print-quality=draft
    options->printer_resolution[0] = driver->x_resolution[0];
    options->printer_resolution[1] = driver->y_resolution[0];
  }
  else if (options->print_quality == IPP_QUALITY_NORMAL)
  {
    // print-quality=normal
    i = driver->num_resolution / 2;
    options->printer_resolution[0] = driver->x_resolution[i];
    options->printer_resolution[1] = driver->y_resolution[i];
  }
  else
  {
    // print-quality=high
    i = driver->num_resolution - 1;
    options->printer_resolution[0] = driver->x_resolution[i];
    options->printer_resolution[1] = driver->y_resolution[i];
  }

  // Figure out the PWG raster header...
  cupsRasterInitPWGHeader(&options->header, options->media_size, "black_1", options->printer_resolution[0], options->printer_resolution[1], "one-sided", "normal");

  // Log options...
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsWidth=%u", options->header.cupsWidth);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsHeight=%u", options->header.cupsHeight);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsBitsPerColor=%u", options->header.cupsBitsPerColor);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsBitsPerPixel=%u", options->header.cupsBitsPerPixel);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsBytesPerLine=%u", options->header.cupsBytesPerLine);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsColorOrder=%u", options->header.cupsColorOrder);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsColorSpace=%u", options->header.cupsColorSpace);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.cupsNumColors=%u", options->header.cupsNumColors);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "header.HWResolution=[%u %u]", options->header.HWResolution[0], options->header.HWResolution[1]);

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "num_pages=%u", options->num_pages);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "copies=%d", options->copies);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_size=%dx%d", options->media_size->width, options->media_size->length);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_size_name='%s'", options->media_size_name);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_source='%s'", options->media_source);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_top_offset=%d", options->media_top_offset);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_tracking='%s'", options->media_tracking);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "media_type='%s'", options->media_type);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "orientation_requested=%d", (int)options->orientation_requested);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "print_color_mode='%s'", options->print_color_mode);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "print_content_optimize='%s'", options->print_content_optimize);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "print_darkness=%d", options->print_darkness);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "print_quality=%d", (int)options->print_quality);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "print_speed=%d", options->print_speed);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "printer_resolution=%dx%d", options->printer_resolution[0], options->printer_resolution[1]);

  pthread_rwlock_unlock(&job->printer->driver->rwlock);
  pthread_rwlock_unlock(&job->printer->rwlock);
}


//
// 'process_png()' - Process a PNG image file.
//

#ifdef HAVE_LIBPNG
static void
process_png(lprint_job_t *job)		// I - Job
{
  int			i;		// Looping var
  lprint_driver_t	*driver = job->printer->driver;
					// Driver
  const unsigned char	*dither;	// Dither line
  lprint_options_t	options;	// Job options
  png_image		png;		// PNG image data
  png_color		bg;		// Background color
  unsigned		iwidth,		// Imageable width
			iheight;	// Imageable length/height
  unsigned char		*line = NULL,	// Output line
			*lineptr,	// Pointer in line
			byte,		// Byte in line
			*pixels,	// Pixels in image
			*pixptr,	// Pointer into image
			bit;		// Current bit
  unsigned		x,		// X position
			xsize,		// Scaled width
			xstep,		// X step
			xstart,		// X start position
			xend,		// X end position
			y,		// Y position
			ysize,		// Scaled height
			ystart,		// Y start position
			yend;		// Y end position
  int			xerr,		// X error accumulator
			xmod;		// X modulus


  // Prepare options...
  prepare_options(job, &options, 1);
  options.header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount] = options.copies;
  job->impressions = options.copies;

  iwidth  = options.header.cupsWidth - 2 * job->printer->driver->left_right * options.printer_resolution[0] / 2540;
  iheight = options.header.cupsHeight - 2 * job->printer->driver->bottom_top * options.printer_resolution[1] / 2540;

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "iwidth=%u, iheight=%u", iwidth, iheight);

  // Load the PNG...
  memset(&png, 0, sizeof(png));
  png.version = PNG_IMAGE_VERSION;

  bg.red = bg.green = bg.blue = 255;

  png_image_begin_read_from_file(&png, job->filename);

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.version=%u", png.version);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.width=%u", png.width);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.height=%u", png.height);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.format=%u", png.format);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.flags=%u", png.flags);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "png.colormap_entries=%u", png.colormap_entries);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "BEGIN png.warning_or_error=%u", png.warning_or_error);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "BEGIN png.message='%s'", png.message);

  png.format = PNG_FORMAT_GRAY;
  pixels     = malloc(PNG_IMAGE_SIZE(png));

  png_image_finish_read(&png, &bg, pixels, 0, NULL);

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "FINISH png.warning_or_error=%u", png.warning_or_error);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "FINISH png.message='%s'", png.message);

  if (png.warning_or_error & PNG_IMAGE_ERROR)
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to open PNG file '%s' - %s", job->filename, png.message);
    goto abort_job;
  }

  // TODO: support PNG rotation

  // Figure out the scaling and rotation of the image...
  xsize = iwidth;
  ysize = xsize * png.height / png.width;
  if (ysize > iheight)
  {
    ysize = iheight;
    xsize = ysize * png.width / png.height;
  }

  xstart = (options.header.cupsWidth - xsize) / 2;
  xend   = xstart + xsize;
  ystart = (options.header.cupsHeight - ysize) / 2;
  yend   = ystart + ysize;

  xmod   = png.width % xsize;
  xstep  = png.width / xsize;

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "xsize=%u, xstart=%u, xend=%u, xmod=%d, xstep=%d", xsize, xstart, xend, xmod, xstep);
  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "ysize=%u, ystart=%u, yend=%u", ysize, ystart, yend);

  // Start the job...
  if (!(driver->rstartjob)(job, &options))
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to start raster job.");
    goto abort_job;
  }

  line = malloc(options.header.cupsBytesPerLine);

  // Print every copy...
  for (i = 0; i < options.copies; i ++)
  {
    if (!(driver->rstartpage)(job, &options, 1))
    {
      lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to start raster page.");
      goto abort_job;
    }

    // Leading blank space...
    memset(line, 0, options.header.cupsBytesPerLine);
    for (y = 0; y < ystart; y ++)
    {
      if (!(driver->rwrite)(job, &options, y, line))
      {
	lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // Now dither the image...
    for (; y < yend; y ++)
    {
      pixptr = pixels + ((y - ystart) * png.height / ysize) * png.width;
      dither = options.dither[y & 15];

      for (x = xstart, lineptr = line + x / 8, bit = 128 >> (x & 7), byte = 0, xerr = 0; x < xend; x ++)
      {
	// Dither the current pixel...
	if (*pixptr <= dither[x & 15])
	  byte |= bit;

	// Advance to the next pixel...
	pixptr += xstep;
	xerr += xmod;
	if (xerr >= xsize)
	{
	  // Accumulated error has overflowed, advance another pixel...
	  xerr -= xsize;
	  pixptr ++;
	}

	// and the next bit
	if (bit == 1)
	{
	  // Current byte is "full", save it...
	  *lineptr++ = byte;
	  byte = 0;
	  bit  = 128;
	}
	else
	  bit /= 2;
      }

      if (bit < 128)
	*lineptr = byte;

      if (!(driver->rwrite)(job, &options, y, line))
      {
	lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // Trailing blank space...
    memset(line, 0, options.header.cupsBytesPerLine);
    for (; y < options.header.cupsHeight; y ++)
    {
      if (!(driver->rwrite)(job, &options, y, line))
      {
	lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to write raster line %u.", y);
	goto abort_job;
      }
    }

    // End the page...
    if (!(driver->rendpage)(job, &options, 1))
    {
      lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to end raster page.");
      goto abort_job;
    }

    job->impcompleted ++;
  }

  // End the job...
  if (!(driver->rendjob)(job, &options))
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to end raster job.");
    goto abort_job;
  }

  // Free the image data when we're done...
  png_image_free(&png);
  free(pixels);
  free(line);

  return;

  // If we get there then something bad happened...
  abort_job:

  job->state = IPP_JSTATE_ABORTED;

  // Free the image data when we're done...
  png_image_free(&png);
  free(pixels);
  free(line);
}
#endif // HAVE_LIBPNG


//
// 'process_raster()' - Process an Apple/PWG Raster file.
//

static void
process_raster(lprint_job_t *job)	// I - Job
{
  lprint_driver_t	*driver = job->printer->driver;
					// Driver for job
  lprint_options_t	options;	// Job options
  int			fd = -1;	// Job file
  cups_raster_t		*ras = NULL;	// Raster stream
  cups_page_header2_t	header;		// Page header
  unsigned char		*line;		// Pixel line
  unsigned		page = 0,	// Current page
			y;		// Current line


  // Open the raster stream...
  if ((fd = open(job->filename, O_RDONLY)) < 0)
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to open job file '%s' - %s", job->filename, strerror(errno));
    goto abort_job;
  }

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_READ)) == NULL)
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to open raster stream for file '%s' - %s", job->filename, cupsLastErrorString());
    goto abort_job;
  }

  // Prepare options...
  if (!cupsRasterReadHeader2(ras, &header))
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to read raster stream for file '%s' - %s", job->filename, cupsLastErrorString());
    goto abort_job;
  }

  job->impressions = (int)header.cupsInteger[CUPS_RASTER_PWG_TotalPageCount];
  prepare_options(job, &options, job->impressions);

  if (!(driver->rstartjob)(job, &options))
    goto abort_job;

  // Print pages...
  do
  {
    page ++;
    job->impcompleted ++;

    if (!(driver->rstartpage)(job, &options, page))
      goto abort_job;

    line = malloc(header.cupsBytesPerLine);

    for (y = 0; y < header.cupsHeight; y ++)
    {
      if (cupsRasterReadPixels(ras, line, header.cupsBytesPerLine))
        (driver->rwrite)(job, &options, y, line);
      else
        break;
    }

    free(line);

    if (!(driver->rendpage)(job, &options, page))
      goto abort_job;

    if (y < header.cupsHeight)
    {
      lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to read page from raster stream for file '%s' - %s", job->filename, cupsLastErrorString());
      (driver->rendjob)(job, &options);
      goto abort_job;
    }
  }
  while (cupsRasterReadHeader2(ras, &header));

  if (!(driver->rendjob)(job, &options))
    goto abort_job;

  cupsRasterClose(ras);
  close(fd);

  return;

  // If we get here something went wrong...
  abort_job:

  if (ras)
    cupsRasterClose(ras);
  if (fd >= 0)
    close(fd);

  job->state = IPP_JSTATE_ABORTED;
}


//
// 'process_raw()' - Process a raw print file.
//

static void
process_raw(lprint_job_t *job)		// I - Job
{
  lprint_options_t	options;	// Job options


  prepare_options(job, &options, 1);
  if (!(job->printer->driver->print)(job, &options))
    job->state = IPP_JSTATE_ABORTED;
}
