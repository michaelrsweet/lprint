//
// NIIMBOT unit tests
//
// Usage:
//
//   ./testniimbot
//
// Copyright © 2026.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "lprint.h"
#include "test.h"


//
// Functions exercised here are the packet builders in lprint-niimbot.c.
//

extern bool		lprint_niimbot_frame(unsigned char *dst, size_t dstlen, size_t *used, unsigned char cmd, const unsigned char *data, unsigned char len, int connect_prefix);
extern int		lprint_niimbot_density_for(const char *driver_name, int configured, int adjust);
extern int		lprint_niimbot_label_code(pappl_media_tracking_t tracking);
extern unsigned char	lprint_niimbot_fill_start(unsigned char *payload, int d110mv4, unsigned copies);
extern unsigned char	lprint_niimbot_fill_pagesize(unsigned char *payload, int d110mv4, unsigned rows, int head_px, unsigned copies);
extern size_t		lprint_niimbot_bitmap_packet(unsigned char *dst, size_t dstlen, int head_px, size_t row_bytes, unsigned y, const unsigned char *row);


//
// Local functions...
//

static int	expect_bytes(const char *title, const unsigned char *got, size_t gotlen, const unsigned char *expect, size_t expectlen);
static int	frame_payload(unsigned char *dst, size_t dstlen, unsigned char cmd, const unsigned char *data, unsigned char len);


//
// 'main()' - Main entry for the NIIMBOT tests.
//

