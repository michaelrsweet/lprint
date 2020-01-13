//
// Common device support code for LPrint, a Label Printer Utility
//
// Copyright © 2019-2020 by Michael R Sweet.
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
    if (device->debug_fd >= 0)
      close(device->debug_fd);

    if (device->fd >= 0)
      close(device->fd);
#ifdef HAVE_LIBUSB
    else if (device->handle)
    {
      libusb_close(device->handle);
      libusb_unref_device(device->device);
    }
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
    const char *lprint_device_debug = getenv("LPRINT_DEVICE_DEBUG");

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
#ifdef HAVE_LIBUSB
    else if (!strcmp(scheme, "usb"))
    {
      // USB printer class device
      device->fd = -1;

      if (!lprint_find_usb(lprint_open_cb, device_uri, device))
        goto error;
    }
#endif // HAVE_LIBUSB

    if (lprint_device_debug)
      device->debug_fd = open(lprint_device_debug, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    else
      device->debug_fd = -1;
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
// 'lprintPutsDevice()' - Write a literal string.
//

ssize_t					// O - Number of characters or -1 on error
lprintPutsDevice(
    lprint_device_t *device,		// I - Device
    const char      *s)			// I - Literal string
{
  return (lprintWriteDevice(device, s, strlen(s)));
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
#ifdef HAVE_LIBUSB
  else if (device->handle)
  {
    int	count;				// Bytes that were read

    if (libusb_bulk_transfer(device->handle, device->read_endp, buffer, (int)bytes, &count, 0) < 0)
      return (-1);
    else
      return (count);
  }
#endif // HAVE_LIBUSB

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

  if (device->debug_fd >= 0)
    write(device->debug_fd, buffer, bytes);

  if (device->fd >= 0)
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
#ifdef HAVE_LIBUSB
  else if (device->handle)
  {
    int	count;				// Bytes that were written

    if (libusb_bulk_transfer(device->handle, device->write_endp, (unsigned char *)buffer, (int)bytes, &count, 0) < 0)
      return (-1);
    else
      return (count);
  }
#endif // HAVE_LIBUSB

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
    fprintf(stderr, "lprint: Unable to initialize USB access: %s\n", libusb_strerror((enum libusb_error)err));
    return (0);
  }

  num_udevs = libusb_get_device_list(NULL, &udevs);

  LPRINT_DEBUG("lprint_find_usb: num_udevs=%d\n", (int)num_udevs);

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
    {
      LPRINT_DEBUG("lprint_find_usb: udev%d - no descriptor.\n", (int)i);
      continue;
    }

    LPRINT_DEBUG("lprint_find_usb: udev%d -\n", (int)i);
    LPRINT_DEBUG("lprint_find_usb:     bLength=%d\n", devdesc.bLength);
    LPRINT_DEBUG("lprint_find_usb:     bDescriptorType=%d\n", devdesc.bDescriptorType);
    LPRINT_DEBUG("lprint_find_usb:     bcdUSB=%04x\n", devdesc.bcdUSB);
    LPRINT_DEBUG("lprint_find_usb:     bDeviceClass=%d\n", devdesc.bDeviceClass);
    LPRINT_DEBUG("lprint_find_usb:     bDeviceSubClass=%d\n", devdesc.bDeviceSubClass);
    LPRINT_DEBUG("lprint_find_usb:     bDeviceProtocol=%d\n", devdesc.bDeviceProtocol);
    LPRINT_DEBUG("lprint_find_usb:     bMaxPacketSize0=%d\n", devdesc.bMaxPacketSize0);
    LPRINT_DEBUG("lprint_find_usb:     idVendor=0x%04x\n", devdesc.idVendor);
    LPRINT_DEBUG("lprint_find_usb:     idProduct=0x%04x\n", devdesc.idProduct);
    LPRINT_DEBUG("lprint_find_usb:     bcdDevice=%04x\n", devdesc.bcdDevice);
    LPRINT_DEBUG("lprint_find_usb:     iManufacturer=%d\n", devdesc.iManufacturer);
    LPRINT_DEBUG("lprint_find_usb:     iProduct=%d\n", devdesc.iProduct);
    LPRINT_DEBUG("lprint_find_usb:     iSerialNumber=%d\n", devdesc.iSerialNumber);
    LPRINT_DEBUG("lprint_find_usb:     bNumConfigurations=%d\n", devdesc.bNumConfigurations);

    if (!devdesc.bNumConfigurations || !devdesc.idVendor || !devdesc.idProduct)
      continue;

    if (devdesc.idVendor == 0x05ac)
      continue;				// Skip Apple devices...

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
      {
        LPRINT_DEBUG("lprint_find_usb:     conf%d - no descriptor\n", conf);
	continue;
      }

      LPRINT_DEBUG("lprint_find_usb:     conf%d -\n", conf);
      LPRINT_DEBUG("lprint_find_usb:         bLength=%d\n", confptr->bLength);
      LPRINT_DEBUG("lprint_find_usb:         bDescriptorType=%d\n", confptr->bDescriptorType);
      LPRINT_DEBUG("lprint_find_usb:         wTotalLength=%d\n", confptr->wTotalLength);
      LPRINT_DEBUG("lprint_find_usb:         bNumInterfaces=%d\n", confptr->bNumInterfaces);
      LPRINT_DEBUG("lprint_find_usb:         bConfigurationValue=%d\n", confptr->bConfigurationValue);
      LPRINT_DEBUG("lprint_find_usb:         iConfiguration=%d\n", confptr->iConfiguration);
      LPRINT_DEBUG("lprint_find_usb:         bmAttributes=%d\n", confptr->bmAttributes);
      LPRINT_DEBUG("lprint_find_usb:         MaxPower=%d\n", confptr->MaxPower);
      LPRINT_DEBUG("lprint_find_usb:         interface=%p\n", confptr->interface);
      LPRINT_DEBUG("lprint_find_usb:         extra=%p\n", confptr->extra);
      LPRINT_DEBUG("lprint_find_usb:         extra_length=%d\n", confptr->extra_length);

      // Some printers offer multiple interfaces...
      for (iface = 0, ifaceptr = confptr->interface; iface < confptr->bNumInterfaces; iface ++, ifaceptr ++)
      {
        if (!ifaceptr->altsetting)
        {
          LPRINT_DEBUG("lprint_find_usb:         iface%d - no alternate setting\n", iface);
          continue;
        }

	LPRINT_DEBUG("lprint_find_usb:         iface%d -\n", iface);
	LPRINT_DEBUG("lprint_find_usb:             num_altsetting=%d\n", ifaceptr->num_altsetting);
	LPRINT_DEBUG("lprint_find_usb:             altsetting=%p\n", ifaceptr->altsetting);

	for (altset = 0, altptr = ifaceptr->altsetting; altset < ifaceptr->num_altsetting; altset ++, altptr ++)
	{
	  LPRINT_DEBUG("lprint_find_usb:             altset%d - bInterfaceClass=%d, bInterfaceSubClass=%d, bInterfaceProtocol=%d\n", altset, altptr->bInterfaceClass, altptr->bInterfaceSubClass, altptr->bInterfaceProtocol);

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
	  device->iface = iface;

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

                LPRINT_DEBUG("lprint_find_usb:     device_id=\"%s\"\n", device_id);
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

              if ((model = strstr(device_id, "MODEL:")) != NULL)
                model += 6;
              else if ((model = strstr(device_id, "MDL:")) != NULL)
                model += 4;

              if ((serial = strstr(device_id, "SERIALNUMBER:")) != NULL)
                serial += 12;
              else if ((serial = strstr(device_id, "SERN:")) != NULL)
                serial += 5;
              else if ((serial = strstr(device_id, "SN:")) != NULL)
                serial += 3;

              if (serial)
              {
                if ((ptr = strchr(serial, ';')) != NULL)
                  *ptr = '\0';
              }
              else
              {
                int length = libusb_get_string_descriptor_ascii(device->handle, devdesc.iSerialNumber, (unsigned char *)temp, sizeof(temp) - 1);
                if (length > 0)
                {
                  temp[length] = '\0';
                  serial       = temp;
                }
              }

              if (make)
              {
                if ((ptr = strchr(make, ';')) != NULL)
                  *ptr = '\0';
              }
              else
                make = "Unknown";

              if (model)
              {
                if ((ptr = strchr(model, ';')) != NULL)
                  *ptr = '\0';
              }
              else
                model = "Unknown";

              if (serial)
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s?serial=%s", model, serial);
              else
                httpAssembleURIf(HTTP_URI_CODING_ALL, device_uri, sizeof(device_uri), "usb", NULL, make, 0, "/%s", model);

              if ((*cb)(device_uri, user_data))
              {
                LPRINT_DEBUG("lprint_find_usb:     Found a match.\n");

		libusb_ref_device(device->device);

		if (device->read_endp != -1)
		  device->read_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->read_endp].bEndpointAddress;

		if (device->write_endp != -1)
		  device->write_endp = confptr->interface[device->iface].altsetting[device->altset].endpoint[device->write_endp].bEndpointAddress;

                goto match_found;
              }

	      libusb_close(device->handle);
	      device->handle = NULL;
            }
	  }
	}
      } // iface loop

      libusb_free_config_descriptor(confptr);
    } // conf loop
  }

  match_found:

  LPRINT_DEBUG("lprint_find_usb: device->handle=%p\n", device->handle);

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
  int match = !strcmp(device_uri, (const char *)user_data);
					// Does this match?

  LPRINT_DEBUG("lprint_open_cb(device_uri=\"%s\", user_data=\"%s\") returning %d.\n", device_uri, (char *)user_data, match);

  return (match);
}
#endif // HAVE_LIBUSB
