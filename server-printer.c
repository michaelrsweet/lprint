//
// Printer object for LPrint, a Label Printer Utility
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
#ifdef __APPLE__
#  include <sys/param.h>
#  include <sys/mount.h>
#else
#  include <sys/statfs.h>
#endif // __APPLE__

//
// Local functions...
//

static int	compare_active_jobs(lprint_job_t *a, lprint_job_t *b);
static int	compare_completed_jobs(lprint_job_t *a, lprint_job_t *b);
static int	compare_jobs(lprint_job_t *a, lprint_job_t *b);
static unsigned	lprint_rand(void);


//
// 'lprintCreatePrinter()' - Create a new printer.
//

lprint_printer_t *			// O - Printer
lprintCreatePrinter(
    lprint_system_t *system,		// I - System
    int             printer_id,		// I - printer-id value or 0 for new
    const char      *printer_name,	// I - Printer name
    const char      *driver_name,	// I - Driver name
    const char      *device_uri,	// I - Device URI
    const char      *geo_location,	// I - Geographic location or `NULL`
    const char      *location,		// I - Human-readable location or `NULL`
    const char      *organization,	// I - Organization
    const char      *org_unit)		// I - Organizational unit
{
  lprint_printer_t	*printer;	// Printer
  char			resource[1024],	// Resource path
			ipp_uri[1024],	// Printer URI
			ipps_uri[1024],	// Secure printer URI
			*uris[2],	// All URIs
			icons[1024],	// printer-icons URI
			adminurl[1024],	// printer-more-info URI
			supplyurl[1024],// printer-supply-info-uri URI
			uuid[128];	// printer-uuid
  int			k_supported;	// Maximum file size supported
  int			num_formats;	// Number of supported document formats
  const char		*formats[10];	// Supported document formats
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
  static const char * const ipp_versions[] =
  {					// ipp-versions-supported values
    "1.1",
    "2.0"
  };
  static const char * const ipp_features[] =
  {					// ipp-features-supported values
    "ipp-everywhere"
  };
  static const int	operations[] =	// operations-supported values
  {
    IPP_OP_PRINT_JOB,
    IPP_OP_VALIDATE_JOB,
    IPP_OP_CREATE_JOB,
    IPP_OP_SEND_DOCUMENT,
    IPP_OP_CANCEL_JOB,
    IPP_OP_GET_JOB_ATTRIBUTES,
    IPP_OP_GET_JOBS,
    IPP_OP_GET_PRINTER_ATTRIBUTES,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER
  };
  static const char * const charset[] =	// charset-supported values
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const compression[] =
  {					// compression-supported values
    "deflate",
    "gzip",
    "none"
  };
  static const char * const identify_actions[] =
  {					// identify-actions-supported values
    "display",
    "sound"
  };
  static const char * const job_creation_attributes[] =
  {					// job-creation-attributes-supported values
    "copies",
    "document-format",
    "document-name",
    "finishings",
    "ipp-attribute-fidelity",
    "job-name",
    "job-priority",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "print-color-mode",
    "print-content-optimize",
    "print-quality",
    "printer-resolution"
  };
  static const char * const media_col[] =
  {					// media-col-supported values
    "media-bottom-margin",
    "media-left-margin",
    "media-right-margin",
    "media-size",
    "media-size-name",
    "media-source",
    "media-top-margin",
    "media-type"
  };
  static const char * const multiple_document_handling[] =
  {					// multiple-document-handling-supported values
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
  };
  static const char * const uri_authentication[] =
  {					// uri-authentication-supported values
    "none",
    "none"
  };
  static const char * const uri_security[] =
  {					// uri-security-supported values
    "none",
    "tls"
  };
  static const char * const which_jobs[] =
  {					// which-jobs-supported values
    "completed",
    "not-completed",
    "all"
  };


  // Allocate memory for the printer...
  if ((printer = calloc(1, sizeof(lprint_printer_t))) == NULL)
  {
    lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to allocate memory for printer: %s", strerror(errno));
    return (NULL);
  }

  // Prepare URI values for the printer attributes...
  snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

  httpAssembleURI(HTTP_URI_CODING_ALL, ipp_uri, sizeof(ipp_uri), "ipp", NULL, system->hostname, system->port, resource);
  httpAssembleURI(HTTP_URI_CODING_ALL, ipps_uri, sizeof(ipps_uri), "ipps", NULL, system->hostname, system->port, resource);
  httpAssembleURI(HTTP_URI_CODING_ALL, icons, sizeof(icons), "https", NULL, system->hostname, system->port, "/lprint.png");
  httpAssembleURI(HTTP_URI_CODING_ALL, adminurl, sizeof(adminurl), "https", NULL, system->hostname, system->port, resource);
  httpAssembleURIf(HTTP_URI_CODING_ALL, supplyurl, sizeof(supplyurl), "https", NULL, system->hostname, system->port, "%s/supplies", resource);
  lprintMakeUUID(system, printer_name, 0, uuid, sizeof(uuid));

  // Get the maximum spool size based on the size of the filesystem used for
  // the spool directory.  If the host OS doesn't support the statfs call
  // or the filesystem is larger than 2TiB, always report INT_MAX.
  if (statfs(system->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize * spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;

  // Create the driver and assemble the final list of document formats...
  printer->driver = lprintCreateDriver(driver_name);

  num_formats = 0;
  formats[num_formats ++] = "application/octet-stream";

  if (printer->driver->format)
    formats[num_formats ++] = printer->driver->format;

  formats[num_formats ++] = "image/png";

  // Initialize printer structure and attributes...
  pthread_rwlock_init(&printer->rwlock, NULL);

  printer->system         = system;
  printer->name           = strdup(printer_name);
  printer->dnssd_name     = strdup(printer_name);
  printer->resource       = strdup(resource);
  printer->resourcelen    = strlen(resource);
  printer->device_uri     = strdup(device_uri);
  printer->driver_name    = strdup(driver_name);
  printer->geo_location   = geo_location ? strdup(geo_location) : NULL;
  printer->location       = location ? strdup(location) : NULL;
  printer->organization   = organization ? strdup(organization) : NULL;
  printer->org_unit       = org_unit ? strdup(org_unit) : NULL;
  printer->attrs          = ippNew();
  printer->start_time     = time(NULL);
  printer->config_time    = printer->start_time;
  printer->state          = IPP_PSTATE_IDLE;
  printer->state_reasons  = LPRINT_PREASON_NONE;
  printer->state_time     = printer->start_time;
  printer->jobs           = cupsArrayNew3((cups_array_func_t)compare_jobs, NULL, NULL, 0, NULL, (cups_afree_func_t)lprintDeleteJob);
  printer->active_jobs    = cupsArrayNew((cups_array_func_t)compare_active_jobs, NULL);
  printer->completed_jobs = cupsArrayNew((cups_array_func_t)compare_completed_jobs, NULL);
  printer->next_job_id    = 1;

  // charset-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  // charset-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charset) / sizeof(charset[0]), NULL, charset);

  // compression-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compression) / sizeof(compression[0])), NULL, compression);

  // document-format-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, "application/octet-stream");

  // document-format-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_formats, NULL, formats);

  // generated-natural-language-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  // identify-actions-default
  ippAddString (printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", NULL, "sound");

  // identify-actions-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-supported", sizeof(identify_actions) / sizeof(identify_actions[0]), NULL, identify_actions);

  // ipp-features-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-features-supported", sizeof(ipp_features) / sizeof(ipp_features[0]), NULL, ipp_features);

  // ipp-versions-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (int)(sizeof(ipp_versions) / sizeof(ipp_versions[0])), NULL, ipp_versions);

  // job-creation-attributes-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-creation-attributes-supported", (int)(sizeof(job_creation_attributes) / sizeof(job_creation_attributes[0])), NULL, job_creation_attributes);

  // job-ids-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  // job-k-octets-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0, k_supported);

  // job-priority-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-default", 50);

  // job-priority-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-supported", 1);

  // job-sheets-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-default", NULL, "none");

  // job-sheets-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-supported", NULL, "none");

  // media-col-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", (int)(sizeof(media_col) / sizeof(media_col[0])), NULL, media_col);

  // multiple-document-handling-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-supported", sizeof(multiple_document_handling) / sizeof(multiple_document_handling[0]), NULL, multiple_document_handling);

  // multiple-document-jobs-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 0);

  // multiple-operation-time-out
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  // multiple-operation-time-out-action
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  // natural-language-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "natural-language-configured", NULL, "en");

  // operations-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported", (int)(sizeof(operations) / sizeof(operations[0])), operations);

  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // printer-get-attributes-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  // printer-icons
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-icons", NULL, icons);

  // printer-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, printer_name);

  // printer-more-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, adminurl);

  // printer-name
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);

  // printer-supply-info-uri
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, supplyurl);

  // printer-uri-supported
  uris[0] = ipp_uri;
  uris[1] = ipps_uri;

  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", 2, NULL, (const char **)uris);

  // printer-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // uri-authentication-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication);

  // uri-security-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security);

  // which-jobs-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  // Add the printer to the system...
  pthread_rwlock_wrlock(&system->rwlock);

  cupsArrayAdd(system->printers, printer);

  // TODO: Set this in server-ipp.c since printers loaded from the config file shouldn't trigger a save?
  if (!system->save_time)
    system->save_time = time(NULL) + 1;

  pthread_rwlock_unlock(&system->rwlock);

  // Register the printer with Bonjour...
  if (system->subtypes)
    lprintRegisterDNSSD(printer);

  // Return it!
  return (printer);
}


//
// 'lprintDeletePrinter()' - Delete a printer.
//

void
lprintDeletePrinter(lprint_printer_t *printer)	/* I - Printer */
{
  lprintUnregisterDNSSD(printer);

  free(printer->name);
  free(printer->dnssd_name);
  free(printer->location);
  free(printer->geo_location);
  free(printer->organization);
  free(printer->org_unit);
  free(printer->resource);
  free(printer->device_uri);
  free(printer->driver_name);

  lprintDeleteDriver(printer->driver);
  ippDelete(printer->attrs);

  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->jobs);

  free(printer);
}


