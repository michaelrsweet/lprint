//
// Common driver code for LPrint, a Label Printer Application
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// 'lprintDitherAlloc()' - Allocate memory for a dither buffer.
//

bool					// O - `true` on success, `false` on error
lprintDitherAlloc(
    lprint_dither_t    *dither,		// I - Dither buffer
    pappl_pr_options_t *options,	// I - Print options
    cups_page_header_t *in_header,	// I - Input raster header
    double             out_gamma)	// I - Output gamma correction
{
  int		i, j;			// Looping vars
  unsigned	right;			// Right margin


  // Adjust dithering array and compress to a range of 16 to 239
  for (i = 0; i < 16; i ++)
  {
    for (j = 0; j < 16; j ++)
    {
      dither->dither[i][j] = (unsigned char)(223.0 * pow(options->dither[i][j] / 255.0, out_gamma) + 16.0);
    }
  }

  // Calculate margins and dimensions...
  dither->left      = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft];
  right             = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight];
  dither->top       = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxTop];
  dither->bottom    = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom];
  dither->out_width = (right - dither->left + 7) / 8;

  if (right <= in_header->cupsWidth)
    dither->in_width = right - dither->left;
  else
    dither->in_width = in_header->cupsWidth - dither->left;

  if (dither->in_width > 65536 || dither->out_width > 65536)
    return (false);			// Protect against large allocations

  // Calculate input/output color values
  dither->in_bpp = in_header->cupsBitsPerPixel;

  switch (in_header->cupsColorSpace)
  {
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_SW :
    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_SRGB :
    case CUPS_CSPACE_ADOBERGB :
        dither->in_white = 255;
        break;

    default :
        dither->in_white = 0;
        break;
  }

  switch (options->header.cupsColorSpace)
  {
    case CUPS_CSPACE_W :
    case CUPS_CSPACE_SW :
    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_SRGB :
    case CUPS_CSPACE_ADOBERGB :
        dither->out_white = 255;
        break;

    default :
        dither->out_white = 0;
        break;
  }

  // Allocate memory...
  if ((dither->input[0] = calloc(4 * dither->in_width, sizeof(lprint_pixel_t))) == NULL)
    return (false);

  for (i = 1; i < 4; i ++)
    dither->input[i] = dither->input[0] + i * dither->in_width;

  if ((dither->output = malloc(dither->out_width)) == NULL)
    return (false);

  // Return indicating success...
  return (true);
}


//
// 'lprintDitherFree()' - Free memory for a dither buffer.
//

void
lprintDitherFree(
    lprint_dither_t *dither)		// I - Dither buffer
{
  free(dither->input[0]);
  free(dither->output);

  memset(dither, 0, sizeof(lprint_dither_t));
}


//
// 'lprintDitherLine()' - Copy and dither a line.
//
// This function copies the current line and dithers it as needed.  `true` is
// returned if the output line needs to be sent to the printer - the `output`
// member points to the output bitmap and `outwidth` specifies the bitmap width
// in bytes.
//
// Dithering is always 1 line behind the current line, so you need to call this
// function one last time in the endpage callback with `y` == `cupsHeight` to
// get the last line.
//

bool					// O - `true` if line dithered, `false` to skip
lprintDitherLine(
    lprint_dither_t *dither,		// I - Dither buffer
    unsigned        y,			// I - Input line number (starting at `0`)
    unsigned char   *line)		// I - Input line
{
  return (false);
}
