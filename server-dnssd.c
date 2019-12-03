//
// DNS-SD support for LPrint, a Label Printer Utility
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
// Local globals...
//

#ifdef HAVE_DNSSD
static DNSServiceRef	DNSSDMaster = NULL;
#elif defined(HAVE_AVAHI)
static AvahiThreadedPoll *DNSSDMaster = NULL;
static AvahiClient	*DNSSDClient = NULL;
#endif /* HAVE_DNSSD */


//
// Local functions...
//

#ifdef HAVE_DNSSD
static void DNSSD_API	dnssd_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, lprint_printer_t *printer);
static void		*dnssd_run(void *data);
#elif defined(HAVE_AVAHI)
static void		dnssd_callback(AvahiEntryGroup *p, AvahiEntryGroupState state, void *context);
static void		dnssd_client_cb(AvahiClient *c, AvahiClientState state, void *userdata);
#endif /* HAVE_DNSSD */


//
// 'lprintInitDNSSD()' - Initialize DNS-SD registration threads...
//

void
lprintInitDNSSD(lprint_system_t *system)// I - System
{
#ifdef HAVE_DNSSD
  if (DNSServiceCreateConnection(&DNSSDMaster) != kDNSServiceErr_NoError)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

#elif defined(HAVE_AVAHI)
  int error;			/* Error code, if any */

  if ((DNSSDMaster = avahi_threaded_poll_new()) == NULL)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

  if ((DNSSDClient = avahi_client_new(avahi_threaded_poll_get(DNSSDMaster), AVAHI_CLIENT_NO_FAIL, dnssd_client_cb, NULL, &error)) == NULL)
  {
    fputs("Error: Unable to initialize Bonjour.\n", stderr);
    exit(1);
  }

  avahi_threaded_poll_start(DNSSDMaster);
#endif /* HAVE_DNSSD */
}


//
// 'lprintRegisterDNSSD()' - Register a printer's DNS-SD service.
//

int					// O - 1 on success, 0 on failure
lprintRegisterDNSSD(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (0);
}


//
// 'lprintUnregisterDNSSD()' - Unregister a printer's DNS-SD service.
//

void
lprintUnregisterDNSSD(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;
}


#ifdef HAVE_DNSSD
/*
 * 'dnssd_callback()' - Handle Bonjour registration events.
 */

static void DNSSD_API
dnssd_callback(
    DNSServiceRef       sdRef,		/* I - Service reference */
    DNSServiceFlags     flags,		/* I - Status flags */
    DNSServiceErrorType errorCode,	/* I - Error, if any */
    const char          *name,		/* I - Service name */
    const char          *regtype,	/* I - Service type */
    const char          *domain,	/* I - Domain for service */
    lprint_printer_t    *printer)	/* I - Printer */
{
  (void)sdRef;
  (void)flags;
  (void)domain;

  if (errorCode)
  {
    lprintLogPrinter(printer, LPRINT_LOGLEVEL_ERROR, "DNSServiceRegister for '%s' failed with error %d.", regtype, (int)errorCode);
    return;
  }
  else if (strcasecmp(name, printer->dnssd_name))
  {
    lprintLogPrinter(printer, LPRINT_LOGLEVEL_INFO, "Now using DNS-SD service name '%s'.\n", name);

    pthread_rwlock_wrlock(&printer->rwlock);

    free(printer->dnssd_name);
    printer->dnssd_name = strdup(name);

    pthread_rwlock_unlock(&printer->rwlock);
  }
}


#elif defined(HAVE_AVAHI)
/*
 * 'dnssd_callback()' - Handle Bonjour registration events.
 */

static void
dnssd_callback(
    AvahiEntryGroup      *srv,		/* I - Service */
    AvahiEntryGroupState state,		/* I - Registration state */
    void                 *context)	/* I - Printer */
{
  (void)srv;
  (void)state;
  (void)context;
}


/*
 * 'dnssd_client_cb()' - Client callback for Avahi.
 *
 * Called whenever the client or server state changes...
 */

static void
dnssd_client_cb(
    AvahiClient      *c,		/* I - Client */
    AvahiClientState state,		/* I - Current state */
    void             *userdata)		/* I - User data (unused) */
{
  (void)userdata;

  if (!c)
    return;

  switch (state)
  {
    default :
        fprintf(stderr, "Ignored Avahi state %d.\n", state);
	break;

    case AVAHI_CLIENT_FAILURE:
	if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
	{
	  fputs("Avahi server crashed, exiting.\n", stderr);
	  exit(1);
	}
	break;
  }
}
#endif /* HAVE_DNSSD */

