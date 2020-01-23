//
// System object for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"
#include <ctype.h>
#include <grp.h>


//
// Local globals...
//

static int		shutdown_system = 0;


//
// Local functions...
//

static int		create_listener(const char *name, int port, int family);
static char		*get_config_file(char *buffer, size_t bufsize);
static void		get_media_col(const char *value, lprint_media_col_t *media);
static int		load_config(lprint_system_t *system);
static void		put_media_col(cups_file_t *fp, const char *name, lprint_media_col_t *media);
static int		save_config(lprint_system_t *system);
static void		sigterm_handler(int sig);


//
// 'lprintCreateSystem()' - Create a system object.
//

lprint_system_t *			// O - System object
lprintCreateSystem(
    const char        *hostname,	// I - Hostname or `NULL` for none
    int               port,		// I - Port number or `0` for auto
    const char        *subtypes,	// I - DNS-SD sub-types or `NULL` for none
    const char        *logfile,		// I - Log file or `NULL` for default
    lprint_loglevel_t loglevel,		// I - Log level
    const char        *auth_service,	// I - PAM authentication service or `NULL` for none
    const char        *admin_group)	// I - Administrative group or `NULL` for none
{
  lprint_system_t	*system;	// System object
  const char		*tmpdir;	// TMPDIR environment variable
  char			spooldir[256],	// Spool directory
			sockname[256];	// Domain socket


  // See if the spool directory can be created...
  if ((tmpdir = getenv("TMPDIR")) == NULL)
#ifdef __APPLE__
    tmpdir = "/private/tmp";
#else
    tmpdir = "/tmp";
#endif // __APPLE__

  snprintf(spooldir, sizeof(spooldir), "%s/lprint%d.d", tmpdir, (int)getuid());
  if (mkdir(spooldir, 0700) && errno != EEXIST)
  {
    perror(spooldir);
    return (NULL);
  }

  // Allocate memory...
  if ((system = (lprint_system_t *)calloc(1, sizeof(lprint_system_t))) == NULL)
    return (NULL);

  // Initialize values...
  pthread_rwlock_init(&system->rwlock, NULL);

  if (hostname)
  {
    system->hostname = strdup(hostname);
    system->port     = port ? port : 8000 + (getuid() % 1000);
  }

  system->start_time      = time(NULL);
  system->directory       = strdup(spooldir);
  system->logfd           = 2;
  system->logfile         = logfile ? strdup(logfile) : NULL;
  system->loglevel        = loglevel;
  system->next_client     = 1;
  system->next_printer_id = 1;
  system->admin_gid       = (gid_t)-1;

  if (subtypes)
    system->subtypes = strdup(subtypes);
  if (auth_service)
    system->auth_service = strdup(auth_service);
  if (admin_group)
    system->admin_group = strdup(admin_group);

  // Setup listeners...
  if ((system->listeners[0].fd = create_listener(lprintGetServerPath(sockname, sizeof(sockname)), 0, AF_LOCAL)) < 0)
  {
    lprintLog(system, LPRINT_LOGLEVEL_FATAL, "Unable to create domain socket listener for %s: %s", sockname, strerror(errno));
    goto fatal;
  }
  else
    system->listeners[0].events = POLLIN;

  system->num_listeners = 1;

  if (system->hostname)
  {
    // Create listener sockets...
    const char *lishost;		// Listen hostname

    if (strcmp(system->hostname, "localhost"))
      lishost = NULL;
    else
      lishost = "localhost";

    if (system->port == 0)
      system->port = 9000 + (getuid() % 1000);

    if ((system->listeners[system->num_listeners].fd = create_listener(lishost, system->port, AF_INET)) < 0)
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to create IPv4 listener for %s:%d: %s", lishost ? lishost : "*", system->port, strerror(errno));
    else
      system->listeners[system->num_listeners ++].events = POLLIN;

    if ((system->listeners[system->num_listeners].fd = create_listener(lishost, system->port, AF_INET6)) < 0)
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to create IPv6 listener for %s:%d: %s", lishost ? lishost : "*", system->port, strerror(errno));
    else
      system->listeners[system->num_listeners ++].events = POLLIN;

    // Error out if we cannot listen to IPv4 or IPv6 addresses...
    if (system->num_listeners == 1)
      goto fatal;
  }

  // Initialize DNS-SD as needed...
  if (system->subtypes)
    lprintInitDNSSD(system);

  // Load printers...
  if (!load_config(system))
    goto fatal;

  // Initialize logging...
  if (system->loglevel == LPRINT_LOGLEVEL_UNSPEC)
    system->loglevel = LPRINT_LOGLEVEL_ERROR;

  if (!system->logfile)
  {
    // Default log file is $TMPDIR/lprintUID.log...
    char newlogfile[256];			// Log filename

    snprintf(newlogfile, sizeof(newlogfile), "%s/lprint%d.log", tmpdir, (int)getuid());

    system->logfile = strdup(newlogfile);
  }

  if (!strcmp(system->logfile, "syslog"))
  {
    // Log to syslog...
    system->logfd = -1;
  }
  else if (!strcmp(system->logfile, "-"))
  {
    // Log to stderr...
    system->logfd = 2;
  }
  else if ((system->logfd = open(system->logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
  {
    // Fallback to stderr if we can't open the log file...
    perror(system->logfile);

    system->logfd = 2;
  }

  lprintLog(system, LPRINT_LOGLEVEL_INFO, "System configuration loaded, %d printers.", cupsArrayCount(system->printers));
  lprintLog(system, LPRINT_LOGLEVEL_INFO, "Listening for local connections at '%s'.", sockname);
  if (system->hostname)
    lprintLog(system, LPRINT_LOGLEVEL_INFO, "Listening for TCP connections at '%s' on port %d.", system->hostname, system->port);

  // Initialize authentication...
  if (system->auth_service && !strcmp(system->auth_service, "none"))
  {
    free(system->auth_service);
    system->auth_service = NULL;
  }

  if (system->admin_group && strcmp(system->admin_group, "none"))
  {
    char		buffer[8192];	// Buffer for strings
    struct group	grpbuf,		// Group buffer
			*grp = NULL;	// Admin group

    if (getgrnam_r(system->admin_group, &grpbuf, buffer, sizeof(buffer), &grp) || !grp)
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to find admin-group '%s'.", system->admin_group);
    else
      system->admin_gid = grp->gr_gid;
  }

  return (system);

  // If we get here, something went wrong...
  fatal:

  lprintDeleteSystem(system);

  return (NULL);
}


//
// 'lprintDeleteSystem()' - Delete a system object.
//

void
lprintDeleteSystem(
    lprint_system_t *system)		// I - System object
{
  int	i;				// Looping var
  char	sockname[256];			// Domain socket filename


  if (!system)
    return;

  free(system->hostname);
  free(system->directory);
  free(system->logfile);
  free(system->subtypes);

  if (system->logfd >= 0 && system->logfd != 2)
    close(system->logfd);

  for (i = 0; i < system->num_listeners; i ++)
    close(system->listeners[i].fd);

  cupsArrayDelete(system->printers);

  pthread_rwlock_destroy(&system->rwlock);

  free(system);

  unlink(lprintGetServerPath(sockname, sizeof(sockname)));
}


//
// 'lprintRunSystem()' - Run the printer service.
//

void
lprintRunSystem(lprint_system_t *system)// I - System
{
  int			i,		// Looping var
			count,		// Number of listeners that fired
			timeout;	// Timeout for poll()
  lprint_client_t	*client;	// New client


  // Catch important signals...
  lprintLog(system, LPRINT_LOGLEVEL_INFO, "Starting main loop.");

  signal(SIGTERM, sigterm_handler);
  signal(SIGINT, sigterm_handler);

  // Loop until we are shutdown or have a hard error...
  while (!shutdown_system)
  {
    if (system->save_time || system->shutdown_time)
      timeout = 5;
    else
      timeout = 10;

    if (system->clean_time && (i = (int)(time(NULL) - system->clean_time)) < timeout)
      timeout = i;

    if ((count = poll(system->listeners, (nfds_t)system->num_listeners, timeout * 1000)) < 0 && errno != EINTR && errno != EAGAIN)
    {
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to accept new connections: %s", strerror(errno));
      break;
    }

    if (count > 0)
    {
      // Accept client connections as needed...
      for (i = 0; i < system->num_listeners; i ++)
      {
	if (system->listeners[i].revents & POLLIN)
	{
	  if ((client = lprintCreateClient(system, system->listeners[i].fd)) != NULL)
	  {
	    if (pthread_create(&client->thread_id, NULL, (void *(*)(void *))lprintProcessClient, client))
	    {
	      // Unable to create client thread...
	      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to create client thread: %s", strerror(errno));
	      lprintDeleteClient(client);
	    }
	    else
	    {
	      // Detach the main thread from the client thread to prevent hangs...
	      pthread_detach(client->thread_id);
	    }
	  }
	}
      }
    }

    if (system->save_time)
    {
      // Save the configuration...
      pthread_rwlock_rdlock(&system->rwlock);
      save_config(system);
      pthread_rwlock_unlock(&system->rwlock);
      system->save_time = 0;
    }

    if (system->shutdown_time)
    {
      // Shutdown requested, see if we can do so safely...
      int		count = 0;	// Number of active jobs
      lprint_printer_t	*printer;	// Current printer

      // Force shutdown after 60 seconds
      if ((time(NULL) - system->shutdown_time) > 60)
        break;

      // Otherwise shutdown immediately if there are no more active jobs...
      pthread_rwlock_rdlock(&system->rwlock);
      for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
      {
        pthread_rwlock_rdlock(&printer->rwlock);
        count += cupsArrayCount(printer->active_jobs);
        pthread_rwlock_unlock(&printer->rwlock);
      }
      pthread_rwlock_unlock(&system->rwlock);

      if (count == 0)
        break;
    }

    // Clean out old jobs...
    if (system->clean_time && time(NULL) >= system->clean_time)
      lprintCleanJobs(system);
  }

  lprintLog(system, LPRINT_LOGLEVEL_INFO, "Shutting down main loop.");

  if (system->save_time)
  {
    // Save the configuration...
    pthread_rwlock_rdlock(&system->rwlock);
    save_config(system);
    pthread_rwlock_unlock(&system->rwlock);
  }
}


//
// 'create_listener()' - Create a listener socket.
//

static int				// O - Listener socket or -1 on error
create_listener(const char *name,	// I - Host name or `NULL` for any address
                int        port,	// I - Port number
                int        family)	// I - Address family
{
  int			sock;		// Listener socket
  http_addrlist_t	*addrlist;	// Listen address
  char			service[255];	// Service port


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
    return (-1);

  sock = httpAddrListen(&(addrlist->addr), port);

  httpAddrFreeList(addrlist);

  return (sock);
}


//
// 'get_config_file()' - Get the configuration filename.
//
// The configuration filename is, by convention, "~/.lprintrc".
//

static char *				// O - Filename
get_config_file(char   *buffer,		// I - Filename buffer
                size_t bufsize)		// I - Size of buffer
{
  const char	*home = getenv("HOME");	// HOME environment variable


  if (home)
    snprintf(buffer, bufsize, "%s/.lprintrc", home);
  else
#ifdef __APPLE__
    snprintf(buffer, bufsize, "/private/tmp/lprintrc.%d", getuid());
#else
    snprintf(buffer, bufsize, "/tmp/lprintrc.%d", getuid());
#endif // __APPLE__

  return (buffer);
}


//
// 'get_media_col()' - Get a media col value.
//

static void
get_media_col(
    const char         *value,		// I - Value string
    lprint_media_col_t *media)		// I - Media collection
{
  unsigned tracking = 0;		// Tracking value


  sscanf(value, "%d,%d,%d,%d,%d,%63[^,],%63[^,],%d,%d,%u,%63s\n", &media->bottom_margin, &media->left_margin, &media->right_margin, &media->size_width, &media->size_length, media->size_name, media->source, &media->top_margin, &media->top_offset, &tracking, media->type);
  media->tracking = tracking;
}




//
// 'load_config()' - Load the configuration file.
//

static int				// O - 1 on success, 0 on failure
load_config(lprint_system_t *system)	// I - System
{
  char		configfile[256];	// Configuration filename
  cups_file_t	*fp;			// File pointer
  char		line[1024],		// Line from file
		*value;			// Value from line
  int		linenum = 0;		// Line number in file

  // Try opening the config file...
  if ((fp = cupsFileOpen(get_config_file(configfile, sizeof(configfile)), "r")) == NULL)
    return (1);

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    if (!value)
    {
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Missing value for '%s' on line %d of '%s'.", line, linenum, configfile);
    }
    else if (!strcmp(line, "DefaultPrinterId"))
    {
      system->default_printer = atoi(value);
    }
    else if (!strcmp(line, "NextPrinterId"))
    {
      system->next_printer_id = atoi(value);
    }
    else if (!strcmp(line, "AdminGroup"))
    {
      if (!system->admin_group)
        system->admin_group = strdup(value);
    }
    else if (!strcmp(line, "AuthService"))
    {
      if (!system->auth_service)
        system->auth_service = strdup(value);
    }
    else if (!strcmp(line, "LogFile"))
    {
      if (!system->logfile)
        system->logfile = strdup(value);
    }
    else if (!strcmp(line, "LogLevel"))
    {
      if (system->loglevel != LPRINT_LOGLEVEL_UNSPEC)
        continue;

      if (!strcmp(value, "debug"))
        system->loglevel = LPRINT_LOGLEVEL_DEBUG;
      else if (!strcmp(value, "info"))
        system->loglevel = LPRINT_LOGLEVEL_INFO;
      else if (!strcmp(value, "warn"))
        system->loglevel = LPRINT_LOGLEVEL_WARN;
      else if (!strcmp(value, "error"))
        system->loglevel = LPRINT_LOGLEVEL_ERROR;
      else if (!strcmp(value, "fatal"))
        system->loglevel = LPRINT_LOGLEVEL_FATAL;
      else
	lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Bad LogLevel value '%s' on line %d of '%s'.", value, linenum, configfile);
    }
    else if (!strcmp(line, "Printer"))
    {
      lprint_printer_t	*printer;	// Printer
      char		*printer_name,	// Printer name
			*printer_id,	// Printer ID number
			*device_uri,	// Device URI
			*lprint_driver;	// Driver name
      ipp_attribute_t	*attr;		// IPP attribute

      printer_name = value;

      if ((printer_id = strchr(printer_name, ' ')) == NULL)
      {
        lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Bad Printer value '%s' on line %d of '%s'.", value, linenum, configfile);
        break;
      }

      if ((device_uri = strchr(printer_id + 1, ' ')) == NULL)
      {
        lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Bad Printer value '%s' on line %d of '%s'.", value, linenum, configfile);
        break;
      }

      if ((lprint_driver = strchr(device_uri + 1, ' ')) == NULL)
      {
        lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Bad Printer value '%s' on line %d of '%s'.", value, linenum, configfile);
        break;
      }

      *printer_id++    = '\0';
      *device_uri++    = '\0';
      *lprint_driver++ = '\0';

      printer = lprintCreatePrinter(system, atoi(printer_id), printer_name, lprint_driver, device_uri, NULL, NULL, NULL, NULL);

      if (printer->printer_id >= system->next_printer_id)
       system->next_printer_id = printer->printer_id + 1;

      while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
      {
        if (!strcmp(line, "EndPrinter"))
        {
          break;
	}
	else if (!value)
	{
	  lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Missing value for '%s' on line %d of '%s'.", line, linenum, configfile);
	}
	else
	{
	  // Delete any existing attribute...
	  if ((attr = ippFindAttribute(printer->attrs, line, IPP_TAG_ZERO)) != NULL)
	    ippDeleteAttribute(printer->attrs, attr);

	  if (!strcmp(line, "copies-default") || !strcmp(line, "print-darkness-default") || !strcmp(line, "print-speed-default"))
	  {
	    ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, line, atoi(value));
	  }
	  else if (!strcmp(line, "document-format-default"))
	  {
	    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, line, NULL, value);
	  }
	  else if (!strcmp(line, "print-quality-default") || !strcmp(line, "orientation-requested-default"))
	  {
	    ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, line, ippEnumValue(line, value));
	  }
	  else if (!strcmp(line, "label-mode-configured"))
	  {
	    printer->driver->mode_configured = lprintLabelModeValue(value);
	  }
	  else if (!strcmp(line, "label-tear-offset-configured"))
	  {
	    printer->driver->tear_offset_configured = atoi(value);
	  }
	  else if (!strcmp(line, "media-col-default"))
	  {
	    get_media_col(value, &printer->driver->media_default);
	  }
	  else if (!strncmp(line, "media-col-ready-", 16))
	  {
	    int	src = atoi(line + 16);	// Source index

            if (src >= 0 && src < printer->driver->num_source)
              get_media_col(value, printer->driver->media_ready + src);
	  }
	  else if (!strcmp(line, "print-color-mode-default") || !strcmp(line, "print-content-optimize-default"))
	  {
	    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, line, NULL, value);
	  }
	  else if (!strcmp(line, "printer-darkness-configured"))
	  {
	    printer->driver->darkness_configured = atoi(value);
	  }
	  else if (!strcmp(line, "printer-geo-location"))
	  {
	    printer->geo_location = strdup(value);
	  }
	  else if (!strcmp(line, "printer-location"))
	  {
	    printer->location = strdup(value);
	  }
	  else if (!strcmp(line, "printer-organization"))
	  {
	    printer->organization = strdup(value);
	  }
	  else if (!strcmp(line, "printer-organizational-unit"))
	  {
	    printer->org_unit = strdup(value);
	  }
	  else if (!strcmp(line, "printer-resolution-default"))
	  {
	    int		xres, yres;	// Resolution values
	    char	units[32];	// Resolution units

	    if (sscanf(value, "%dx%d%31s", &xres, &yres, units) != 3)
	    {
	      if (sscanf(value, "%d%31s", &xres, units) != 2)
	      {
		xres = 300;

		strlcpy(units, "dpi", sizeof(units));
	      }

	      yres = xres;
	    }

	    ippAddResolution(printer->attrs, IPP_TAG_PRINTER, "printer-resolution-default", !strcmp(units, "dpi") ? IPP_RES_PER_INCH : IPP_RES_PER_CM, xres, yres);
	  }
	  else
	  {
	    lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unsupported attribute '%s' with value '%s' on line %d of '%s'.", line, value, linenum, configfile);
	  }
	}
      }
    }
    else
    {
      lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unknown '%s %s' on line %d of '%s'.", line, value, linenum, configfile);
    }
  }

  cupsFileClose(fp);

  return (1);
}


