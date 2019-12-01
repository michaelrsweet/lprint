//
// System object for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// Local functions...
//

static int		create_listener(const char *name, int port, int family);
static char		*get_config_file(char *buffer, size_t bufsize);
static int		load_config(lprint_system_t *system);
static int		save_config(lprint_system_t *system);


//
// 'lprintCreateSystem()' - Create a system object.
//

lprint_system_t *			// O - System object
lprintCreateSystem(
    const char        *hostname,	// I - Hostname or `NULL` for none
    int               port,		// I - Port number or `0` for none
    const char        *subtypes,	// I - DNS-SD sub-types or `NULL` for none
    const char        *logfile,		// I - Log file or `NULL` for syslog
    lprint_loglevel_t loglevel)		// I - Log level
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

  snprintf(spooldir, sizeof(spooldir), "%s/lprint.%d", tmpdir, getpid());
  if (mkdir(spooldir, 0700))
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

  system->directory = strdup(spooldir);

  if (logfile)
  {
    system->logfile = strdup(logfile);

    if (!strcmp(logfile, "-"))
    {
      // Log to stderr...
      system->logfd = 2;
    }
    else if ((system->logfd = open(logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600)) < 0)
    {
      // Fallback to stderr if we can't open the log file...
      perror(logfile);

      system->logfd = 2;
    }
  }
  else
    system->logfd = -1;

  system->loglevel = loglevel;

  if (subtypes)
    system->subtypes = strdup(subtypes);

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
    if ((system->listeners[system->num_listeners].fd = create_listener(system->hostname, system->port, AF_INET)) < 0)
    {
      lprintLog(system, LPRINT_LOGLEVEL_FATAL, "Unable to create IPv4 listener for %s:%d: %s", system->hostname, system->port, strerror(errno));
      goto fatal;
    }
    else
      system->listeners[system->num_listeners ++].events = POLLIN;

    if ((system->listeners[system->num_listeners].fd = create_listener(system->hostname, system->port, AF_INET6)) < 0)
    {
      lprintLog(system, LPRINT_LOGLEVEL_FATAL, "Unable to create IPv6 listener for %s:%d: %s", system->hostname, system->port, strerror(errno));
      goto fatal;
    }
    else
      system->listeners[system->num_listeners ++].events = POLLIN;
  }

  // Initialize DNS-SD as needed...
  if (system->subtypes)
    lprintInitDNSSD(system);

  // Load printers
  if (!load_config(system))
    goto fatal;

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


  if (!system)
    return;

  free(system->hostname);
  free(system->directory);
  free(system->logfile);
  free(system->subtypes);

  if (system->logfd > 2)
    close(system->logfd);

  for (i = 0; i < system->num_listeners; i ++)
    close(system->listeners[i].fd);

  cupsArrayDelete(system->clients);
  cupsArrayDelete(system->printers);

  pthread_rwlock_destroy(&system->rwlock);

  free(system);
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


  // Loop until we are shutdown or have a hard error...
  for (;;)
  {
    // TODO: Fix "pending jobs" logic
//    if (cupsArrayCount(system->jobs) || system->save_needed || system->shutdown_requested)
    if (system->save_needed || system->shutdown_requested)
      timeout = 5;
    else
      timeout = 60;

    if ((count = poll(system->listeners, (nfds_t)system->num_listeners, timeout)) < 0 && errno != EINTR && errno != EAGAIN)
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

    if (system->save_needed)
    {
      // Save the configuration...
      save_config(system);
      system->save_needed = 0;
    }

    if (system->shutdown_requested)
    {
      // Shutdown requested, see if we can do so safely...
      // TODO: Fix active job logic
//      if (cupsArrayCount(system->active_jobs) == 0)
        break;
    }

    // Clean out old jobs...
    lprintCleanJobs(system);
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
// 'load_config()' - Load the configuration file.
//

static int				// O - 1 on success, 0 on failure
load_config(lprint_system_t *system)	// I - System
{
  char		configfile[256];	// Configuration filename
#if 0
  cups_file_t	*fp;			// File pointer
  char		line[1024],		// Line from file
		*value;			// Value from line
#endif // 0

  printf("LOAD: %s\n", get_config_file(configfile, sizeof(configfile)));

  (void)system;
  return (1);
}


//
// 'save_config()' - Save the configuration file.
//

static int				// O - 1 on success, 0 on failure
save_config(lprint_system_t *system)	// I - System
{
  char		configfile[256];	// Configuration filename
#if 0
  cups_file_t	*fp;			// File pointer
#endif // 0

  printf("LOAD: %s\n", get_config_file(configfile, sizeof(configfile)));

  (void)system;
  return (1);
}
