//
// Device communication functions for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
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
// Types...
//

typedef struct lprint_device_s		// Device connection data
{
  int			fd;		// File descriptor connection to device
  int			debug_fd;	// Debugging copy of data sent
#ifdef HAVE_LIBUSB
  struct libusb_device	*device;	// Device info
  struct libusb_device_handle *handle;	// Open handle to device
  int			conf,		// Configuration
			origconf,	// Original configuration
			iface,		// Interface
			ifacenum,	// Interface number
			altset,		// Alternate setting
			write_endp,	// Write endpoint
			read_endp,	// Read endpoint
			protocol;	// Protocol: 1 = Uni-di, 2 = Bi-di.
#endif // HAVE_LIBUSB
} lprint_device_t;

// Device callback - return 1 to stop, 0 to continue
typedef int (*lprint_device_cb_t)(const char *device_uri, const void *user_data);


//
// Functions...
//

extern void		lprintCloseDevice(lprint_device_t *device);
extern void		lprintListDevices(lprint_device_cb_t cb, const void *user_data);
extern lprint_device_t	*lprintOpenDevice(const char *device_uri);
extern ssize_t		lprintPrintfDevice(lprint_device_t *device, const char *format, ...) LPRINT_FORMAT(2, 3);
extern ssize_t		lprintPutsDevice(lprint_device_t *device, const char *s);
extern ssize_t		lprintReadDevice(lprint_device_t *device, void *buffer, size_t bytes);
extern ssize_t		lprintWriteDevice(lprint_device_t *device, const void *buffer, size_t bytes);


#endif // !_DEVICE_COMMON_H_
