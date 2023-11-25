//
// Common driver code for LPrint, a Label Printer Application
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


#define LPRINT_WHITE	56
#define LPRINT_BLACK	199


//
// 'lprintDitherAlloc()' - Allocate memory for a dither buffer.
//

bool					// O - `true` on success, `false` on error
lprintDitherAlloc(
    lprint_dither_t    *dither,		// I - Dither buffer
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Print options
    cups_cspace_t      out_cspace,	// I - Output color space
    double             out_gamma)	// I - Output gamma correction
{
  int		i, j;			// Looping vars
  unsigned	right;			// Right margin


  // Adjust dithering array and compress to a range of 16 to 239
  for (i = 0; i < 16; i ++)
  {
    for (j = 0; j < 16; j ++)
    {
      dither->dither[i][j] = (unsigned char)((LPRINT_BLACK - LPRINT_WHITE) * pow(options->dither[i][j] / 255.0, out_gamma) + LPRINT_WHITE);
    }
  }

  // Calculate margins and dimensions...
  if (!options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom] || !options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft] || !options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight] || !options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxTop])
  {
    options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft]   = options->header.HWResolution[0] * (unsigned)options->media.left_margin / 2540;
    options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxTop]    = options->header.HWResolution[1] * (unsigned)options->media.top_margin / 2540;
    options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight]  = options->header.cupsWidth - options->header.HWResolution[0] * (unsigned)options->media.right_margin / 2540 - 1;
    options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom] = options->header.cupsHeight - options->header.HWResolution[1] * (unsigned)options->media.bottom_margin / 2540 - 1;
  }

  dither->in_left   = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft];
  right             = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight];
  dither->in_top    = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxTop];
  dither->in_bottom = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxBottom];
  dither->in_width  = right - dither->in_left + 1;
  dither->in_height = dither->in_bottom - dither->in_top + 1;
  dither->out_width = (right - dither->in_left + 8) / 8;

  if (dither->in_width > 65536)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Page too wide.");
    return (false);			// Protect against large allocations
  }

  // Calculate input/output color values
  dither->in_bpp = options->header.cupsBitsPerPixel;

  switch (options->header.cupsColorSpace)
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

  switch (out_cspace)
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
  if ((dither->input[0] = calloc(4 * dither->in_width, sizeof(unsigned char))) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate input buffer.");
    return (false);
  }

  for (i = 1; i < 4; i ++)
    dither->input[i] = dither->input[0] + i * dither->in_width;

  if ((dither->output = malloc(dither->out_width)) == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate output buffer.");
    return (false);
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "dither=[");
  for (i = 0; i < 16; i ++)
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "  [ %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u ]", dither->dither[i][0], dither->dither[i][1], dither->dither[i][2], dither->dither[i][3], dither->dither[i][4], dither->dither[i][5], dither->dither[i][6], dither->dither[i][7], dither->dither[i][8], dither->dither[i][9], dither->dither[i][10], dither->dither[i][11], dither->dither[i][12], dither->dither[i][13], dither->dither[i][14], dither->dither[i][15]);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "]");
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_bottom=%u", dither->in_bottom);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_left=%u", dither->in_left);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_top=%u", dither->in_top);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_width=%u", dither->in_width);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_bpp=%u", dither->in_bpp);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "in_white=%u", dither->in_white);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "out_white=%u", dither->out_white);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "out_width=%u", dither->out_width);

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
    lprint_dither_t     *dither,	// I - Dither buffer
    unsigned            y,		// I - Input line number (starting at `0`)
    const unsigned char *line)		// I - Input line
{
  unsigned	x,			// Current column
		count;			// Remaining count
  unsigned char	*current,		// Current line
		*prev,			// Previous line
		*next;			// Next line
  unsigned char	*dline,			// Dither line
		*outptr,		// Pointer into output
		byte,			// Current byte
		bit;			// Current bit


  // Copy current input line...
  count = dither->in_width;
  next  = dither->input[y & 3];

  memset(next, 0, count);

  if (line)
  {
    switch (dither->in_bpp)
    {
      case 1 : // 1-bit black
	  for (line += dither->in_left / 8, byte = *line++, bit = 128 >> (dither->in_left & 7); count > 0; count --, next ++)
	  {
	    // Convert to 8-bit black...
	    if (byte & bit)
	      *next = 255;

	    if (bit > 1)
	    {
	      bit /= 2;
	    }
	    else
	    {
	      bit  = 128;
	      byte = *line++;
	    }
	  }
	  break;

      case 8 : // Grayscale or 8-bit black
	  if (dither->in_white)
	  {
	    // Convert grayscale to black...
	    for (line += dither->in_left; count > 0; count --, next ++, line ++)
	    {
	      if (*line < LPRINT_WHITE)
		*next = 255;
	      else if (*line > LPRINT_BLACK)
		*next = 0;
	      else
		*next = 255 - *line;
	    }
	  }
	  else
	  {
	    // Copy with clamping...
	    for (line += dither->in_left; count > 0; count --, next ++, line ++)
	    {
	      if (*line < LPRINT_WHITE)
		*next = 255;
	      else if (*line > LPRINT_BLACK)
		*next = 0;
	      else
		*next = *line;
	    }
	  }
	  break;

      default : // Something else...
	  return (false);
    }
  }

  // If we are outside the imageable area then don't dither...
  if (y < (dither->in_top + 1) || y > (dither->in_bottom + 1))
    return (false);

  // Dither...
  for (x = 0, count = dither->in_width, prev = dither->input[(y - 2) & 3], current = dither->input[(y - 1) & 3], next = dither->input[y & 3], outptr = dither->output, byte = dither->out_white, bit = 128, dline = dither->dither[y & 15]; count > 0; x ++, count --, prev ++, current ++, next ++)
  {
    if (*current)
    {
      // Not pure white/blank...
      if (*current == 255)
      {
        // 100% black...
        byte ^= bit;
      }
      else
      {
        // Only dither if this pixel does not border 100% white or black...
	if ((x > 0 && (current[-1] == 255 || current[-1] == 0)) ||
	    (count > 1 && (current[1] == 255 || current[1] == 0)) ||
	    *prev == 255 || *prev == 0 || *next == 255 || *next == 0)
        {
          // Threshold
          if (*current > 127)
	    byte ^= bit;
        }
        else if (*current > dline[x & 15])
        {
          // Dither anything else
	  byte ^= bit;
	}
      }
    }

    // Next output bit...
    if (bit > 1)
    {
      bit /= 2;
    }
    else
    {
      *outptr++ = byte;
      byte      = dither->out_white;
      bit       = 128;
    }
  }

  // Save last byte of output as needed and return...
  if (bit < 128)
    *outptr = byte;

  return (true);
}
