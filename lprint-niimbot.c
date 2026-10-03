//
// NIIMBOT driver for LPrint, a Label Printer Application
//
// Copyright © 2026.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Speaks the framed raster protocol used by the NIIMBOT models that have been
// printed end to end: B1, B3S_P, and B4 (203 dpi, top feed) and D11_H (300 dpi,
// side feed). The B4 path was checked on hardware over both USB CDC serial and
// the USB printer-class interface. A page is 1-bit, MSB first, set bit burns.
//

#include "lprint.h"
#include <ctype.h>
#include <time.h>
#include <unistd.h>


//
// Model facts for the confirmed printers...
//

typedef enum lprint_niimbot_seq_e
{
  LPRINT_NIIMBOT_SEQ_B1,		// 7-byte start, 6-byte page size
  LPRINT_NIIMBOT_SEQ_D110MV4		// 9-byte start, 13-byte page size
} lprint_niimbot_seq_t;

typedef struct lprint_niimbot_model_s
{
  const char		*name;		// Driver name
  int			dpi;		// Head resolution
  int			head_px;	// Printhead width in dots
  int			density_min,	// Darkness range reported by the model
			density_max;
  lprint_niimbot_seq_t seq;		// Which command sizes to send
  const char * const *media;	// Supported size names
  int			num_media;	// Number of size names
  const char		*ready_size;	// Default loaded size
} lprint_niimbot_model_t;

typedef struct lprint_niimbot_s	// Per-job state
{
  lprint_dither_t	dither;		// Dithering state
  const lprint_niimbot_model_t *model;
					// Model selected by the driver name
  unsigned char	*rows;			// Packed 1-bit rows, head width
  size_t		nrows,		// Rows stored
			capacity,	// Allocated rows
			row_bytes;	// Bytes per row
  unsigned char	rx[2048];		// Bytes read since the last command
  size_t		rx_len;		// Bytes currently in rx
  unsigned char	last_cmd;		// Opcode of the last parsed frame
  unsigned char	last_len;		// Payload length of that frame
  unsigned char	last_data[32];		// Payload of that frame
} lprint_niimbot_t;


//
// Packet opcodes from the B4 capture...
//

#define LPRINT_NIIMBOT_CONNECT		0xC1
#define LPRINT_NIIMBOT_PRINT_START	0x01
#define LPRINT_NIIMBOT_PAGE_START	0x03
#define LPRINT_NIIMBOT_SET_PAGE_SIZE	0x13
#define LPRINT_NIIMBOT_SET_DENSITY	0x21
#define LPRINT_NIIMBOT_SET_LABEL_TYPE	0x23
#define LPRINT_NIIMBOT_EMPTY_ROW	0x84
#define LPRINT_NIIMBOT_BITMAP_ROW	0x85
#define LPRINT_NIIMBOT_PRINT_STATUS	0xA3
#define LPRINT_NIIMBOT_HEARTBEAT	0xDC
#define LPRINT_NIIMBOT_PAGE_END		0xE3
#define LPRINT_NIIMBOT_PRINT_END	0xF3

#define LPRINT_NIIMBOT_RSP_PRINT_START	0x02
#define LPRINT_NIIMBOT_RSP_PAGE_START	0x04
#define LPRINT_NIIMBOT_RSP_PAGE_SIZE	0x14
#define LPRINT_NIIMBOT_RSP_DENSITY	0x31
#define LPRINT_NIIMBOT_RSP_LABEL_TYPE	0x33
#define LPRINT_NIIMBOT_RSP_STATUS	0xB3
#define LPRINT_NIIMBOT_RSP_CONNECT	0xC2
#define LPRINT_NIIMBOT_RSP_ERROR	0xDB
#define LPRINT_NIIMBOT_RSP_PAGE_END	0xE4
#define LPRINT_NIIMBOT_RSP_PRINT_END	0xF4

#define LPRINT_NIIMBOT_LABEL_GAP	1
#define LPRINT_NIIMBOT_LABEL_MARK	2
#define LPRINT_NIIMBOT_LABEL_CONTINUOUS	3


//
// Media lists. Widths stay inside each head: 48, 72, 104, and 12 mm...
//

static const char * const	lprint_niimbot_b1_media[] =
{
  "oe_address-label_1.125x3.5in",
  "oe_sm-multipurpose-label_1x2.125in",

  "roll_max_1.89x3600in",
  "roll_min_0.5x0.5in"
};

static const char * const	lprint_niimbot_b3s_media[] =
{
  "oe_shipping-label_2.3125x4in",
  "oe_multipurpose-label_2x2.3125in",

  "roll_max_2.83x3600in",
  "roll_min_0.5x0.5in"
};