int					// O - Exit status
main(void)
{
  unsigned char	pkt[160],		// Built packet
		payload[13];		// Setup payload
  unsigned char	row[104];		// One packed row
  size_t	pktlen;			// Built length
  unsigned char	one = 0x01;		// Connect payload
  static const unsigned char connect_bytes[9] =
  { 0x03, 0x55, 0x55, 0xc1, 0x01, 0x01, 0xc1, 0xaa, 0xaa };
  static const unsigned char b4_start[14] =
  { 0x55, 0x55, 0x01, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xaa, 0xaa };
  static const unsigned char b4_pagesize[13] =
  { 0x55, 0x55, 0x13, 0x06, 0x00, 0x40, 0x03, 0x40, 0x00, 0x01, 0x17, 0xaa, 0xaa };
  const char	*name;			// Matched driver


  // Connect packet captured from the B4.
  testBegin("connect packet");
  pktlen = 0;
  if (!lprint_niimbot_frame(pkt, sizeof(pkt), &pktlen, 0xc1, &one, 1, 1) ||
      expect_bytes("connect packet", pkt, pktlen, connect_bytes, sizeof(connect_bytes)))
    testEnd(false);
  else
    testEnd(true);

  testBegin("short buffer");
  testEnd(!lprint_niimbot_frame(pkt, 4, &pktlen, 0xc1, &one, 1, 0));

  // Device-id matching, including the leading space in the B4 model field.
  testBegin("auto-add B4");
  name = lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL: B4;");
  testEnd(name && !strcmp(name, "niimbot_b4"));

  testBegin("auto-add B1");
  name = lprintNiimbotAutoAdd("MANUFACTURER:NIIMBOT;MODEL:B1;");
  testEnd(name && !strcmp(name, "niimbot_b1"));

  testBegin("auto-add B3S_P");
  name = lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL:B3S_P;");
  testEnd(name && !strcmp(name, "niimbot_b3s_p"));

  testBegin("auto-add D11_H");
  name = lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL:D11-H;");
  testEnd(name && !strcmp(name, "niimbot_d11_h"));

  testBegin("auto-add rejects neighbors");
  testEnd(lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL:B18;") == NULL &&
          lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL:B11;") == NULL &&
          lprintNiimbotAutoAdd("MFG:NIIMBOT;MDL:B1 Pro;") == NULL &&
          lprintNiimbotAutoAdd("MFG:Zebra;MDL:B4;") == NULL &&
          lprintNiimbotAutoAdd("") == NULL &&
          lprintNiimbotAutoAdd(NULL) == NULL);

  // 0-100 darkness onto the confirmed 1-5 range. 50 is density 3.
  testBegin("density");
  testEnd(lprint_niimbot_density_for("niimbot_b4", 0, 0) == 1 &&
          lprint_niimbot_density_for("niimbot_b4", 50, 0) == 3 &&
          lprint_niimbot_density_for("niimbot_b4", 100, 0) == 5 &&
          lprint_niimbot_density_for("niimbot_b1", 50, 50) == 5 &&
          lprint_niimbot_density_for("niimbot_d11_h", 50, -100) == 1 &&
          lprint_niimbot_density_for("niimbot_unknown", 50, 0) == -1);

  testBegin("label type");
  testEnd(lprint_niimbot_label_code(PAPPL_MEDIA_TRACKING_GAP) == 1 &&
          lprint_niimbot_label_code(PAPPL_MEDIA_TRACKING_MARK) == 2 &&
          lprint_niimbot_label_code(PAPPL_MEDIA_TRACKING_CONTINUOUS) == 3 &&
          lprint_niimbot_label_code(PAPPL_MEDIA_TRACKING_WEB) == 1);

  // B4 print-start and page-size bytes captured on the printer.
  testBegin("B4 print start");
  pktlen = frame_payload(pkt, sizeof(pkt), 0x01, payload, lprint_niimbot_fill_start(payload, 0, 1));
  testEnd(!expect_bytes("B4 print start", pkt, pktlen, b4_start, sizeof(b4_start)));

  testBegin("B4 page size");
  pktlen = frame_payload(pkt, sizeof(pkt), 0x13, payload, lprint_niimbot_fill_pagesize(payload, 0, 64, 832, 1));
  testEnd(!expect_bytes("B4 page size", pkt, pktlen, b4_pagesize, sizeof(b4_pagesize)));

  // D11_H uses the longer forms. Head width is 142 dots.
  testBegin("D11_H print start");
  {
    static const unsigned char d11_start[16] =
    { 0x55, 0x55, 0x01, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xaa, 0xaa };

    pktlen = frame_payload(pkt, sizeof(pkt), 0x01, payload, lprint_niimbot_fill_start(payload, 1, 1));
    testEnd(!expect_bytes("D11_H print start", pkt, pktlen, d11_start, sizeof(d11_start)));
  }

  testBegin("D11_H page size");
  {
    static const unsigned char d11_page[20] =
    {
      0x55, 0x55, 0x13, 0x0d, 0x00, 0x0a, 0x00, 0x8e, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9b, 0xaa, 0xaa
    };

    pktlen = frame_payload(pkt, sizeof(pkt), 0x13, payload, lprint_niimbot_fill_pagesize(payload, 1, 10, 142, 1));
    testEnd(!expect_bytes("D11_H page size", pkt, pktlen, d11_page, sizeof(d11_page)));
  }

  // B4's 104-byte row does not divide into three even thirds, so the count is the total.
  testBegin("B4 row count");
  memset(row, 0, sizeof(row));
  row[0] = 0x80;
  pktlen = lprint_niimbot_bitmap_packet(pkt, sizeof(pkt), 832, 104, 0, row);
  testEnd(pktlen == 117 && pkt[4] == 0x00 && pkt[5] == 0x00 && pkt[6] == 0x00 && pkt[7] == 0x01 && pkt[8] == 0x00 && pkt[9] == 0x01 && pkt[10] == 0x80);

  // B1's 48-byte row divides evenly, so each third is counted separately.
  testBegin("B1 row count");
  memset(row, 0xff, 48);
  pktlen = lprint_niimbot_bitmap_packet(pkt, sizeof(pkt), 384, 48, 3, row);
  testEnd(pktlen == 61 && pkt[4] == 0x00 && pkt[5] == 0x03 && pkt[6] == 0x80 && pkt[7] == 0x80 && pkt[8] == 0x80 && pkt[9] == 0x01);

  return (testsPassed ? 0 : 1);
}


//
// 'expect_bytes()' - Compare a buffer to an expected packet.
//

static int				// O - 0 on match, 1 otherwise
expect_bytes(
    const char          *title,		// I - Test name
    const unsigned char *got,		// I - Actual bytes
    size_t              gotlen,		// I - Actual length
    const unsigned char *expect,	// I - Expected bytes
    size_t              expectlen)	// I - Expected length
{
  if (gotlen == expectlen && !memcmp(got, expect, expectlen))
    return (0);

  testError("\n%s: got %u bytes, expected %u\n", title, (unsigned)gotlen, (unsigned)expectlen);
  testHexDump(got, gotlen);
  return (1);
}


//
// 'frame_payload()' - Frame a payload the same way the driver sends it.
//

static int				// O - Packet length, or 0 on error
frame_payload(
    unsigned char       *dst,		// O - Output buffer
    size_t              dstlen,		// I - Output buffer size
    unsigned char       cmd,		// I - Opcode
    const unsigned char *data,		// I - Payload
    unsigned char       len)		// I - Payload length
{
  size_t used = 0;			// Framed length

  if (!lprint_niimbot_frame(dst, dstlen, &used, cmd, data, len, 0))
    return (0);

  return ((int)used);
}
