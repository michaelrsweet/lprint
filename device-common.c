//
// Common device support code for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
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
// Local functions...
//

#ifdef HAVE_LIBUSB
static int	lprint_find_usb(lprint_device_cb_t cb, const void *user_data, lprint_device_t *device);
static int	lprint_open_cb(const char *device_uri, const void *user_data);
#endif // HAVE_LIBUSB


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
// 'lprintListDevices()' - List available devices.
//

void
lprintListDevices(
    lprint_device_cb_t cb,		// I - Callback function
    const void         *user_data)	// I - User data for callback
{
#ifdef HAVE_LIBUSB
  lprint_device_t	junk;		// Dummy device data


  lprint_find_usb(cb, user_data, &junk);
#endif /* HAVE_LIBUSB */
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
  else if (device->handle)
  {
    int	count;				// Bytes that were read

    if (libusb_bulk_transfer(device->handle, device->read_endp, buffer, bytes, &count, 0) < 0)
      return (-1);
    else
      return (count);
  }

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
  else if (device->handle)
  {
    int	count;				// Bytes that were written

    if (libusb_bulk_transfer(device->handle, device->write_endp, (unsigned char *)buffer, bytes, &count, 0) < 0)
      return (-1);
    else
      return (count);
  }

  return (-1);
}


#ifdef HAVE_LIBUSB
//
// 'lprint_find_usb()' - Find a USB printer.
//

