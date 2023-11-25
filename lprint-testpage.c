//
// Test page generator for LPrint, a Label Printer Application
//
// Copyright © 2021-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


//
// 'lprintTestFilterCB()' - Print a test page.
//

bool					// O - `true` on success, `false` on failure
lprintTestFilterCB(
    pappl_job_t    *job,		// I - Job
    pappl_device_t *device,		// I - Output device
    void           *cbdata)		// I - Callback data (not used)
{
  pappl_pr_driver_data_t data;		// Printer driver data
  pappl_pr_options_t	*options = papplJobCreatePrintOptions(job, 1, false);
					// Print options
  unsigned char	*line = NULL,		// Output line
		*lineptr;		// Pointer into line
  unsigned	width,			// Width accounting for margins
		height,			// Height accounting for margins
		y,			// Current position in page
		ytop, yend, ybottom,	// Top/end-of-image/bottom lines in page
		xc,			// X count
		x1mm, y1mm,		// Number of columns/rows per pixel (nominally 1mm)
		xleft,			// Start for line
		pw, ph;			// Image width and height
  const char	**pixels,		// Output image pixels
		*pixel;			// Current output pixel
  bool		ret = false;		// Return value
  static const char *portrait[] =	// Portrait label image
  {
    "....            ",
    ".TTT.           ",
    ".TT.            ",
    ".TT.          ..",
    ".TT...........T.",
    ".TTTTTTTTTTTTTT.",
    ".TTTTTTTTTTTTTT.",
    ".TT...........T.",
    ".TT.          ..",
    ".TT.            ",
    ".TTT.           ",
    "....            ",
    "   ..     ...   ",
    "  .SS.   .SSS.  ",
    " .SS.   .SSSSS. ",
    " .SS.  .SS..SS. ",
    ".SS.   .SS. .SS.",
    ".SS.  .SS.  .SS.",
    ".SS.  .SS.  .SS.",
    ".SS. .SS.   .SS.",
    " .SS..SS.  .SS. ",
    " .SSSSS.   .SS. ",
    "  .SSS.   .SS.  ",
    "   ...     ..   ",
    "....        ....",
    ".EEE.      .EEE.",
    ".EE.  ....  .EE.",
    ".EE. .EEEE. .EE.",
    ".EE.  .EE.  .EE.",
    ".EE.  .EE.  .EE.",
    ".EE.  .EE.  .EE.",
    ".EE.  .EE.  .EE.",
    ".EE....EE....EE.",
    ".EEEEEEEEEEEEEE.",
    ".EEEEEEEEEEEEEE.",
    "................",
    "....            ",
    ".TTT.           ",
    ".TT.            ",
    ".TT.          ..",
    ".TT...........T.",
    ".TTTTTTTTTTTTTT.",
    ".TTTTTTTTTTTTTT.",
    ".TT...........T.",
    ".TT.          ..",
    ".TT.            ",
    ".TTT.           ",
    "....            "
  };
  static const char *landscape[] =	// Landscape label image
  {
    "........................    ....    ............",
    ".TTTTTTTTTT..EEEEEEEEEE.  ..SSSS..  .TTTTTTTTTT.",
    ".TTTTTTTTTT..EEEEEEEEEE. .SSSSSSSS. .TTTTTTTTTT.",
    ".T...TT...T..EE.......E..SSS....SSS..T...TT...T.",
    " .  .TT.  . .EE.      . .SS.    ..S. .  .TT.  . ",
    "    .TT.    .EE.    .   .SS..     .     .TT.    ",
    "    .TT.    .EE.....E.   .SSS..         .TT.    ",
    "    .TT.    .EEEEEEEE.    .SSSS..       .TT.    ",
    "    .TT.    .EEEEEEEE.     ..SSSS.      .TT.    ",
    "    .TT.    .EE.....E.       ..SSS.     .TT.    ",
    "    .TT.    .EE.    .    .     ..SS.    .TT.    ",
    "    .TT.    .EE.      . .S.     .SS.    .TT.    ",
    "    .TT.    .EE.......E..SSS....SSS.    .TT.    ",
    "    .TT.    .EEEEEEEEEE. .SSSSSSSS.     .TT.    ",
    "   .TTTT.   .EEEEEEEEEE.  ..SSSS..     .TTTT.   ",
    "   ......   ............    ....       ......   "
  };


  (void)cbdata;

  // Validate options and allocate a line buffer...
  if (!options)
    goto done;

  if ((line = malloc(options->header.cupsBytesPerLine)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate %u bytes for test page: %s", options->header.cupsBytesPerLine, strerror(errno));
    goto done;
  }

  width  = options->header.HWResolution[0] * (options->media.size_width - options->media.left_margin - options->media.right_margin) / 2540;
  height = options->header.HWResolution[1] * (options->media.size_length - options->media.bottom_margin - options->media.top_margin) / 2540;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Printable area for test page is %ux%u pixels.", width, height);

  if (width > height)
  {
    // Send landscape raster...
    pixels = landscape;
    pw     = 48;
    ph     = 16;
  }
  else
  {
    // Send portrait raster...
    pixels = portrait;
    pw     = 16;
    ph     = 48;
  }

  y1mm = 10 * options->header.HWResolution[1] / 254;
  if ((y1mm * ph) > height)
  {
    if ((y1mm = height / ph) == 0)
      y1mm = 1;
  }

  if ((x1mm = y1mm * options->header.HWResolution[0] / options->header.HWResolution[1]) == 0)
  {
    x1mm = 1;
    y1mm = x1mm * options->header.HWResolution[1] / options->header.HWResolution[0];
  }

  if ((pw * x1mm) > width)
  {
    // Too wide, try scaling to fit the width...
    x1mm = 10 * options->header.HWResolution[0] / 254;
    if ((x1mm * pw) > width)
      x1mm = width / pw;

    if ((y1mm = x1mm * options->header.HWResolution[1] / options->header.HWResolution[0]) == 0)
    {
      y1mm = 1;
      x1mm = y1mm * options->header.HWResolution[0] / options->header.HWResolution[1];
    }
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "pw=%u, ph=%u, x1mm=%u, y1mm=%u", pw, ph, x1mm, y1mm);

  if (((pw + 2) * x1mm) > options->header.cupsWidth || ((ph + 2) * y1mm) > options->header.cupsHeight)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Label too small to print test page.");
    goto done;
  }

  pw *= x1mm;
  ph *= y1mm;

  xleft   = (width - pw) / 2 - x1mm + options->header.HWResolution[0] * options->media.left_margin / 2540;
  ytop    = (height - ph) / 2 + options->header.HWResolution[1] * options->media.top_margin / 2540;
  yend    = ytop + ph;
  ybottom = options->header.cupsHeight - y1mm;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "pw=%u, ph=%u, ytop=%u, yend=%u, ybottom=%u", pw, ph, ytop, yend, ybottom);

  // Start the page...
  papplPrinterGetDriverData(papplJobGetPrinter(job), &data);

  papplJobSetImpressions(job, 1);

  if (!(data.rstartjob_cb)(job, options, device))
    goto done;

  if (!(data.rstartpage_cb)(job, options, device, 1))
    goto done;

  // Send lines for the test label:
  //
  // +---------------+
  // |...............|
  // |...         ...|
  // |... T E S T ...|
  // |...         ...|
  // |...............|
  // +---------------+

  // Top border
  memset(line, 0, options->header.cupsBytesPerLine);
  for (y = 0; y < y1mm; y ++)
  {
    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders and gray shading on upper half
  memset(line, 0, x1mm);
  memset(line + x1mm, 128, options->header.cupsBytesPerLine - 2 * x1mm);
  memset(line + options->header.cupsBytesPerLine - x1mm, 0, x1mm);

  for (; y < ytop; y ++)
  {
    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders, gray shading, and image pixels in middle
  for (; y < yend; y ++)
  {
    // Draw a 50% gray pattern with black borders...
    memset(line, 0, x1mm);
    memset(line + x1mm, 128, options->header.cupsBytesPerLine - 2 * x1mm);
    memset(line + options->header.cupsBytesPerLine - x1mm, 0, x1mm);

    // Render the interior text...
    for (pixel = pixels[(y - ytop) / y1mm], lineptr = line + xleft; *pixel; pixel ++)
    {
      for (xc = x1mm; xc > 0; xc --, lineptr ++)
      {
        if (*pixel == '.')
	  *lineptr = 255;
        else if (!isspace(*pixel))
	  *lineptr = 0;
      }
    }

    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders and gray shading on lower half
  memset(line, 0, x1mm);
  memset(line + x1mm, 128, options->header.cupsBytesPerLine - 2 * x1mm);
  memset(line + options->header.cupsBytesPerLine - x1mm, 0, x1mm);

  for (; y < ybottom; y ++)
  {
    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Bottom border
  memset(line, 0, options->header.cupsBytesPerLine);
  for (; y < options->header.cupsHeight; y ++)
  {
    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Finish up...
  if (!(data.rendpage_cb)(job, options, device, 1))
    goto done;

  if (!(data.rendjob_cb)(job, options, device))
    goto done;

  ret = true;

  papplJobSetImpressionsCompleted(job, 1);

  done:

  free(line);
  papplJobDeletePrintOptions(options);

  return (ret);
}


//
// 'lprintTestPageCB()' - Print a test page.
//

const char *				// O - Test page file
lprintTestPageCB(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - Filename buffer
    size_t          bufsize)		// I - Size of filename buffer
{
  int	fd;				// File descriptor
  char	testpage[] = LPRINT_TESTPAGE_HEADER;
					// Test page file header


  if ((fd = papplCreateTempFile(buffer, bufsize, "testpage", "tst")) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create temporary file for test page: %s", strerror(errno));
    return (NULL);
  }

  if (write(fd, testpage, sizeof(testpage)) < 0)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to write temporary file for test page: %s", strerror(errno));
    close(fd);
    return (NULL);
  }

  close(fd);

  return (buffer);
}
