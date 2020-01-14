//
// Printer object for LPrint, a Label Printer Application
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
#ifdef __APPLE__
#  include <sys/param.h>
#  include <sys/mount.h>
#else
#  include <sys/statfs.h>
#endif // __APPLE__
#ifdef HAVE_SYS_RANDOM_H
#  include <sys/random.h>
#endif // HAVE_SYS_RANDOM_H
#ifdef HAVE_GNUTLS_RND
#  include <gnutls/crypto.h>
#endif // HAVE_GNUTLS_RND


//
// Local functions...
//

static int	compare_active_jobs(lprint_job_t *a, lprint_job_t *b);
static int	compare_completed_jobs(lprint_job_t *a, lprint_job_t *b);
static int	compare_jobs(lprint_job_t *a, lprint_job_t *b);
static int	compare_printers(lprint_printer_t *a, lprint_printer_t *b);
static void	free_printer(lprint_printer_t *printer);
static unsigned	lprint_rand(void);
static ipp_t	*make_xri(const char *uri, const char *authentication, const char *security);


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
			icons[2][1024],	// printer-icons URIs
			adminurl[1024],	// printer-more-info URI
			supplyurl[1024],// printer-supply-info-uri URI
			uuid[128];	// printer-uuid
  ipp_t			*xris[2];	// All XRIs
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
    IPP_OP_SET_PRINTER_ATTRIBUTES,
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
    "ipp-attribute-fidelity",
    "job-name",
    "job-priority",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "print-color-mode",
    "print-content-optimize",
    "print-darkness",
    "print-quality",
    "print-speed",
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
    "media-top-offset",
    "media-tracking",
    "media-type"
  };
  static const char * const multiple_document_handling[] =
  {					// multiple-document-handling-supported values
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
  };
  static const int orientation_requested[] =
  {
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT,
    IPP_ORIENT_NONE
  };
  static const char * const print_color_mode[] =
  {					// print-color-mode-supported
    "bi-level",
    "monochrome"
  };
  static const char * const print_content_optimize[] =
  {					// print-content-optimize-supported
    "auto",
    "graphic",
    "photo",
    "text-and-graphic",
    "text"
  };
  static const int print_quality[] =	// print-quality-supported
  {
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const printer_kind[] =
  {					// printer-kind values
    "labels",
    "receipt"
  };
  static const char * const printer_settable_attributes[] =
  {					// printer-settable-attributes values
    "copies-default",
    "document-format-default",
    "label-mode-configured",
    "label-tear-off-configured",
    "media-col-default",
    "media-col-ready",
    "media-default",
    "media-ready",
    "multiple-document-handling-default",
    "orientation-requested-default",
    "print-color-mode-default",
    "print-content-optimize-default",
    "print-darkness-default",
    "print-quality-default",
    "print-speed-default",
    "printer-darkness-configured",
    "printer-geo-location",
    "printer-location",
    "printer-organization",
    "printer-organizational-unit",
    "printer-resolution-default"
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
  httpAssembleURI(HTTP_URI_CODING_ALL, icons[0], sizeof(icons[0]), "https", NULL, system->hostname, system->port, "/lprint.png");
  httpAssembleURI(HTTP_URI_CODING_ALL, icons[1], sizeof(icons[1]), "https", NULL, system->hostname, system->port, "/lprint-large.png");
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

  if (printer->driver->format && strcmp(printer->driver->format, "application/octet-stream"))
    formats[num_formats ++] = printer->driver->format;

#ifdef HAVE_LIBPNG
  formats[num_formats ++] = "image/png";
#endif // HAVE_LIBPNG
  formats[num_formats ++] = "image/pwg-raster";
  formats[num_formats ++] = "image/urf";

  // Initialize printer structure and attributes...
  pthread_rwlock_init(&printer->rwlock, NULL);

  printer->system         = system;
  printer->printer_name   = strdup(printer_name);
  printer->dns_sd_name    = strdup(printer_name);
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

  // copies-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  // copies-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "copies-supported", 1, 999);

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

  // orientation-requested-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-default", IPP_ORIENT_NONE);

  // orientation-requested-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);

  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // print-color-mode-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, "monochrome");

  // print-color-mode-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode) / sizeof(print_color_mode[0])), NULL, print_color_mode);

  // print-content-optimize-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-default", NULL, "auto");

  // print-content-optimize-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", (int)(sizeof(print_content_optimize) / sizeof(print_content_optimize[0])), NULL, print_content_optimize);

  // print-quality-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-default", IPP_QUALITY_NORMAL);

  // print-quality-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality) / sizeof(print_quality[0])), print_quality);

  // printer-get-attributes-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  // printer-icons
  uris[0] = icons[0];
  uris[1] = icons[1];

  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-icons", 2, NULL, (const char * const *)uris);

  // printer-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, printer_name);

  // printer-kind
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-kind", (int)(sizeof(printer_kind) / sizeof(printer_kind[0])), NULL, printer_kind);

  // printer-more-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-more-info", NULL, adminurl);

  // printer-name
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);

  // printer-settable-attributes
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-settable-attributes", (int)(sizeof(printer_settable_attributes) / sizeof(printer_settable_attributes[0])), NULL, printer_settable_attributes);

  // printer-supply-info-uri
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-supply-info-uri", NULL, supplyurl);

  // printer-uri-supported
  uris[0] = ipp_uri;
  uris[1] = ipps_uri;

  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri-supported", 2, NULL, (const char * const *)uris);

  // printer-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // printer-xri-supported
  xris[0] = make_xri(ipp_uri, uri_authentication[0], uri_security[0]);
  xris[1] = make_xri(ipps_uri, uri_authentication[1], uri_security[1]);

  printer->xri_supported = ippAddCollections(printer->attrs, IPP_TAG_PRINTER, "printer-xri-supported", 2, (const ipp_t **)xris);

  ippDelete(xris[0]);
  ippDelete(xris[1]);

  // uri-authentication-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-authentication-supported", 2, NULL, uri_authentication);

  // uri-security-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security);

  // which-jobs-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);

  // Add the printer to the system...
  pthread_rwlock_wrlock(&system->rwlock);

  if (printer_id)
    printer->printer_id = printer_id;
  else
    printer->printer_id = system->next_printer_id ++;

  if (!system->printers)
    system->printers = cupsArrayNew3((cups_array_func_t)compare_printers, NULL, NULL, 0, NULL, (cups_afree_func_t)free_printer);

  cupsArrayAdd(system->printers, printer);

  if (!system->default_printer)
    system->default_printer = printer->printer_id;

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
  // Remove the printer from the system object...
  pthread_rwlock_wrlock(&printer->system->rwlock);
  cupsArrayRemove(printer->system->printers, printer);
  pthread_rwlock_unlock(&printer->system->rwlock);
}


