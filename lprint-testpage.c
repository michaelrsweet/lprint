//
// Test page generator for LPrint, a Label Printer Application
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
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
		*lineptr,		// Pointer into line
		white = 0x00,		// White pixel
		black = 0xff,		// Black pixel
	        gray0 = 0x88,		// Gray pixel for even lines
	        gray1 = 0x22;		// Gray pixel for odd lines
  unsigned	y,			// Current position in page
		ytop, yend, ybottom,	// Top/end-of-image/bottom lines in page
		x1mm, y1mm,		// Number of bytes/rows for 2mm
		pw, ph;			// Image width and height
  const char	**pixels,		// Output image pixels
		*pixel;			// Current output pixel
  bool		ret = false;		// Return value
  static const char *portrait[] =	// Portrait label image
  {
    "                ",
    " TTT            ",
    " TT             ",
    " TT             ",
    " TT           T ",
    " TTTTTTTTTTTTTT ",
    " TTTTTTTTTTTTTT ",
    " TT           T ",
    " TT             ",
    " TT             ",
    " TTT            ",
    "                ",
    "                ",
    "   SS     SSS   ",
    "  SS     SSSSS  ",
    "  SS    SS  SS  ",
    " SS     SS   SS ",
    " SS    SS    SS ",
    " SS    SS    SS ",
    " SS   SS     SS ",
    "  SS  SS    SS  ",
    "  SSSSS     SS  ",
    "   SSS     SS   ",
    "                ",
    "                ",
    " EEE        EEE ",
    " EE          EE ",
    " EE   EEEE   EE ",
    " EE    EE    EE ",
    " EE    EE    EE ",
    " EE    EE    EE ",
    " EE    EE    EE ",
    " EE    EE    EE ",
    " EEEEEEEEEEEEEE ",
    " EEEEEEEEEEEEEE ",
    "                ",
    "                ",
    " TTT            ",
    " TT             ",
    " TT             ",
    " TT           T ",
    " TTTTTTTTTTTTTT ",
    " TTTTTTTTTTTTTT ",
    " TT           T ",
    " TT             ",
    " TT             ",
    " TTT            ",
    "                "
  };
  static const char *landscape[] =	// Landscape label image
  {
    "                                                ",
    " TTTTTTTTTT  EEEEEEEEEE     SSSS     TTTTTTTTTT ",
    " TTTTTTTTTT  EEEEEEEEEE   SSSSSSSS   TTTTTTTTTT ",
    " T   TT   T  EE       E  SSS    SSS  T   TT   T ",
    "     TT      EE          SS       S      TT     ",
    "     TT      EE          SS              TT     ",
    "     TT      EE     E     SSS            TT     ",
    "     TT      EEEEEEEE      SSSS          TT     ",
    "     TT      EEEEEEEE        SSSS        TT     ",
    "     TT      EE     E          SSS       TT     ",
    "     TT      EE                  SS      TT     ",
    "     TT      EE          S       SS      TT     ",
    "     TT      EE       E  SSS    SSS      TT     ",
    "     TT      EEEEEEEEEE   SSSSSSSS       TT     ",
    "    TTTT     EEEEEEEEEE     SSSS        TTTT    ",
    "                                                "
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

  if ((x1mm = options->header.HWResolution[0] / 200) == 0)
    x1mm = 1;
  if ((y1mm = 8 * (options->header.HWResolution[1] / 200)) == 0)
    y1mm = 1;

  if (options->header.cupsWidth > options->header.cupsHeight)
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

  if (((pw + 2) * x1mm) > options->header.cupsWidth || ((ph + 2) * y1mm) > options->header.cupsHeight)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Label too small to print test page.");
    goto done;
  }

  pw *= x1mm;
  ph *= y1mm;

  ytop    = (options->header.cupsHeight - ph) / 2;
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

  // Top border (2mm)
  memset(line, black, options->header.cupsBytesPerLine);
  for (y = 0; y < y1mm; y ++)
  {
    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders and gray shading on upper half
  for (; y < ytop; y ++)
  {
    memset(line, (y & 1) ? gray1 : gray0, options->header.cupsBytesPerLine);
    memset(line, black, x1mm);
    memset(line + options->header.cupsBytesPerLine - x1mm, black, x1mm);

    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders, gray shading, and image pixels in middle
  for (; y < yend; y ++)
  {
    memset(line, (y & 1) ? gray1 : gray0, options->header.cupsBytesPerLine);
    memset(line, black, x1mm);
    memset(line + options->header.cupsBytesPerLine - x1mm, black, x1mm);

    for (pixel = pixels[(y - ytop) / y1mm], lineptr = line + (options->header.cupsBytesPerLine - pw) / 2; *pixel; pixel ++, lineptr += x1mm)
      memset(lineptr, isspace(*pixel) ? white : black, x1mm);

    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Side borders and gray shading on lower half
  for (; y < ybottom; y ++)
  {
    memset(line, (y & 1) ? gray1 : gray0, options->header.cupsBytesPerLine);
    memset(line, black, x1mm);
    memset(line + options->header.cupsBytesPerLine - x1mm, black, x1mm);

    if (!(data.rwriteline_cb)(job, options, device, y, line))
      goto done;
  }

  // Bottom border (2mm)
  memset(line, black, options->header.cupsBytesPerLine);
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