static const char * const	lprint_niimbot_b4_media[] =
{
  "na_index-4x6_4x6in",
  "oe_shipping-label_2.3125x4in",

  "roll_max_4.09x3600in",
  "roll_min_0.5x0.5in"
};

static const char * const	lprint_niimbot_d11h_media[] =
{
  "oe_cable-label_0.47x1.57in",

  "roll_max_0.47x3600in",
  "roll_min_0.25x0.25in"
};

static const lprint_niimbot_model_t lprint_niimbot_models[] =
{
  { "niimbot_b1",    203, 384, 1, 5, LPRINT_NIIMBOT_SEQ_B1,
    lprint_niimbot_b1_media, (int)(sizeof(lprint_niimbot_b1_media) / sizeof(lprint_niimbot_b1_media[0])),
    "oe_address-label_1.125x3.5in" },
  { "niimbot_b3s_p", 203, 576, 1, 5, LPRINT_NIIMBOT_SEQ_B1,
    lprint_niimbot_b3s_media, (int)(sizeof(lprint_niimbot_b3s_media) / sizeof(lprint_niimbot_b3s_media[0])),
    "oe_shipping-label_2.3125x4in" },
  { "niimbot_b4",    203, 832, 1, 5, LPRINT_NIIMBOT_SEQ_B1,
    lprint_niimbot_b4_media, (int)(sizeof(lprint_niimbot_b4_media) / sizeof(lprint_niimbot_b4_media[0])),
    "na_index-4x6_4x6in" },
  { "niimbot_d11_h", 300, 142, 1, 5, LPRINT_NIIMBOT_SEQ_D110MV4,
    lprint_niimbot_d11h_media, (int)(sizeof(lprint_niimbot_d11h_media) / sizeof(lprint_niimbot_d11h_media[0])),
    "oe_cable-label_0.47x1.57in" }
};


//
// Local functions...
//

static bool		lprint_niimbot_command(pappl_job_t *job, lprint_niimbot_t *nj, pappl_device_t *device, const char *what, unsigned char cmd, const unsigned char *data, unsigned char len, int connect_prefix, unsigned char expect);
static unsigned char	lprint_niimbot_checksum(unsigned char cmd, const unsigned char *data, unsigned char len);
static int		lprint_niimbot_density(const lprint_niimbot_model_t *model, pappl_pr_options_t *options);
bool			lprint_niimbot_frame(unsigned char *dst, size_t dstlen, size_t *used, unsigned char cmd, const unsigned char *data, unsigned char len, int connect_prefix);
static int		lprint_niimbot_label_type(pappl_pr_options_t *options);
static const lprint_niimbot_model_t *lprint_niimbot_model(const char *driver_name);
static void		lprint_niimbot_normalize(char *dst, size_t dstlen, const char *src);
static bool		lprint_niimbot_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool		lprint_niimbot_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool		lprint_niimbot_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool		lprint_niimbot_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool		lprint_niimbot_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);
static bool		lprint_niimbot_send_page(pappl_job_t *job, lprint_niimbot_t *nj, pappl_device_t *device, pappl_pr_options_t *options);
static bool		lprint_niimbot_send_rows(pappl_job_t *job, lprint_niimbot_t *nj, pappl_device_t *device);
static bool		lprint_niimbot_status(pappl_printer_t *printer);
static bool		lprint_niimbot_store_row(pappl_job_t *job, lprint_niimbot_t *nj);
static int		lprint_niimbot_take_frame(lprint_niimbot_t *nj);
static bool		lprint_niimbot_wait_finished(pappl_job_t *job, lprint_niimbot_t *nj, pappl_device_t *device, int copies);
static bool		lprint_niimbot_write(pappl_job_t *job, pappl_device_t *device, const unsigned char *buf, size_t len);
static void		lprint_niimbot_put_u16(unsigned char *dst, unsigned value);


//
// 'lprintNiimbot()' - Initialize the NIIMBOT driver.
//

