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


//
// 'lprintCreateSystem()' - Create a system object.
//

lprint_system_t *			// O - System object
lprintCreateSystem(
    const char        *name,		// I - Hostname or `NULL` for none
    int               port,		// I - Port number or `0` for none
    FILE              *logfile,		// I - Log file or `NULL` for syslog
    lprint_loglevel_t loglevel)		// I - Log level
{


#if 0
  // Setup poll() data for the Bonjour service socket and IPv4/6 listeners...
  num_fds = 1;

  if ()
  polldata[0].fd     = system->ipv4;
  polldata[0].events = POLLIN;

  polldata[1].fd     = system->ipv6;
  polldata[1].events = POLLIN;

  num_fds = 2;
#endif // 0


}


//
// 'lprintDeleteSystem()' - Delete a system object.
//

void
lprintDeleteSystem(
    lprint_system_t *system)		// I - System object
{
}


//
// 'lprintRunSystem()' - Run the printer service.
//

static void
lprintRunSystem(lprint_system_t *system)// I - System
{
  int			i,		// Looping var
			count,		// Number of listeners that fired
			timeout;	// Timeout for poll()
  lprint_client_t	*client;	// New client


  // Loop until we are shutdown or have a hard error...
  for (;;)
  {
    if (cupsArrayCount(system->jobs) || system->shutdown_requested)
      timeout = 10;
    else
      timeout = -1;

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
    else
    {
      // See if we are expected to shutdown...
      if (system->shutdown_requested)
        break;



   /*
    * Clean out old jobs...
    */

    lprintCleanJobs(system);
  }
}


/*
 * 'create_listener()' - Create a listener socket.
 */

static int				/* O - Listener socket or -1 on error */
create_listener(const char *name,	/* I - Host name (`NULL` for any address) */
                int        port,	/* I - Port number */
                int        family)	/* I - Address family */
{
  int			sock;		/* Listener socket */
  http_addrlist_t	*addrlist;	/* Listen address */
  char			service[255];	/* Service port */


  snprintf(service, sizeof(service), "%d", port);
  if ((addrlist = httpAddrGetList(name, family, service)) == NULL)
    return (-1);

  sock = httpAddrListen(&(addrlist->addr), port);

  httpAddrFreeList(addrlist);

  return (sock);
}
