#ifndef __TIFF_PACKBITS_H
#define __TIFF_PACKBITS_H

static inline unsigned tiff_packbits_bufsize(unsigned len)
{
  return len + (len + 127) / 128;
}

unsigned tiff_packbits(unsigned char *out, const unsigned char *in, unsigned len);

#endif /* __TIFF_PACKBITS_H */