bool					// O - `true` on success, `false` on error
lprintNiimbot(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  const lprint_niimbot_model_t *model = lprint_niimbot_model(driver_name);
					// Selected model


  (void)system;
  (void)device_uri;
  (void)device_id;
  (void)attrs;
  (void)cbdata;

  if (!model)
    return (false);

  data->rendjob_cb   = lprint_niimbot_rendjob;
  data->rendpage_cb  = lprint_niimbot_rendpage;
  data->rstartjob_cb = lprint_niimbot_rstartjob;
  data->rstartpage_cb = lprint_niimbot_rstartpage;
  data->rwriteline_cb = lprint_niimbot_rwriteline;
  data->status_cb    = lprint_niimbot_status;

  data->num_resolution  = 1;
  data->x_resolution[0] = model->dpi;
  data->y_resolution[0] = model->dpi;
  data->x_default       = model->dpi;
  data->y_default       = model->dpi;

  data->left_right = 1;
  data->bottom_top = 1;

  data->num_media = model->num_media;
  memcpy(data->media, model->media, (size_t)model->num_media * sizeof(data->media[0]));

  data->num_source = 1;
  data->source[0]  = "main-roll";

  cupsCopyString(data->media_ready[0].size_name, model->ready_size, sizeof(data->media_ready[0].size_name));
  cupsCopyString(data->media_ready[0].type, "labels", sizeof(data->media_ready[0].type));
  data->media_ready[0].tracking = PAPPL_MEDIA_TRACKING_GAP;

  data->num_type = 2;
  data->type[0]  = "labels";
  data->type[1]  = "continuous";

  data->tracking_supported = PAPPL_MEDIA_TRACKING_GAP | PAPPL_MEDIA_TRACKING_MARK | PAPPL_MEDIA_TRACKING_CONTINUOUS;

  data->mode_configured = PAPPL_LABEL_MODE_TEAR_OFF;
  data->mode_supported  = PAPPL_LABEL_MODE_TEAR_OFF;

  data->darkness_configured = 50;
  data->darkness_supported  = 50;

  return (true);
}


//
// 'lprintNiimbotAutoAdd()' - Pick a confirmed model from a NIIMBOT device ID.
//

const char *				// O - Driver name or `NULL`
lprintNiimbotAutoAdd(const char *device_id)
					// I - IEEE-1284 device ID
{
  int		num_did;		// Number of device-id pairs
  cups_option_t	*did;			// Device-id pairs
  const char	*make,			// Manufacturer
		*model;			// Model
  char		norm[128];		// Model with spaces and punctuation removed


  if (!device_id || !device_id[0])
    return (NULL);

  num_did = papplDeviceParseID(device_id, &did);
  if ((make = cupsGetOption("MANUFACTURER", (cups_len_t)num_did, did)) == NULL)
    if ((make = cupsGetOption("MANU", (cups_len_t)num_did, did)) == NULL)
      make = cupsGetOption("MFG", (cups_len_t)num_did, did);

  if (!make || strncasecmp(make, "NIIMBOT", 7))
  {
    cupsFreeOptions(num_did, did);
    return (NULL);
  }

  if ((model = cupsGetOption("MODEL", (cups_len_t)num_did, did)) == NULL)
    model = cupsGetOption("MDL", (cups_len_t)num_did, did);

  lprint_niimbot_normalize(norm, sizeof(norm), model ? model : "");
  cupsFreeOptions(num_did, did);

  if (!strcmp(norm, "B4"))
    return ("niimbot_b4");
  if (!strcmp(norm, "B1"))
    return ("niimbot_b1");
  if (!strcmp(norm, "B3S") || !strcmp(norm, "B3SP"))
    return ("niimbot_b3s_p");
  if (!strcmp(norm, "D11H"))
    return ("niimbot_d11_h");

  return (NULL);
}


//
// 'lprint_niimbot_checksum()' - XOR of opcode, length, and payload.
//

static unsigned char			// O - Checksum byte
lprint_niimbot_checksum(
    unsigned char       cmd,		// I - Opcode
    const unsigned char *data,		// I - Payload
    unsigned char       len)		// I - Payload length
{
  unsigned char	sum = (unsigned char)(cmd ^ len);
					// Running checksum
  unsigned char	i;			// Looping var


  for (i = 0; i < len; i ++)
    sum ^= data[i];

  return (sum);
}


//
// 'lprint_niimbot_command()' - Send one packet and wait for a reply opcode.
//

