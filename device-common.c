//
// ??? for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"
#include <stdarg.h>


//
// 'lprintCloseDevice()' - Close a device connection.
//

void
lprintCloseDevice(
    lprint_device_t *device)		// I - Device to close
{
  if (device)
  {
    if (device->fd >= 0)
      close(device->fd);
#ifdef HAVE_LIBUSB
    else if (device->handle)
      libusb_close(device->handle);
#endif /* HAVE_LIBUSB */

    free(device);
  }
}


//
// 'lprintOpenDevice()' - Open a connection to a device.
//
// Currently only "file:///dev/filename", "socket://address:port", and
// "usb://make/model?serial=value" URIs are supported.
//

lprint_device_t	*			// O - Device connection or `NULL`
lprintOpenDevice(
    const char *device_uri)		// I - Device URI
{
  lprint_device_t	*device;	// Device structure
  char			scheme[32],	// URI scheme
			userpass[32],	// Username/password (not used)
			host[256],	// Host name or make
			resource[256],	// Resource path, if any
			*options;	// Pointer to options, if any
  int			port;		// Port number


  if (!device_uri)
    return (NULL);

  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    return (NULL);

  if (strcmp(scheme, "file") && strcmp(scheme, "socket") && strcmp(scheme, "usb"))
    return (NULL);

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  if ((device = calloc(1, sizeof(lprint_device_t))) != NULL)
  {
    if (!strcmp(scheme, "file"))
    {
      // Character device file...
      if ((device->fd = open(resource, O_RDWR | O_EXCL)) < 0)
        goto error;
    }
    else if (!strcmp(scheme, "socket"))
    {
      // Raw socket (JetDirect or similar)
      char		port_str[32];	// String for port number
      http_addrlist_t	*list;		// Address list

      snprintf(port_str, sizeof(port_str), "%d", port);
      if ((list = httpAddrGetList(host, AF_UNSPEC, port_str)) == NULL)
        goto error;

      httpAddrConnect2(list, &device->fd, 30000, NULL);
      httpAddrFreeList(list);

      if (device->fd < 0)
        goto error;
    }
    else if (!strcmp(scheme, "usb"))
    {
      // USB printer class device
      goto error;
    }
  }

  return (device);

  error:

  free(device);
  return (NULL);

}


//
// 'lprintPrintfDevice()' - Write a formatted string.
//

ssize_t					// O - Number of characters or -1 on error
lprintPrintfDevice(
    lprint_device_t *device,		// I - Device
    const char      *format,		// I - Printf-style format string
    ...)				// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional args
  char		buffer[8192];		// Output buffer


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  return (lprintWriteDevice(device, buffer, strlen(buffer)));
}


//
// 'lprintReadDevice()' - Read from a device.
//

ssize_t					// O - Number of bytes read or -1 on error
lprintReadDevice(
    lprint_device_t *device,		// I - Device
    void            *buffer,		// I - Read buffer
    size_t          bytes)		// I - Max bytes to read
{
  if (!device)
    return (-1);
  else if (device->fd >= 0)
  {
    ssize_t	count;			// Bytes read this time

    while ((count = read(device->fd, buffer, bytes)) < 0)
      if (errno != EINTR && errno != EAGAIN)
        break;

    return (count);
  }

  // TODO: Implement USB
  return (-1);
}


//
// 'lprintWriteDevice()' - Write to a device.
//

ssize_t					// O - Number of bytes written or -1 on error
lprintWriteDevice(
    lprint_device_t *device,		// I - Device
    const void      *buffer,		// I - Write buffer
    size_t          bytes)		// I - Number of bytes to write
{
  if (!device)
    return (-1);
  else if (device->fd >= 0)
  {
    const char	*ptr = (const char *)buffer;
					// Pointer into buffer
    ssize_t	total = 0,		// Total bytes written
		count;			// Bytes written this time

    while (total < bytes)
    {
      if ((count = write(device->fd, ptr, bytes - total)) < 0)
      {
        if (errno == EINTR || errno == EAGAIN)
          continue;

        return (-1);
      }

      total += (size_t)count;
      ptr   += count;
    }

    return ((ssize_t)total);
  }

  // TODO: Implement USB writes
  return (-1);
}