static int				// O - 1 if found, 0 if not
lprint_find_usb(
    lprint_device_cb_t cb,		// I - Callback function
    const void         *user_data,	// I - User data pointer
    lprint_device_t    *device)		// O - Device info
{
  ssize_t	err = 0,		// Current error
		i,			// Looping var
		num_udevs;		// Number of USB devices
  libusb_device	**udevs;		// USB devices


 /*
  * Get the list of connected USB devices...
  */

  device->device = NULL;
  device->handle = NULL;

  if ((err = libusb_init(NULL)) != 0)
  {
    fprintf(stderr, "lprint: Unable to initialize USB access: %s\n", libusb_strerror(err));
    return (0);
  }

  num_udevs = libusb_get_device_list(NULL, &udevs);

 /*
  * Find the printers and do the callback until we find a match.
  */

  for (i = 0; i < num_udevs; i ++)
  {
    libusb_device *udevice = udevs[i];	// Current device
    char	device_id[1024],	// Current device ID
		device_uri[1024];	// Current device URI
    struct libusb_device_descriptor devdesc;
					// Current device descriptor
    struct libusb_config_descriptor *confptr = NULL;
					// Pointer to current configuration
    const struct libusb_interface *ifaceptr = NULL;
					// Pointer to current interface
    const struct libusb_interface_descriptor *altptr = NULL;
					// Pointer to current alternate setting
    const struct libusb_endpoint_descriptor *endpptr = NULL;
					// Pointer to current endpoint
    uint8_t	conf,			// Current configuration
		iface,			// Current interface
		altset,			// Current alternate setting
		endp,			// Current endpoint
		read_endp,		// Current read endpoint
		write_endp;		// Current write endpoint

    // Ignore devices with no configuration data and anything that is not
    // a printer...
    if (libusb_get_device_descriptor(udevice, &devdesc) < 0)
      continue;

    if (!devdesc.bNumConfigurations || !devdesc.idVendor || !devdesc.idProduct)
      continue;

    device->device     = udevice;
    device->handle     = NULL;
    device->conf       = -1;
    device->origconf   = -1;
    device->iface      = -1;
    device->ifacenum   = -1;
    device->altset     = -1;
    device->write_endp = -1;
    device->read_endp  = -1;
    device->protocol   = 0;

    for (conf = 0; conf < devdesc.bNumConfigurations; conf ++)
    {
      if (libusb_get_config_descriptor(udevice, conf, &confptr) < 0)
	continue;

      // Some printers offer multiple interfaces...
      for (iface = confptr->bNumInterfaces, ifaceptr = confptr->interface; iface > 0; iface --, ifaceptr ++)
      {
        if (!ifaceptr->altsetting)
          continue;

	for (altset = ifaceptr->num_altsetting, altptr = ifaceptr->altsetting; altset > 0; altset --, altptr ++)
	{
	  if (altptr->bInterfaceClass != LIBUSB_CLASS_PRINTER || altptr->bInterfaceSubClass != 1)
	    continue;

	  if (altptr->bInterfaceProtocol != 1 && altptr->bInterfaceProtocol != 2)
	    continue;

	  if (altptr->bInterfaceProtocol < device->protocol)
	    continue;

	  read_endp  = 0xff;
	  write_endp = 0xff;

	  for (endp = 0, endpptr = altptr->endpoint; endp < altptr->bNumEndpoints; endp ++, endpptr ++)
	  {
	    if ((endpptr->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK)
	    {
	      if (endpptr->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
		read_endp = endp;
	      else
		write_endp = endp;
	    }
	  }

	  if (write_endp != 0xff)
	  {
	    // Save the best match so far...
	    device->protocol   = altptr->bInterfaceProtocol;
	    device->altset     = altptr->bAlternateSetting;
	    device->ifacenum   = altptr->bInterfaceNumber;
	    device->write_endp = write_endp;
	    if (device->protocol > 1)
	      device->read_endp = read_endp;
	  }
	}

	if (device->protocol > 0)
	{
	  device->conf  = conf;
	  device->iface = ifaceptr - confptr->interface;

	  if (!libusb_open(udevice, &device->handle))
	  {
	    uint8_t	current;	// Current configuration

	    // Opened the device, try to set the configuration...
	    if (libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE, 8, /* GET_CONFIGURATION */ 0, 0, (unsigned char *)&current, 1, 5000) < 0)
	      current = 0;

            if (confptr->bConfigurationValue != current)
            {
              // Select the configuration we want...
              if (libusb_set_configuration(device->handle, confptr->bConfigurationValue) < 0)
              {
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle)
            {
              // Claim the interface...
              if (libusb_claim_interface(device->handle, device->ifacenum) < 0)
              {
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle && ifaceptr->num_altsetting > 1)
            {
              // Set the alternate setting as needed...
              if (libusb_set_interface_alt_setting(device->handle, device->ifacenum, device->altset) < 0)
              {
                libusb_close(device->handle);
                device->handle = NULL;
              }
            }

            if (device->handle)
            {
              // Get the 1284 Device ID...
              if (libusb_control_transfer(device->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE, 0, device->conf, (device->iface << 8) | device->altset, (unsigned char *)device_id, sizeof(device_id), 5000) < 0)
              {
                device_id[0] = '\0';
                libusb_close(device->handle);
                device->handle = NULL;
              }
              else
              {
                int length = ((device_id[0] & 255) << 8) | (device_id[1] & 255);
                if (length < 14 || length > sizeof(device_id))
                  length = ((device_id[1] & 255) << 8) | (device_id[0] & 255);

                if (length > sizeof(device_id))
                  length = sizeof(device_id);

                length -= 2;
                memmove(device_id, device_id + 2, (size_t)length);
                device_id[length] = '\0';
              }
            }

            if (device->handle)
            {
              // Build the device URI...
              char	*make,		// Pointer to make
			*model,		// Pointer to model
			*serial = NULL,	// Pointer to serial number
			*ptr,		// Pointer into device ID
			temp[256];	// Temporary string for serial #

              if ((make = strstr(device_id, "MANUFACTURER:")) != NULL)
                make += 13;
              else if ((make = strstr(device_id, "MFG:")) != NULL)
                make += 4;
              else
                make = "Unknown";

              if ((model = strstr(device_id, "MODEL:")) != NULL)
                model += 6;
              else if ((model = strstr(device_id, "MDL:")) != NULL)
                model += 4;
              else
                model = "Unknown";

              if ((serial = strstr(device_id, "SERIALNUMBER:")) != NULL)
                serial += 12;
              else if ((serial = strstr(device_id, "SERN:")) != NULL)
                serial += 5;
              else if ((serial = strstr(device_id, "SN:")) != NULL)
                serial += 3;
              else
              {
                int length = libusb_get_string_descriptor_ascii(device->handle, devdesc.iSerialNumber, (unsigned char *)temp, sizeof(temp) - 1);
                if (length > 0)
                {
                  temp[length] = '\0';
                  serial       = temp;
                }
              }

              if (serial)
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s?serial=%s", model, serial);
              else
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s", model);

              if ((*cb)(device_uri, user_data))
              {
		if (device->read_endp != -1)
		  device->read_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->read_endp].bEndpointAddress;

		if (device->write_endp != -1)
		  device->write_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->write_endp].bEndpointAddress;

                break;
              }

	      libusb_close(device->handle);
	      device->handle = NULL;
            }
	  }
	}

	libusb_free_config_descriptor(confptr);
      }
    }
  }

  // Clean up ....
  if (num_udevs >= 0)
    libusb_free_device_list(udevs, 1);

  return (device->handle != NULL);
}


//
// 'lprint_open_cb()' - Look for a matching device URI.
//

static int				// O - 1 on match, 0 otherwise
lprint_open_cb(const char *device_uri,	// I - This device's URI
	       const void *user_data)	// I - URI we are looking for
{
  return (!strcmp(device_uri, (const char *)user_data));
}
#endif // HAVE_LIBUSB