static bool				// O - `true` when `expect` arrives
lprint_niimbot_command(
    pappl_job_t         *job,		// I - Job
    lprint_niimbot_t    *nj,		// I - Driver state
    pappl_device_t      *device,	// I - Output device
    const char          *what,		// I - Command name for logs
    unsigned char       cmd,		// I - Opcode
    const unsigned char *data,		// I - Payload
    unsigned char       len,		// I - Payload length
    int                 connect_prefix,	// I - Prefix the connect quirk byte?
    unsigned char       expect)		// I - Response opcode
{
  unsigned char	pkt[160];		// Framed packet
  size_t	pktlen;			// Packet bytes
  int		reads;			// Reads attempted while waiting


  if (!lprint_niimbot_frame(pkt, sizeof(pkt), &pktlen, cmd, data, len, connect_prefix))
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT %s packet does not fit.", what);
    return (false);
  }

  if (!lprint_niimbot_write(job, device, pkt, pktlen))
    return (false);

  for (reads = 0; reads < 8; reads ++)
  {
    unsigned char tmp[512];		// Fresh bytes
    ssize_t	n;			// Bytes read

    while (lprint_niimbot_take_frame(nj))
    {
      if (nj->last_cmd == expect)
        return (true);
      if (nj->last_cmd == LPRINT_NIIMBOT_RSP_ERROR)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT reported a print error during %s.", what);
        return (false);
      }
    }

    n = papplDeviceRead(device, tmp, sizeof(tmp));
    if (n < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT %s: no reply.", what);
      return (false);
    }
    if (n == 0)
      continue;
    if (nj->rx_len + (size_t)n > sizeof(nj->rx))
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT reply overflow during %s.", what);
      return (false);
    }
    memcpy(nj->rx + nj->rx_len, tmp, (size_t)n);
    nj->rx_len += (size_t)n;

    while (lprint_niimbot_take_frame(nj))
    {
      if (nj->last_cmd == expect)
        return (true);
      if (nj->last_cmd == LPRINT_NIIMBOT_RSP_ERROR)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT reported a print error during %s.", what);
        return (false);
      }
    }
  }

  papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT %s: reply was not opcode %02X.", what, expect);
  return (false);
}


//
// 'lprint_niimbot_density()' - Map the 0-100 darkness control onto the model range.
//

