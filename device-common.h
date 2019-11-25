//
// Device communication functions for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _DEVICE_COMMON_H_
#  define _DEVICE_COMMON_H_

//
// Include necessary headers...
//

#  include "common.h"
#  ifdef HAVE_LIBUSB
#    include <libusb.h>
#  endif // HAVE_LIBUSB


//
// Device data...
//

typedef struct lprint_device_s		// Device connection data
{
  int				fd;	// File descriptor connection to device
#ifdef HAVE_LIBUSB
  struct libusb_device		*device;// Device info
  struct libusb_device_handle	*handle;// Open handle to device
#endif // HAVE_LIBUSB
} lprint_device_t;


//
// Functions...
//

extern void		lprintCloseDevice(lprint_device_t *device);
extern lprint_device_t	*lprintOpenDevice(const char *device_uri);
extern ssize_t		lprintPrintfDevice(lprint_device_t *device, const char *format, ...) LPRINT_FORMAT(2, 3);
extern ssize_t		lprintReadDevice(lprint_device_t *device, void *buffer, size_t bytes);
extern ssize_t		lprintWriteDevice(lprint_device_t *device, const void *buffer, size_t bytes);


#endif // !_DEVICE_COMMON_H_
