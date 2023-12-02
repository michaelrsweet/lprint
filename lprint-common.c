//
// Common driver code for LPrint, a Label Printer Application
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"


//
// Constants...
//

#define LPRINT_WHITE	56
#define LPRINT_BLACK	199
#define LPRINT_TRASH	"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" fill=\"currentColor\" class=\"bi bi-trash3-fill\" viewBox=\"0 0 16 16\"><path d=\"M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5Zm-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5ZM4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06Zm6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528ZM8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5Z\"/></svg>"


//
// Local functions...
//

static char	*localize_keyword(pappl_client_t *client, const char *attrname, const char *keyword, char *buffer, size_t bufsize);
static void	media_chooser(pappl_client_t *client, pappl_pr_driver_data_t *driver_data, pappl_media_col_t *media);


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


//
// 'lprintMediaLoad()' - Load custom label sizes for a printer.
//

bool					// O - `true` on success, `false` on error
lprintMediaLoad(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Driver data
{
  (void)printer;
  (void)data;

  return (false);
}


//
// 'lprintMediaSave()' - Save custom label sizes for a printer.
//

bool					// O - `true` on success, `false` on error
lprintMediaSave(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Driver data
{
  (void)printer;
  (void)data;

  return (false);
}


//
// 'lprintMediaUI()' - Show the printer media web page.
//

bool					// O - `true` on success, `false` on failure
lprintMediaUI(
    pappl_client_t *client,		// I - Client
    pappl_system_t *system)		// I - System
{
  int			i;		// Looping var
  pappl_printer_t	*printer;	// Printer
  pappl_pr_driver_data_t data;		// Driver data
  lprint_media_t	*media;		// Custom label sizes, if any
  const char		*status = NULL;	// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return (true);

  if ((printer = papplSystemFindPrinter(system, "/ipp/print", 1, NULL)) == NULL)
  {
    // TODO: Error
    return (true);
  }

  papplPrinterGetDriverData(printer, &data);
  media = (lprint_media_t *)data.extension;

  if (papplClientGetMethod(client) == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*value;		// Value of form variable
    bool		changed = false;// Did the media list change?
    int			num_ready = 0;	// Number of ready media

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = papplClientGetLocString(client, "Invalid form data.");
    }
    else if ((value = cupsGetOption("delete", num_form, form)) != NULL && media)
    {
      // Delete a custom size...
      for (i = 0; i < media->num_media; i ++)
      {
        if (!strcmp(media->media[i], value))
        {
          // Forget this custom size...
          changed = true;
          media->num_media --;
          if (i < media->num_media)
            memmove(media->media[i], media->media[i + 1], (media->num_media - i) * sizeof(media->media[0]));
          status = papplClientGetLocString(client, "Deleted custom label size.");
          break;
        }
      }
    }
    else if (!papplClientIsValidForm(client, num_form, form) || (value = cupsGetOption("ready-size", num_form, form)) == NULL)
    {
      status = papplClientGetLocString(client, "Invalid form submission.");
    }
    else
    {
      // Configure the current ready media...
      pwg_media_t	*pwg = NULL;	// PWG media info
      const char	*custom_width,	// Custom media width
			*custom_length,	// Custom media length
			*custom_units;	// Custom media units

      // Update ready media...
      memset(data.media_ready, 0, sizeof(data.media_ready));

      // size
      if (!strcmp(value, "custom"))
      {
	// Custom size...
	custom_width  = cupsGetOption("ready-custom-width", num_form, form);
	custom_length = cupsGetOption("ready-custom-length", num_form, form);
	custom_units  = cupsGetOption("ready-custom-units", num_form, form);

	if (custom_width && custom_length)
	{
	  if (!custom_units || !strcmp(custom_units, "in"))
	    pwg = pwgMediaForSize((int)(2540.0 * strtod(custom_width, NULL)), (int)(2540.0 * strtod(custom_length, NULL)));
	  else
	    pwg = pwgMediaForSize((int)(100.0 * strtod(custom_width, NULL)), (int)(100.0 * strtod(custom_length, NULL)));
	}

	value = pwg->pwg;
      }
      else
      {
	// Standard size...
	pwg = pwgMediaForPWG(value);
      }

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "ready-size='%s',%d,%d", pwg ? pwg->pwg : "unknown", pwg ? pwg->width : 0, pwg ? pwg->length : 0);

      if (pwg)
      {
	papplCopyString(data.media_ready[0].size_name, value, sizeof(data.media_ready[0].size_name));
	data.media_ready[0].size_width  = pwg->width;
	data.media_ready[0].size_length = pwg->length;
      }

      // source
      papplCopyString(data.media_ready[0].source, data.source[0], sizeof(data.media_ready[0].source));

      // margins
      data.media_ready[0].bottom_margin = data.media_ready[0].top_margin   = data.bottom_top;
      data.media_ready[0].left_margin   = data.media_ready[0].right_margin = data.left_right;

      // tracking
      if ((value = cupsGetOption("ready-tracking", num_form, form)) == NULL || !strcmp(value, "gap"))
        data.media_ready[0].tracking = PAPPL_MEDIA_TRACKING_GAP;
      else if (!strcmp(value, "continuous"))
        data.media_ready[0].tracking = PAPPL_MEDIA_TRACKING_CONTINUOUS;
      else
        data.media_ready[0].tracking = PAPPL_MEDIA_TRACKING_MARK;

      // type
      papplCopyString(data.media_ready[0].type, "labels", sizeof(data.media_ready[0].type));

      num_ready ++;

      // Update custom media...
      if (!strncmp(data.media_ready[0].size_name, "custom_", 7))
      {
        // Add/update the selected custom size...
	for (i = 0; i < media->num_media; i ++)
	{
	  if (!strcmp(media->media[i], data.media_ready[0].size_name))
	    break;
	}

	if (i < media->num_media)
	{
	  // Existing custom size...
	  if (i)
	  {
	    // Move current media to the top of the list...
	    changed = true;
	    memmove(media->media[1], media->media[0], i * sizeof(media->media[0]));
	    papplCopyString(media->media[0], data.media_ready[0].size_name, sizeof(media->media[0]));
	  }
	}
	else
	{
	  // Insert new custom size...
	  changed = true;
	  if (media->num_media >= LPRINT_MAX_CUSTOM)
	    media->num_media = LPRINT_MAX_CUSTOM - 1;

	  memmove(media->media[1], media->media[0], media->num_media * sizeof(media->media[0]));
	  papplCopyString(media->media[0], data.media_ready[0].size_name, sizeof(media->media[0]));
	  media->num_media ++;
	}
      }

      status = papplClientGetLocString(client, "Changes saved.");
    }

    if (changed)
    {
      // Rebuild media size list and save...
      lprintMediaUpdate(&data);
      papplPrinterSetDriverData(printer, &data, NULL);
      lprintMediaSave(printer, &data);
    }

    if (num_ready > 0)
      papplPrinterSetReadyMedia(printer, num_ready, data.media_ready);

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, "Media", 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", status);

  papplClientHTMLStartForm(client, papplClientGetURI(client), false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  media_chooser(client, &data, data.media_ready);

  papplClientHTMLPrintf(client,
			"              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
			"            </tbody>\n"
			"          </table>"
			"        </form>\n"
			"        <script>function show_hide_custom(name) {\n"
			"  let selelem = document.getElementById('custom');\n"
			"  let divelem = document.getElementById(name + '-custom');\n"
			"  if (selelem.checked)\n"
			"    divelem.style.display = 'inline-block';\n"
			"  else\n"
			"    divelem.style.display = 'none';\n"
			"}</script>\n", papplClientGetLocString(client, "Save Changes"));

  papplClientHTMLPrinterFooter(client);
  return (true);
}


//
// 'lprintMediaUpdate()' - Update the list of custom label sizes.
//

void
lprintMediaUpdate(
    pappl_pr_driver_data_t *data)	// I - Driver data
{
  int			i, j;		// Looping vars
  lprint_media_t	*media;		// Custom label sizes


  // Find the first custom size in the media list...
  for (i = 0; i < data->num_media; i ++)
  {
    if (!strncmp(data->media[i], "custom_", 7))
      break;
  }

  // Then copy any custom sizes over...
  media = (lprint_media_t *)data->extension;
  if (media)
  {
    for (j = 0; j < media->num_media && i < PAPPL_MAX_MEDIA; i ++, j ++)
      data->media[i] = media->media[j];
  }

  data->num_media = i;
}


//
// 'localize_keyword()' - Localize an attribute keyword value.
//

static char *				// O - Localized string
localize_keyword(
    pappl_client_t *client,		// I - Client
    const char     *attrname,		// I - Attribute name
    const char     *keyword,		// I - Keyword value
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - Size of string buffer
{
  const char	*loctext;		// Localized text
  char		key[256];		// Localization key


  snprintf(key, sizeof(key), "%s.%s", attrname, keyword);
  if ((loctext = papplClientGetLocString(client, key)) != key)
  {
    // Use localized string...
    papplCopyString(buffer, loctext, bufsize);
  }
  else if (!strcmp(attrname, "media"))
  {
    // Create a dimensional name for the size...
    pwg_media_t *pwg = pwgMediaForPWG(keyword);
					// PWG media size info

    if ((pwg->width % 100) == 0 && (pwg->width % 2540) != 0)
      snprintf(buffer, bufsize, "%d x %dmm Custom Label", pwg->width / 100, pwg->length / 100);
    else
      snprintf(buffer, bufsize, "%g x %gʺ Custom Label", pwg->width / 2540.0, pwg->length / 2540.0);
  }
  else
  {
    // Convert "separated-words" to "Separated Words"...
    char	*ptr;			// Pointer into string

    papplCopyString(buffer, keyword, bufsize);
    *buffer = (char)toupper(*buffer & 255);

    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = (char)toupper(*ptr & 255);
      }
    }
  }

  return (buffer);
}


//
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t         *client,	// I - Client
    pappl_pr_driver_data_t *driver_data,// I - Driver data
    pappl_media_col_t      *media)	// I - Current media values
{
  int		i,			// Looping vars
		cur_index = 0,		// Current size index
	        sel_index = 0;		// Selected size index...
  pwg_media_t	*pwg;			// PWG media size info
  char		text[256];		// Localized text
  const char	*min_size = NULL,	// Minimum size
		*max_size = NULL;	// Maximum size


  // media-size
  papplClientHTMLPrintf(client, "              <tr><th>%s</th><td>", papplClientGetLocString(client, "Current Media:"));
  for (i = 0; i < driver_data->num_media && (!min_size || !max_size); i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_m", 8) || !strncmp(driver_data->media[i], "roll_m", 6))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];
    }
  }

  for (i = 0; i < driver_data->num_media; i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_m", 8) || !strncmp(driver_data->media[i], "roll_m", 6))
      continue;

    if (!strcmp(driver_data->media[i], media->size_name))
      sel_index = cur_index;

    papplClientHTMLPrintf(client, "<div class=\"radio-list\"><input type=\"radio\" name=\"ready-size\" id=\"%s\" value=\"%s\" onChange=\"show_hide_custom('ready');\"%s><label class=\"radio-label\" for=\"%s\">%s", driver_data->media[i], driver_data->media[i], sel_index == cur_index ? " checked" : "", driver_data->media[i], localize_keyword(client, "media", driver_data->media[i], text, sizeof(text)));
    if (!strncmp(driver_data->media[i], "custom_", 7))
    {
      char	custom_delete[256];	// Delete confirmation prompt

      papplClientHTMLPrintf(client, "<button class=\"radio-action\" type=\"submit\" name=\"delete\" value=\"%s\" onClick=\"return confirm('%s');\">" LPRINT_TRASH "</button>", driver_data->media[i], papplLocFormatString(papplClientGetLoc(client), custom_delete, sizeof(custom_delete), "Delete %s?", text));
    }
    papplClientHTMLPuts(client, "</label></div>");
    cur_index ++;
  }
  if (min_size && max_size)
  {
    int cur_width, min_width, max_width;// Current/min/max width
    int cur_length, min_length, max_length;
					// Current/min/max length
    double form_width, form_length;	// Current size values for form

    if ((pwg = pwgMediaForPWG(min_size)) != NULL)
    {
      min_width  = pwg->width;
      min_length = pwg->length;
    }
    else
    {
      min_width  = 1 * 2540;
      min_length = 1 * 2540;
    }

    if ((pwg = pwgMediaForPWG(max_size)) != NULL)
    {
      max_width  = pwg->width;
      max_length = pwg->length;
    }
    else
    {
      max_width  = 9 * 2540;
      max_length = 22 * 2540;
    }

    if ((cur_width = media->size_width) < min_width)
      cur_width = min_width;
    else if (cur_width > max_width)
      cur_width = max_width;

    if ((cur_length = media->size_length) < min_length)
      cur_length = min_length;
    else if (cur_length > max_length)
      cur_length = max_length;

    // For custom sizes, min width/length is based on inches while max
    // width/length is based on millimeters, for simplicity...
    if (strstr(media->size_name, "mm"))
    {
      form_width  = cur_width / 100.0;
      form_length = cur_length / 100.0;
    }
    else
    {
      form_width  = cur_width / 2540.0;
      form_length = cur_length / 2540.0;
    }

    papplClientHTMLPrintf(client, "<div class=\"radio-list\"><input type=\"radio\" name=\"ready-size\" id=\"custom\" value=\"custom\" onChange=\"show_hide_custom('ready');\"><label class=\"radio-label\" for=\"custom\">%s</label></div>", papplClientGetLocString(client, "New Custom Size"));

    papplClientHTMLPrintf(client, "<div class=\"custom\" style=\"display: none;\" id=\"ready-custom\"><input type=\"number\" name=\"ready-custom-width\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\">&nbsp;x&nbsp;<input type=\"number\" name=\"ready-custom-length\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\"><div class=\"switch\"><input type=\"radio\" id=\"ready-custom-units-in\" name=\"ready-custom-units\" value=\"in\"%s><label for=\"ready-custom-units-in\">in</label><input type=\"radio\" id=\"ready-custom-units-mm\" name=\"ready-custom-units\" value=\"mm\"%s><label for=\"ready-custom-units-mm\">mm</label></div></div>", min_width / 2540.0, max_width / 100.0, form_width, papplClientGetLocString(client, "Width"), min_length / 2540.0, max_length / 100.0, form_length, papplClientGetLocString(client, "Height"), strstr(media->size_name, "mm") == NULL ? " checked" : "", strstr(media->size_name, "mm") != NULL ? " checked" : "");
  }

  papplClientHTMLPuts(client, "</td></tr>\n");

  // media-tracking (if needed)
  if (driver_data->tracking_supported)
  {
    pappl_media_tracking_t tracking;	// Tracking value
    static const char * const trackings[] =
    {					// Tracking strings
      "continuous",
      "gap",
      "mark",
      "web"
    };

    papplClientHTMLPrintf(client, "              <tr><th>%s</th><td><select name=\"ready-tracking\">", papplClientGetLocString(client, "Label Separator:"));
    for (i = 0, tracking = PAPPL_MEDIA_TRACKING_CONTINUOUS; i <= (int)(sizeof(trackings) / sizeof(trackings[0])); i ++, tracking *= 2)
    {
      if (!(driver_data->tracking_supported & tracking))
	continue;

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", trackings[i], tracking == media->tracking ? " selected" : "", localize_keyword(client, "media-tracking", trackings[i], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }
}