static int				// O - Device density
lprint_niimbot_density(
    const lprint_niimbot_model_t *model,// I - Model
    pappl_pr_options_t           *options)
					// I - Job options
{
  int		darkness,		// Combined 0-100 value
		span;			// Model range


  darkness = options->darkness_configured + options->print_darkness;
  if (darkness < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  span = model->density_max - model->density_min;
  return (model->density_min + (span * darkness + 50) / 100);
}


//
// 'lprint_niimbot_frame()' - Build one on-wire packet.
//

bool					// O - `true` on success
lprint_niimbot_frame(
    unsigned char       *dst,		// O - Output buffer
    size_t              dstlen,		// I - Output buffer size
    size_t              *used,		// O - Bytes written
    unsigned char       cmd,		// I - Opcode
    const unsigned char *data,		// I - Payload
    unsigned char       len,		// I - Payload length
    int                 connect_prefix)// I - Prefix 0x03 for connect
{
  size_t	need = (size_t)(7 + len + (connect_prefix ? 1 : 0));
					// Full packet size
  size_t	pos = 0;		// Write index


  if (need > dstlen)
    return (false);

  if (connect_prefix)
    dst[pos ++] = 0x03;
  dst[pos ++] = 0x55;
  dst[pos ++] = 0x55;
  dst[pos ++] = cmd;
  dst[pos ++] = len;
  if (len)
  {
    memcpy(dst + pos, data, len);
    pos += len;
  }
  dst[pos ++] = lprint_niimbot_checksum(cmd, data, len);
  dst[pos ++] = 0xAA;
  dst[pos ++] = 0xAA;
  *used = pos;

  return (true);
}


//
// 'lprint_niimbot_label_code()' - Map media tracking to the device paper code.
//

int					// O - Label type byte
lprint_niimbot_label_code(
    pappl_media_tracking_t tracking)	// I - Media tracking
{
  switch (tracking)
  {
    case PAPPL_MEDIA_TRACKING_MARK :
        return (LPRINT_NIIMBOT_LABEL_MARK);
    case PAPPL_MEDIA_TRACKING_CONTINUOUS :
        return (LPRINT_NIIMBOT_LABEL_CONTINUOUS);
    default :
        return (LPRINT_NIIMBOT_LABEL_GAP);
  }
}


//
// 'lprint_niimbot_label_type()' - Map media tracking to the device paper code.
//

static int				// O - Label type byte
lprint_niimbot_label_type(
    pappl_pr_options_t *options)	// I - Job options
{
  return (lprint_niimbot_label_code(options->media.tracking));
}


//
// 'lprint_niimbot_density_for()' - Darkness for a named confirmed model.
//

int					// O - Device density, or -1 if unknown
lprint_niimbot_density_for(
    const char *driver_name,		// I - Driver name
    int        configured,		// I - Configured darkness, 0-100
    int        adjust)			// I - Per-job darkness adjustment
{
  const lprint_niimbot_model_t *model = lprint_niimbot_model(driver_name);
					// Selected model
  pappl_pr_options_t options;		// Options passed to the mapper


  if (!model)
    return (-1);

  memset(&options, 0, sizeof(options));
  options.darkness_configured = configured;
  options.print_darkness      = adjust;

  return (lprint_niimbot_density(model, &options));
}


//
// 'lprint_niimbot_fill_start()' - Print-start payload for one sequence.
//

unsigned char				// O - Payload length
lprint_niimbot_fill_start(
    unsigned char *payload,		// O - At least 9 bytes
    int           d110mv4,		// I - Non-zero for the D11_H sequence
    unsigned      copies)		// I - Copies
{
  memset(payload, 0, 9);
  lprint_niimbot_put_u16(payload, copies);

  return (d110mv4 ? 9 : 7);
}


//
// 'lprint_niimbot_fill_pagesize()' - Page-size payload for one sequence.
//

unsigned char				// O - Payload length
lprint_niimbot_fill_pagesize(
    unsigned char *payload,		// O - At least 13 bytes
    int           d110mv4,		// I - Non-zero for the D11_H sequence
    unsigned      rows,			// I - Page rows
    int           head_px,		// I - Printhead dots
    unsigned      copies)		// I - Copies
{
  memset(payload, 0, 13);
  lprint_niimbot_put_u16(payload + 0, rows);
  lprint_niimbot_put_u16(payload + 2, (unsigned)head_px);
  lprint_niimbot_put_u16(payload + 4, copies);

  return (d110mv4 ? 13 : 6);
}


//
// 'lprint_niimbot_bitmap_packet()' - Frame one bitmap row.
//

size_t					// O - Packet bytes, or 0 on error
lprint_niimbot_bitmap_packet(
    unsigned char       *dst,		// O - Output buffer
    size_t              dstlen,		// I - Output buffer size
    int                 head_px,	// I - Printhead dots
    size_t              row_bytes,	// I - Packed row length
    unsigned            y,		// I - Row index
    const unsigned char *row)		// I - Packed row, MSB first
{
  unsigned char	payload[6 + 128];	// Position, counts, repeat, pixels
  int		chunk = (head_px / 8) / 3;	// Bytes in one head third
  int		split = chunk > 0 && (int)row_bytes <= chunk * 3;
					// Per-third counts fit this head
  int		total = 0,		// Black dots in the row
		parts[3] = { 0, 0, 0 };	// Black dots per third
  size_t	i,			// Looping var
		used = 0;		// Framed length


  if (row_bytes > 128)
    return (0);

  for (i = 0; i < row_bytes; i ++)
  {
    unsigned char value = row[i];	// Current byte
    int		bits = 0;		// Set bits in this byte

    while (value)
    {
      bits  += value & 1;
      value >>= 1;
    }
    total += bits;
    if (split)
    {
      int idx = (int)i / chunk;		// Which third this byte belongs to

      if (idx > 2)
        idx = 2;
      parts[idx] += bits;
    }
  }

  lprint_niimbot_put_u16(payload, y);
  if (split)
  {
    payload[2] = (unsigned char)parts[0];
    payload[3] = (unsigned char)parts[1];
    payload[4] = (unsigned char)parts[2];
  }
  else
  {
    payload[2] = 0;
    payload[3] = (unsigned char)(total & 0xff);
    payload[4] = (unsigned char)((total >> 8) & 0xff);
  }
  payload[5] = 1;
  memcpy(payload + 6, row, row_bytes);

  if (!lprint_niimbot_frame(dst, dstlen, &used, LPRINT_NIIMBOT_BITMAP_ROW, payload, (unsigned char)(6 + row_bytes), 0))
    return (0);

  return (used);
}


//
// 'lprint_niimbot_model()' - Look up a confirmed model by driver name.
//

static const lprint_niimbot_model_t *	// O - Model or `NULL`
lprint_niimbot_model(const char *driver_name)
					// I - Driver name
{
  size_t	i;			// Looping var


  for (i = 0; i < (sizeof(lprint_niimbot_models) / sizeof(lprint_niimbot_models[0])); i ++)
  {
    if (!strcmp(driver_name, lprint_niimbot_models[i].name))
      return (lprint_niimbot_models + i);
  }

  return (NULL);
}


//
// 'lprint_niimbot_normalize()' - Uppercase a model and drop separators.
//

static void
lprint_niimbot_normalize(
    char       *dst,			// O - Output buffer
    size_t     dstlen,			// I - Output buffer size
    const char *src)			// I - Model string
{
  char	*end = dst + dstlen - 1;	// Last writable byte


  while (*src && dst < end)
  {
    if (*src != ' ' && *src != '_' && *src != '-')
      *dst++ = (char)toupper((unsigned char)*src);
    src ++;
  }
  *dst = '\0';
}


//
// 'lprint_niimbot_put_u16()' - Store a big-endian 16-bit value.
//

static void
lprint_niimbot_put_u16(unsigned char *dst,// O - Two bytes
                       unsigned      value)
					// I - Value
{
  dst[0] = (unsigned char)((value >> 8) & 0xff);
  dst[1] = (unsigned char)(value & 0xff);
}


//
// 'lprint_niimbot_rendjob()' - Free per-job state.
//

static bool				// O - `true`
lprint_niimbot_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_niimbot_t *nj = (lprint_niimbot_t *)papplJobGetData(job);
					// Driver state


  (void)options;
  (void)device;

  if (nj)
  {
    free(nj->rows);
    free(nj);
    papplJobSetData(job, NULL);
  }

  return (true);
}


