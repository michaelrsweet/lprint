//
// Common definitions file for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _COMMON_H_
#  define _COMMON_H_

//
// Include necessary headers...
//

#  include "config.h"
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <errno.h>
#  include <cups/cups.h>
#  include <pthread.h>


//
// Debug macro...
//

#  ifdef DEBUG
#    define LPRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#  else
#    define LPRINT_DEBUG(...)
#  endif // DEBUG


//
// Function annotations...
//

#  if defined(__GNUC__) || defined(__has_extension)
#    define LPRINT_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define LPRINT_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define LPRINT_NORETURN	__attribute__ ((noreturn))
#  else
#    define LPRINT_FORMAT(a,b)
#    define LPRINT_NONNULL(...)
#    define LPRINT_NORETURN
#  endif // __GNUC__ || __has_extension


//
// Emulate safe string functions as needed...
//

#  ifndef __APPLE__
#    define strlcat(dst,src,dstsize) lprint_strlcat(dst,src,dstsize)
static inline size_t lprint_strlcat(char *dst, const char *src, size_t dstsize)
{
  size_t dstlen, srclen;
  dstlen = strlen(dst);
  if (dstsize < (dstlen + 1))
    return (dstlen);
  dstsize -= dstlen + 1;
  if ((srclen = strlen(src)) > dstsize)
    srclen = dstsize;
  memmove(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';
  return (dstlen = srclen);
}
#    define strlcpy(dst,src,dstsize) lprint_strlcpy(dst,src,dstsize)
static inline size_t lprint_strlcpy(char *dst, const char *src, size_t dstsize)
{
  size_t srclen = strlen(src);
  dstsize --;
  if (srclen > dstsize)
    srclen = dstsize;
  memmove(dst, src, srclen);
  dst[srclen] = '\0';
  return (srclen);
}
#  endif // !__APPLE__
#endif // !_COMMON_H_
