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
    lprint_printer_t *printer,		// I - Printer
    const char       *subtypes)		// I - Subtypes
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
