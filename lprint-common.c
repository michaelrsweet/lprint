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
  unsigned	x,			// Current column
		count;			// Remaining count
  lprint_pixel_t *current,		// Current line
		*prev,			// Previous line
		*next;			// Next line
  unsigned char	*dline,			// Dither line
		*outptr,		// Pointer into output
		byte,			// Current byte
		bit;			// Current bit


  // Copy current input line...
  count = dither->in_width;
  next  = dither->input[y & 3];

  memset(next, 0, count * sizeof(lprint_pixel_t));

  switch (dither->in_bpp)
  {
    case 1 : // 1-bit black
        for (line += dither->left / 8, byte = *line++, bit = 128 >> (dither->left & 7); count > 0; count --, next ++)
        {
          // Convert to 8-bit black...
          if (byte & bit)
            next->value = 255;

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
          for (line += dither->left; count > 0; count --, next ++, line ++)
          {
            if (*line < 16)
              next->value = 255;
	    else if (*line > 239)
	      next->value = 0;
	    else
	      next->value = 255 - *line;
	  }
	}
	else
	{
          // Copy with clamping...
          for (line += dither->left; count > 0; count --, next ++, line ++)
          {
            if (*line < 16)
              next->value = 255;
	    else if (*line > 239)
	      next->value = 0;
	    else
	      next->value = *line;
	  }
	}
        break;

    default : // Something else...
        return (false);
  }

  // Then look for runs of repeated pixels
  for (count = dither->in_width, next = dither->input[y & 3]; count > 1;)
  {
    if (next->value == next[1].value)
    {
      // Repeated sequence...
      unsigned	length;			// Length of repetition

      // Calculate run length
      for (length = 2; length < count; length ++)
      {
        if (next[length - 1].value != next[length].value)
          break;
      }

      // Store run length in run...
      while (length > 0)
      {
        if (length < 255)
          next->count = length;
	else
	  next->count = 255;

        length --;
        count --;
        next ++;
      }
    }
    else
    {
      // Non-repeated sequence...
      count --;
      next ++;
    }
  }

  // If we are outside the imageable area then don't dither...
  if (y < (dither->top + 2) || y > (dither->bottom + 1))
    return (false);

  // Dither...
  for (x = 0, count = dither->in_width, prev = dither->input[(y - 2) & 3], current = dither->input[(y - 1) & 3], next = dither->input[y & 3], outptr = dither->output, byte = dither->out_white, bit = 128, dline = dither->dither[y & 15]; count > 0; x ++, count --, prev ++, current ++, next ++)
  {
    if (current->value)
    {
      // Not pure white/blank...
      if (current->value == 255)
      {
        // 100% black...
        byte ^= bit;
      }
      else
      {
        // Something potentially to dither.  If this pixel borders a 100% black
        // run, use thresholding, otherwise dither it...
        if ((!current->count && x > 0 && current[-1].value == 255 && current[-1].count) ||
            (!current->count && count > 1 && current[1].value == 255 && current[1].count) ||
            (prev->value == 255 && prev->count) ||
            (next->value == 255 && next->count))
        {
          // Threshold
          if (current->value > 127)
	    byte ^= bit;
        }
        else if (current->value > dline[x & 15])
        {
          // Dither
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
    }
  }

  // Save last byte of output as needed and return...
  if (bit < 128)
    *outptr = byte;

  return (true);
}