//
// 'put_media_col()' - Put a media col value.
//

static void
put_media_col(
    cups_file_t        *fp,		// I - File to write to
    const char         *name,		// I - Name of attribute
    lprint_media_col_t *media)		// I - Media collection
{
  cupsFilePrintf(fp, "%s %d,%d,%d,%d,%d,%s,%s,%d,%d,%u,%s\n", name, media->bottom_margin, media->left_margin, media->right_margin, media->size_width, media->size_length, media->size_name, media->source, media->top_margin, media->top_offset, media->tracking, media->type);
}


//
// 'save_config()' - Save the configuration file.
//

static int				// O - 1 on success, 0 on failure
save_config(lprint_system_t *system)	// I - System
{
  char			configfile[256];// Configuration filename
  cups_file_t		*fp;		// File pointer
  lprint_printer_t	*printer;	// Current printer
  int			i;		// Looping var
  ipp_attribute_t	*attr;		// Printer attribute
  char			value[1024];	// Attribute value
  static const char * const llevels[] =	// Log level strings
  {
    "debug",
    "info",
    "warn",
    "error",
    "fatal"
  };
  static const char * const pattrs[] =	// List of printer attributes to save
  {
    "copies-default",
    "document-format-default-default",
    "orientation-requested-default",
    "print-color-mode-default",
    "print-content-optimize-default",
    "print-darkness-default",
    "print-quality-default",
    "print-speed-default",
    "printer-resolution-default"
  };


  if ((fp = cupsFileOpen(get_config_file(configfile, sizeof(configfile)), "w")) == NULL)
  {
    lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to save configuration to '%s': %s", configfile, strerror(errno));
    return (0);
  }

  lprintLog(system, LPRINT_LOGLEVEL_INFO, "Saving system configuration to '%s'.", configfile);

  cupsFilePrintf(fp, "DefaultPrinterId %d\n", system->default_printer);
  cupsFilePrintf(fp, "NextPrinterId %d\n", system->next_printer_id);

  if (system->admin_group)
    cupsFilePutConf(fp, "AdminGroup", system->admin_group);
  if (system->auth_service)
    cupsFilePutConf(fp, "AuthService", system->auth_service);

  if (system->logfile)
    cupsFilePutConf(fp, "LogFile", system->logfile);
  cupsFilePutConf(fp, "LogLevel", llevels[system->loglevel]);

  for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    cupsFilePrintf(fp, "Printer %s %d %s %s\n", printer->printer_name, printer->printer_id, printer->device_uri, printer->driver_name);
    if (printer->driver->mode_supported)
      cupsFilePutConf(fp, "label-mode-configured", lprintLabelModeString(printer->driver->mode_configured));
    if (printer->driver->tear_offset_supported[0] != printer->driver->tear_offset_supported[1])
      cupsFilePrintf(fp, "label-tear-offset-configured %d\n", printer->driver->tear_offset_configured);
    put_media_col(fp, "media-col-default", &printer->driver->media_default);
    for (i = 0; i < printer->driver->num_source; i ++)
    {
      if (printer->driver->media_ready[i].size_name[0])
      {
        snprintf(value, sizeof(value), "media-col-ready-%d", i);
        put_media_col(fp, value, printer->driver->media_ready + i);
      }
    }
    if (printer->driver->darkness_supported)
      cupsFilePrintf(fp, "printer-darkness-configured %d\n", printer->driver->darkness_configured);
    if (printer->geo_location)
      cupsFilePutConf(fp, "printer-geo-location", printer->geo_location);
    if (printer->location)
      cupsFilePutConf(fp, "printer-location", printer->location);
    if (printer->organization)
      cupsFilePutConf(fp, "printer-organization", printer->organization);
    if (printer->org_unit)
      cupsFilePutConf(fp, "printer-organizational-unit", printer->org_unit);

    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if ((attr = ippFindAttribute(printer->attrs, pattrs[i], IPP_TAG_ZERO)) != NULL)
      {
        ippAttributeString(attr, value, sizeof(value));
        cupsFilePutConf(fp, pattrs[i], value);
      }
    }

    cupsFilePuts(fp, "EndPrinter\n");
  }

  cupsFileClose(fp);

  return (1);
}


//
// 'sigterm_handler()' - SIGTERM handler.
//

static void
sigterm_handler(int sig)		// I - Signal (ignored)
{
  shutdown_system = 1;
}
