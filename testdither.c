//
// Dither test program
//
// Copyright © 2023 by Michael R Sweet
//

#include "lprint.h"
#include <fcntl.h>
#include <unistd.h>


//
// Local functions...
//

static void	write_line(lprint_dither_t *dither, unsigned y, cups_raster_t *out_ras, cups_page_header_t *out_header, unsigned char *out_line);


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			ret = 0;	// Exit status
  pappl_pr_options_t	options;	// Print job options
  unsigned		page,		// Current page
			y;		// Current line on page
  int			in_file;	// Input file
  cups_raster_t		*in_ras;	// Input raster stream
  cups_page_header_t	in_header;	// Input page header
  unsigned char		*in_line;	// Input line
  cups_raster_t		*out_ras;	// Output raster stream
  cups_page_header_t	out_header;	// Output page header
  unsigned char		*out_line;	// Output line
  lprint_dither_t	dither;		// Dithering data


  // Check command-line
  if (argc != 2)
  {
    fputs("Usage: ./dithertest FILENAME.pwg >OUTPUT.pwg\n", stderr);
    return (1);
  }

  // Open input raster file...
  if ((in_file = open(argv[1], O_RDONLY)) < 0)
  {
    perror(argv[1]);
    return (1);
  }

  if ((in_ras = cupsRasterOpen(in_file, CUPS_RASTER_READ)) == NULL)
  {
    fprintf(stderr, "%s: %s\n", argv[1], cupsLastErrorString());
    close(in_file);
    return (1);
  }

  // Output output raster stream...
  if ((out_ras = cupsRasterOpen(1, CUPS_RASTER_WRITE_PWG)) == NULL)
  {
    fprintf(stderr, "stdout: %s\n", cupsLastErrorString());
    close(in_file);
    cupsRasterClose(in_ras);
    return (1);
  }

  // Loop until we run out of pages...
  memset(&options, 0, sizeof(options));

  for (page = 1; cupsRasterReadHeader(in_ras, &in_header); page ++)
  {
    // Show page info...
    fprintf(stderr, "Page %u: %ux%ux%u\n", page, in_header.cupsWidth, in_header.cupsHeight, in_header.cupsBitsPerPixel);

    // Build the output header and job options...
    memcpy(&options.header, &in_header, sizeof(options.header));

    memcpy(&out_header, &in_header, sizeof(out_header));
    out_header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
    out_header.cupsColorSpace   = CUPS_CSPACE_SRGB;
    out_header.cupsBitsPerColor = 8;
    out_header.cupsBitsPerPixel = 24;
    out_header.cupsBytesPerLine = out_header.cupsWidth * 3;
    out_header.cupsNumColors    = 3;

    // Allocate memory
    if (!lprintDitherAlloc(&dither, NULL, &options, CUPS_CSPACE_K, 1.0))
    {
      fputs("Unable to initialize dither buffer.\n", stderr);
      ret = 1;
      break;
    }

    in_line  = malloc(in_header.cupsBytesPerLine);
    out_line = malloc(out_header.cupsBytesPerLine);

    if (!in_line || !out_line)
    {
      perror("Unable to allocate memory for page");
      ret = 1;
      break;
    }

    // Dither page...
    cupsRasterWriteHeader(out_ras, &out_header);

    for (y = 0; y < in_header.cupsHeight; y ++)
    {
      cupsRasterReadPixels(in_ras, in_line, in_header.cupsBytesPerLine);

      if (lprintDitherLine(&dither, y, in_line))
        write_line(&dither, y, out_ras, &out_header, out_line);
    }

    if (lprintDitherLine(&dither, y, NULL))
      write_line(&dither, y, out_ras, &out_header, out_line);

    // Free memory
    lprintDitherFree(&dither);
    free(in_line);
    free(out_line);
  }

  // Cleanup and exit...
  close(in_file);
  cupsRasterClose(in_ras);
  cupsRasterClose(out_ras);

  return (ret);
}


//
// 'write_line()' - Write a color-coded line showing how dithering is applied.
//


static void
write_line(
    lprint_dither_t    *dither,		// Dither buffer
    unsigned           y,		// Current line
    cups_raster_t      *out_ras,	// Output raster stream
    cups_page_header_t *out_header,	// Output page header
    unsigned char      *out_line)	// Output line buffer
{
  unsigned		count;		// Number of pixels left
  unsigned char		*out_ptr,	// Pointer into output line
			*dptr,		// Pointer into dither output
			dbit;		// Bit in dither output
  lprint_pixel_t	*in_ptr;	// Pointer into input line


  // Provide a color-coded version of the dithered output, with blue showing
  // repeated areas...
  for (count = dither->in_width, out_ptr = out_line, dptr = dither->output, dbit = 128, in_ptr = dither->input[(y - 1) & 3]; count > 0; count --, in_ptr ++)
  {
    // Set the current output pixel color...
    if (*dptr & dbit)
    {
      // Black or dark blue
      if (in_ptr->count)
      {
        // Dark blue
	*out_ptr++ = 0;
	*out_ptr++ = 0;
        *out_ptr++ = 63 - in_ptr->value / 4;
      }
      else if (in_ptr->value < 255)
      {
        // Dark yellow for dithered gray to black
	*out_ptr++ = 63 - in_ptr->value / 8;
	*out_ptr++ = 63 - in_ptr->value / 8;
	*out_ptr++ = 0;
      }
      else
      {
        // Black
	*out_ptr++ = 0;
	*out_ptr++ = 0;
        *out_ptr++ = 0;
      }
    }
    else if (in_ptr->count)
    {
      // Light blue
      *out_ptr++ = 223;
      *out_ptr++ = 223;
      *out_ptr++ = 255 - in_ptr->value / 8;
    }
    else if (in_ptr->value)
    {
      // Yellow for gray that didn't come out white
      *out_ptr++ = 255 - in_ptr->value / 4;
      *out_ptr++ = 255 - in_ptr->value / 4;
      *out_ptr++ = 127;
    }
    else
    {
      // White
      *out_ptr++ = 255;
      *out_ptr++ = 255;
      *out_ptr++ = 255;
    }

    // Advance to the next bit in the dithered output...
    if (dbit == 1)
    {
      dbit = 128;
      dptr ++;
    }
    else
    {
      dbit /= 2;
    }
  }

  // Write the output line and return...
  cupsRasterWritePixels(out_ras, out_line, out_header->cupsBytesPerLine);
}