//
// 'lprint_niimbot_rendpage()' - Send the buffered page.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_niimbot_t *nj = (lprint_niimbot_t *)papplJobGetData(job);
					// Driver state
  bool		ok;			// Send result


  (void)page;

  lprint_niimbot_rwriteline(job, options, device, options->header.cupsHeight, NULL);
  ok = lprint_niimbot_send_page(job, nj, device, options);

  free(nj->rows);
  nj->rows     = NULL;
  nj->nrows    = 0;
  nj->capacity = 0;
  lprintDitherFree(&nj->dither);

  return (ok);
}


//
// 'lprint_niimbot_rstartjob()' - Allocate per-job state.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  lprint_niimbot_t *nj = (lprint_niimbot_t *)calloc(1, sizeof(lprint_niimbot_t));
					// Driver state


  (void)options;
  (void)device;

  if (!nj)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate NIIMBOT job state.");
    return (false);
  }

  nj->model = lprint_niimbot_model(papplPrinterGetDriverName(papplJobGetPrinter(job)));
  if (!nj->model)
  {
    free(nj);
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unknown NIIMBOT driver.");
    return (false);
  }

  papplJobSetData(job, nj);
  return (true);
}


//
// 'lprint_niimbot_rstartpage()' - Prepare dithering for one page.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  lprint_niimbot_t *nj = (lprint_niimbot_t *)papplJobGetData(job);
					// Driver state
  unsigned	image_px = options->header.cupsWidth;
					// Dots that will be clipped to the head


  (void)device;
  (void)page;

  nj->rx_len = 0;

  if (options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight] > 0 &&
      options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight] >= options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft])
    image_px = options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxRight] - options->header.cupsInteger[CUPS_RASTER_PWG_ImageBoxLeft] + 1;

  if (image_px > (unsigned)nj->model->head_px)
    papplLogJob(job, PAPPL_LOGLEVEL_WARN, "Page is wider than the %d-dot NIIMBOT head; clipping.", nj->model->head_px);

  if (!lprintDitherAlloc(&nj->dither, job, options, (unsigned)nj->model->head_px, CUPS_CSPACE_K, 1.0, /*out_mirror*/false))
    return (false);

  nj->row_bytes = nj->dither.out_width;
  return (true);
}


//
// 'lprint_niimbot_rwriteline()' - Keep one dithered row.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_niimbot_t *nj = (lprint_niimbot_t *)papplJobGetData(job);
					// Driver state


  (void)options;
  (void)device;

  if (!lprintDitherLine(&nj->dither, y, line))
    return (true);

  return (lprint_niimbot_store_row(job, nj));
}


