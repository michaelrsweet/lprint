//
// Dither test program
//
// Usage:
//
//   ./testdither [--plain] INPUT.pwg > OUTPUT.pwg
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
  bool			plain = false;	// Plain/original output
  pappl_pr_options_t	options;	// Print job options
  unsigned		page,		// Current page
			y;		// Current line on page
  const char		*in_name;	// Input filename
  int			in_file;	// Input file
  cups_raster_t		*in_ras;	// Input raster stream
  cups_page_header_t	in_header;	// Input page header
  unsigned char		*in_line;	// Input line
  cups_raster_t		*out_ras;	// Output raster stream
  cups_page_header_t	out_header;	// Output page header
  unsigned char		*out_line;	// Output line
  lprint_dither_t	dither;		// Dithering data
  static const pappl_dither_t clustered =
  {					// Clustered-Dot Dither Matrix
    {  96,  40,  48, 104, 140, 188, 196, 148,  97,  41,  49, 105, 141, 189, 197, 149 },
    {  32,   0,   8,  56, 180, 236, 244, 204,  33,   1,   9,  57, 181, 237, 245, 205 },
    {  88,  24,  16,  64, 172, 228, 252, 212,  89,  25,  17,  65, 173, 229, 253, 213 },
    { 120,  80,  72, 112, 132, 164, 220, 156, 121,  81,  73, 113, 133, 165, 221, 157 },
    { 136, 184, 192, 144, 100,  44,  52, 108, 137, 185, 193, 145, 101,  45,  53, 109 },
    { 176, 232, 240, 200,  36,   4,  12,  60, 177, 233, 241, 201,  37,   5,  13,  61 },
    { 168, 224, 248, 208,  92,  28,  20,  68, 169, 225, 249, 209,  93,  29,  21,  69 },
    { 128, 160, 216, 152, 124,  84,  76, 116, 129, 161, 217, 153, 125,  85,  77, 117 },
    {  98,  42,  50, 106, 142, 190, 198, 150,  99,  43,  51, 107, 143, 191, 199, 151 },
    {  34,   2,  10,  58, 182, 238, 246, 206,  35,   3,  11,  59, 183, 239, 247, 207 },
    {  90,  26,  18,  66, 174, 230, 254, 214,  91,  27,  19,  67, 175, 231, 254, 215 },
    { 122,  82,  74, 114, 134, 166, 222, 158, 123,  83,  75, 115, 135, 167, 223, 159 },
    { 138, 186, 194, 146, 102,  46,  54, 110, 139, 187, 195, 147, 103,  47,  55, 111 },
    { 178, 234, 242, 202,  38,   6,  14,  62, 179, 235, 243, 203,  39,   7,  15,  63 },
    { 170, 226, 250, 210,  94,  30,  22,  70, 171, 227, 251, 211,  95,  31,  23,  71 },
    { 130, 162, 218, 154, 126,  86,  78, 118, 131, 163, 219, 155, 127,  87,  79, 119 }
  };


  // Check command-line
  if (argc == 2 && argv[1][0] != '-')
  {
    in_name = argv[1];
  }
  else if (argc == 3 && !strcmp(argv[1], "--plain"))
  {
    plain   = true;
    in_name = argv[2];
  }
  else
  {
    fputs("Usage: ./dithertest [--plain] INPUT.pwg >OUTPUT.pwg\n", stderr);
    return (1);
  }

  // Open input raster file...
  if ((in_file = open(in_name, O_RDONLY)) < 0)
  {
    perror(in_name);
    return (1);
  }

  if ((in_ras = cupsRasterOpen(in_file, CUPS_RASTER_READ)) == NULL)
  {
    fprintf(stderr, "%s: %s\n", in_name, cupsLastErrorString());
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
  memcpy(&options.dither, clustered, sizeof(options.dither));

  for (page = 1; cupsRasterReadHeader(in_ras, &in_header); page ++)
  {
    // Show page info...
    fprintf(stderr, "Page %u: %ux%ux%u\n", page, in_header.cupsWidth, in_header.cupsHeight, in_header.cupsBitsPerPixel);

    // Build the output header and job options...
    memcpy(&options.header, &in_header, sizeof(options.header));

    memcpy(&out_header, &in_header, sizeof(out_header));
    if (plain)
    {
      out_header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
      out_header.cupsColorSpace   = CUPS_CSPACE_K;
      out_header.cupsBitsPerColor = 1;
      out_header.cupsBitsPerPixel = 1;
      out_header.cupsBytesPerLine = (out_header.cupsWidth + 7) / 8;
      out_header.cupsNumColors    = 1;
    }
    else
    {
      out_header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
      out_header.cupsColorSpace   = CUPS_CSPACE_SRGB;
      out_header.cupsBitsPerColor = 8;
      out_header.cupsBitsPerPixel = 24;
      out_header.cupsBytesPerLine = out_header.cupsWidth * 3;
      out_header.cupsNumColors    = 3;
    }

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
      free(in_line);
      free(out_line);
      lprintDitherFree(&dither);
      ret = 1;
      break;
    }

    // Dither page...
    cupsRasterWriteHeader(out_ras, &out_header);

    for (y = 0; y < in_header.cupsHeight; y ++)
    {
      cupsRasterReadPixels(in_ras, in_line, in_header.cupsBytesPerLine);

      if (lprintDitherLine(&dither, y, in_line))
      {
	if (plain)
	  cupsRasterWritePixels(out_ras, dither.output, out_header.cupsBytesPerLine);
	else
	  write_line(&dither, y, out_ras, &out_header, out_line);
      }
    }

    if (lprintDitherLine(&dither, y, NULL))
    {
      if (plain)
        cupsRasterWritePixels(out_ras, dither.output, out_header.cupsBytesPerLine);
      else
	write_line(&dither, y, out_ras, &out_header, out_line);
    }

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
        *out_ptr++ = 127 - in_ptr->value / 4;
      }
      else if (in_ptr->value < 255)
      {
        // Dark yellow for dithered gray to black
	*out_ptr++ = 79 - in_ptr->value / 8;
	*out_ptr++ = 79 - in_ptr->value / 8;
	*out_ptr++ = 31;
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