//
// 'lprintFindPrinter()' - Find a printer by resource...
//

lprint_printer_t *			// O - Printer or `NULL` if none
lprintFindPrinter(
    lprint_system_t *system,		// I - System
    const char      *resource,		// I - Resource path or `NULL`
    int             printer_id)		// I - Printer ID or `0`
{
  lprint_printer_t	*printer;	// Matching printer


  lprintLog(system, LPRINT_LOGLEVEL_DEBUG, "lprintFindPrinter(system, resource=\"%s\", printer_id=%d)", resource, printer_id);

  pthread_rwlock_rdlock(&system->rwlock);

  if (resource && (!strcmp(resource, "/ipp/print") || (!strncmp(resource, "/ipp/print/", 11) && isdigit(resource[11] & 255))))
  {
    printer_id = system->default_printer;
    resource   = NULL;

    lprintLog(system, LPRINT_LOGLEVEL_DEBUG, "lprintFindPrinter: Looking for default printer_id=%d", printer_id);
  }

  for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    lprintLog(system, LPRINT_LOGLEVEL_DEBUG, "lprintFindPrinter: printer '%s' - resource=\"%s\", printer_id=%d", printer->printer_name, printer->resource, printer->printer_id);

    if (resource && !strncmp(printer->resource, resource, printer->resourcelen) && (!resource[printer->resourcelen] || resource[printer->resourcelen] == '/'))
      break;
    else if (printer->printer_id == printer_id)
      break;
  }
  pthread_rwlock_unlock(&system->rwlock);

  lprintLog(system, LPRINT_LOGLEVEL_DEBUG, "lprintFindPrinter: Returning %p(%s)", printer, printer ? printer->printer_name : "none");

  return (printer);
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
// 'compare_printers()' - Compare two printers.
//

static int				// O - Result of comparison
compare_printers(lprint_printer_t *a,	// I - First printer
                 lprint_printer_t *b)	// I - Second printer
{
  return (strcmp(a->printer_name, b->printer_name));
}


//
// 'free_printer()' - Free the memory used by a printer.
//

static void
free_printer(lprint_printer_t *printer)	// I - Printer
{
  // Remove DNS-SD registrations...
  lprintUnregisterDNSSD(printer);

  // Free memory...
  free(printer->printer_name);
  free(printer->dns_sd_name);
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
// 'lprint_rand()' - Return the best 32-bit random number we can.
//

static unsigned				// O - Random number
lprint_rand(void)
{
#ifdef HAVE_ARC4RANDOM
  // arc4random uses real entropy automatically...
  return (arc4random());

#else
#  ifdef HAVE_GETRANDOM
  // Linux has the getrandom function to get real entropy, but can fail...
  unsigned	buffer;			// Random number buffer

  if (getrandom(&buffer, sizeof(buffer), 0) == sizeof(buffer))
    return (buffer);

#  elif defined(HAVE_GNUTLS_RND)
  // GNU TLS has the gnutls_rnd function we can use as well, but can fail...
  unsigned	buffer;			// Random number buffer

  if (!gnutls_rnd(GNUTLS_RND_NONCE, &buffer, sizeof(buffer)))
    return (buffer);
#  endif // HAVE_GETRANDOM

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


//
// 'make_xri()' - Make a printer-xri collection value.
//

static ipp_t *				// O - Collection value
make_xri(const char *uri,		// I - xri-uri
         const char *authentication,	// I - xri-authentication
         const char *security)		// I - xri-security
{
  ipp_t	*col = ippNew();		// Collection value


  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "xri-authentication", NULL, authentication);
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "xri-security", NULL, security);
  ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_URI, "xri-uri", NULL, uri);

  return (col);
}