//
// 'lprint_niimbot_send_page()' - Run one confirmed print sequence.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_send_page(
    pappl_job_t         *job,		// I - Job
    lprint_niimbot_t    *nj,		// I - Driver state
    pappl_device_t      *device,	// I - Output device
    pappl_pr_options_t  *options)	// I - Job options
{
  unsigned char	one = 0x01;		// Single-byte payload
  unsigned char	payload[13];		// Longest setup payload
  int		copies,			// Copies requested
		density,		// Device density
		label_type;		// Device paper code
  bool		opened = false;		// PrintStart was accepted


  if (nj->nrows == 0)
    return (true);
  if (nj->nrows > 65535)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT page is longer than 65535 rows.");
    return (false);
  }

  copies = options->header.NumCopies > 0 ? (int)options->header.NumCopies : 1;
  if (copies > 65535)
    copies = 65535;
  density    = lprint_niimbot_density(nj->model, options);
  label_type = lprint_niimbot_label_type(options);

  papplLogJob(job, PAPPL_LOGLEVEL_INFO, "NIIMBOT %s: %u×%u dots, density %d, %d %s.",
              nj->model->name, (unsigned)nj->model->head_px, (unsigned)nj->nrows, density, copies,
              label_type == LPRINT_NIIMBOT_LABEL_MARK ? "mark" :
              label_type == LPRINT_NIIMBOT_LABEL_CONTINUOUS ? "continuous" : "gap");

  if (!lprint_niimbot_command(job, nj, device, "connect", LPRINT_NIIMBOT_CONNECT, &one, 1, 1, LPRINT_NIIMBOT_RSP_CONNECT))
    return (false);

  payload[0] = (unsigned char)density;
  if (!lprint_niimbot_command(job, nj, device, "density", LPRINT_NIIMBOT_SET_DENSITY, payload, 1, 0, LPRINT_NIIMBOT_RSP_DENSITY))
    return (false);

  payload[0] = (unsigned char)label_type;
  if (!lprint_niimbot_command(job, nj, device, "label type", LPRINT_NIIMBOT_SET_LABEL_TYPE, payload, 1, 0, LPRINT_NIIMBOT_RSP_LABEL_TYPE))
    return (false);

  {
    int d110 = nj->model->seq == LPRINT_NIIMBOT_SEQ_D110MV4;
					// D11_H uses the longer packets
    unsigned char paylen;		// Payload length

    paylen = lprint_niimbot_fill_start(payload, d110, (unsigned)copies);
    if (!lprint_niimbot_command(job, nj, device, "print start", LPRINT_NIIMBOT_PRINT_START, payload, paylen, 0, LPRINT_NIIMBOT_RSP_PRINT_START))
      return (false);
    opened = true;

    if (d110)
    {
      unsigned char pkt[16];		// One-way status packet
      size_t	pktlen;			// Packet bytes

      // This sequence writes a status query and does not wait for it.
      if (!lprint_niimbot_frame(pkt, sizeof(pkt), &pktlen, LPRINT_NIIMBOT_PRINT_STATUS, &one, 1, 0) ||
          !lprint_niimbot_write(job, device, pkt, pktlen))
        goto failed;
    }
    else if (!lprint_niimbot_command(job, nj, device, "page start", LPRINT_NIIMBOT_PAGE_START, &one, 1, 0, LPRINT_NIIMBOT_RSP_PAGE_START))
      goto failed;

    paylen = lprint_niimbot_fill_pagesize(payload, d110, (unsigned)nj->nrows, nj->model->head_px, (unsigned)copies);
    if (!lprint_niimbot_command(job, nj, device, "page size", LPRINT_NIIMBOT_SET_PAGE_SIZE, payload, paylen, 0, LPRINT_NIIMBOT_RSP_PAGE_SIZE))
      goto failed;
  }

  if (!lprint_niimbot_send_rows(job, nj, device))
    goto failed;

  if (!lprint_niimbot_command(job, nj, device, "page end", LPRINT_NIIMBOT_PAGE_END, &one, 1, 0, LPRINT_NIIMBOT_RSP_PAGE_END))
    goto failed;

  if (!lprint_niimbot_wait_finished(job, nj, device, copies))
    goto failed;

  if (nj->model->seq == LPRINT_NIIMBOT_SEQ_D110MV4)
  {
    unsigned char pkt[16];		// One-way heartbeat
    size_t	pktlen;			// Packet bytes

    if (!lprint_niimbot_frame(pkt, sizeof(pkt), &pktlen, LPRINT_NIIMBOT_HEARTBEAT, &one, 1, 0) ||
        !lprint_niimbot_write(job, device, pkt, pktlen))
      goto failed;
  }

  if (!lprint_niimbot_command(job, nj, device, "print end", LPRINT_NIIMBOT_PRINT_END, &one, 1, 0, LPRINT_NIIMBOT_RSP_PRINT_END))
    return (false);

  return (true);

  failed:

  if (opened)
    lprint_niimbot_command(job, nj, device, "print end", LPRINT_NIIMBOT_PRINT_END, &one, 1, 0, LPRINT_NIIMBOT_RSP_PRINT_END);

  return (false);
}


//
// 'lprint_niimbot_send_rows()' - Send blank runs and bitmap rows.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_send_rows(
    pappl_job_t      *job,		// I - Job
    lprint_niimbot_t *nj,		// I - Driver state
    pappl_device_t   *device)		// I - Output device
{
  size_t	y;			// Current row


  for (y = 0; y < nj->nrows; y ++)
  {
    unsigned char pkt[160];		// Framed row
    size_t	pktlen;			// Packet bytes

    pktlen = lprint_niimbot_bitmap_packet(pkt, sizeof(pkt), nj->model->head_px, nj->row_bytes, (unsigned)y, nj->rows + y * nj->row_bytes);
    if (pktlen == 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT row is wider than the packet limit.");
      return (false);
    }
    if (!lprint_niimbot_write(job, device, pkt, pktlen))
      return (false);
  }

  return (true);
}


//
// 'lprint_niimbot_status()' - Status polling is done while a job is printing.
//

static bool				// O - `true`
lprint_niimbot_status(pappl_printer_t *printer)
					// I - Printer
{
  (void)printer;
  return (true);
}