//
// 'lprintMakeUUID()' - Make a UUID for a system, printer, or job.
//
// Unlike httpAssembleUUID, this function does not introduce random data for
// printers and systems so the UUIDs are stable.
//

char *					// I - UUID string
lprintMakeUUID(
    lprint_system_t *system,		// I - System
    const char      *printer_name,	// I - Printer name or `NULL` for none
    int             job_id,		// I - Job ID or `0` for none
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of buffer
{
  char			data[1024];	// Source string for MD5
  unsigned char		sha256[32];	// SHA-256 digest/sum


  // Build a version 3 UUID conforming to RFC 4122.
  //
  // Start with the SHA-256 sum of the hostname, port, object name and
  // number, and some random data on the end for jobs (to avoid duplicates).
  if (printer_name && job_id)
    snprintf(data, sizeof(data), "_LPRINT_JOB_:%s:%d:%s:%d:%08x", system->hostname, system->port, printer_name, job_id, lprint_rand());
  else if (printer_name)
    snprintf(data, sizeof(data), "_LPRINT_PRINTER_:%s:%d:%s", system->hostname, system->port, printer_name);
  else
    snprintf(data, sizeof(data), "_LPRINT_SYSTEM_:%s:%d", system->hostname, system->port);

  cupsHashData("sha-256", (unsigned char *)data, strlen(data), sha256, sizeof(sha256));

  // Generate the UUID from the SHA-256...
  snprintf(buffer, bufsize, "urn:uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", sha256[0], sha256[1], sha256[3], sha256[4], sha256[5], sha256[6], (sha256[10] & 15) | 0x30, sha256[11], (sha256[15] & 0x3f) | 0x40, sha256[16], sha256[20], sha256[21], sha256[25], sha256[26], sha256[30], sha256[31]);

  return (buffer);
}


//
// 'compare_active_jobs()' - Compare two active jobs.
//

static int				// O - Result of comparison
compare_active_jobs(lprint_job_t *a,	// I - First job
                    lprint_job_t *b)	// I - Second job
{
  return (b->id - a->id);
}


//
// 'compare_completed_jobs()' - Compare two completed jobs.
//

static int				// O - Result of comparison
compare_completed_jobs(lprint_job_t *a,	// I - First job
                       lprint_job_t *b)	// I - Second job
{
  return (b->id - a->id);
}


//
// 'compare_jobs()' - Compare two jobs.
//

static int				// O - Result of comparison
compare_jobs(lprint_job_t *a,		// I - First job
             lprint_job_t *b)		// I - Second job
{
  return (b->id - a->id);
}


//
// 'lprint_rand()' - Return the best 32-bit random number we can.
//

static unsigned				// O - Random number
lprint_rand(void)
{
#if defined(__APPLE__) || defined(__OpenBSD__)
  // arc4random uses real entropy automatically...
  return (arc4random());

#else
#  ifdef __linux
  // Linux has the getrandom function to get real entropy, but can fail...
  unsigned	buffer;			// Random number buffer

  if (getrandom(&buffer, sizeof(buffer)) == sizeof(buffer))
    return (buffer);
#  endif // __linux

  // Fall back to random() seeded with the current time - not ideal, but for
  // our non-cryptographic purposes this is OK...
  static int first_time = 1;		// First time we ran?

  if (first_time)
  {
    srandom(time(NULL));
    first_time = 0;
  }

  return ((unsigned)random());
#endif // __APPLE__
}