//
// 'lprint_niimbot_store_row()' - Append the current dither output.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_store_row(
    pappl_job_t      *job,		// I - Job
    lprint_niimbot_t *nj)		// I - Driver state
{
  if (nj->nrows == nj->capacity)
  {
    size_t		ncap = nj->capacity ? nj->capacity * 2 : 64;
					// New row capacity
    unsigned char	*grown;		// Reallocated raster

    if (ncap > 200000)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT page is too long.");
      return (false);
    }
    if ((grown = realloc(nj->rows, ncap * nj->row_bytes)) == NULL)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to allocate NIIMBOT raster.");
      return (false);
    }
    nj->rows     = grown;
    nj->capacity = ncap;
  }

  memcpy(nj->rows + nj->nrows * nj->row_bytes, nj->dither.output, nj->row_bytes);
  nj->nrows ++;
  return (true);
}


//
// 'lprint_niimbot_take_frame()' - Pop one complete frame from the read buffer.
//

static int				// O - 1 if a frame was popped, 0 if not
lprint_niimbot_take_frame(lprint_niimbot_t *nj)
					// I - Driver state
{
  size_t	i = 0;			// Scan index


  while (i + 1 < nj->rx_len)
  {
    size_t	total,			// Full frame size
		dlen;			// Payload length
    unsigned char cmd,			// Opcode
		expect;			// Computed checksum

    if (nj->rx[i] != 0x55 || nj->rx[i + 1] != 0x55)
    {
      i ++;
      continue;
    }
    if (i + 4 > nj->rx_len)
      break;

    cmd   = nj->rx[i + 2];
    dlen  = nj->rx[i + 3];
    total = 7 + dlen;
    if (i + total > nj->rx_len)
      break;
    if (nj->rx[i + total - 2] != 0xAA || nj->rx[i + total - 1] != 0xAA)
    {
      i ++;
      continue;
    }

    if (i > 0)
    {
      memmove(nj->rx, nj->rx + i, nj->rx_len - i);
      nj->rx_len -= i;
    }

    expect = lprint_niimbot_checksum(cmd, nj->rx + 4, (unsigned char)dlen);
    (void)expect;
    nj->last_cmd = cmd;
    nj->last_len = (unsigned char)(dlen > sizeof(nj->last_data) ? sizeof(nj->last_data) : dlen);
    memcpy(nj->last_data, nj->rx + 4, nj->last_len);
    memmove(nj->rx, nj->rx + total, nj->rx_len - total);
    nj->rx_len -= total;
    return (1);
  }

  if (i > 0 && i < nj->rx_len)
  {
    memmove(nj->rx, nj->rx + i, nj->rx_len - i);
    nj->rx_len -= i;
  }
  else if (i >= nj->rx_len)
  {
    nj->rx_len = 0;
  }

  return (0);
}


//
// 'lprint_niimbot_wait_finished()' - Poll until the page has printed and fed.
//

static bool				// O - `true` when the page is done
lprint_niimbot_wait_finished(
    pappl_job_t      *job,		// I - Job
    lprint_niimbot_t *nj,		// I - Driver state
    pappl_device_t   *device,		// I - Output device
    int              copies)		// I - Copies requested
{
  unsigned char	one = 0x01;		// Status query payload
  struct timespec start;		// When polling began


  clock_gettime(CLOCK_MONOTONIC, &start);

  while (1)
  {
    struct timespec now;		// Current time
    long		elapsed;	// Milliseconds spent polling

    if (!lprint_niimbot_command(job, nj, device, "print status", LPRINT_NIIMBOT_PRINT_STATUS, &one, 1, 0, LPRINT_NIIMBOT_RSP_STATUS))
      return (false);

    if (nj->last_len >= 4)
    {
      unsigned page = ((unsigned)nj->last_data[0] << 8) | nj->last_data[1];
					// Completed pages
      unsigned printed = nj->last_data[2];
					// Print progress
      unsigned fed = nj->last_data[3];	// Feed progress

      if (page >= (unsigned)copies && printed >= 100 && fed >= 100)
        return (true);
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
    if (elapsed > 30000)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "NIIMBOT did not finish feeding within 30 seconds.");
      return (false);
    }

    usleep(300000);
  }
}


//
// 'lprint_niimbot_write()' - Write one buffer to the device.
//

static bool				// O - `true` on success, `false` on failure
lprint_niimbot_write(
    pappl_job_t        *job,		// I - Job
    pappl_device_t     *device,		// I - Output device
    const unsigned char *buf,		// I - Bytes
    size_t             len)		// I - Byte count
{
  if (papplDeviceWrite(device, (void *)buf, len) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to write %u bytes to the NIIMBOT.", (unsigned)len);
    return (false);
  }

  return (true);
}
