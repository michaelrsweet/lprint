//
// Header file for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _LPRINT_H_
#  define _LPRINT_H_

//
// Include necessary headers...
//

#  include "config.h"
#  include <cups/cups.h>
#  include <cups/raster.h>
#  include <limits.h>
#  include <poll.h>
#  include <pthread.h>
#  include <sys/fcntl.h>
#  include <sys/stat.h>
#  include <sys/wait.h>

extern char **environ;

#  ifdef HAVE_DNSSD
#    include <dns_sd.h>
#  elif defined(HAVE_AVAHI)
#    include <avahi-client/client.h>
#    include <avahi-client/publish.h>
#    include <avahi-common/error.h>
#    include <avahi-common/thread-watch.h>
#  endif // HAVE_DNSSD


//
// Wrappers for global constants...
//

#  ifdef _LPRINT_C_
#    VAR
#    VALUE(...)		= __VA_ARGS__
#  else
#    VAR		extern
#    VALUE(...)
#  endif // _LPRINT_C_


//
// Constants...
//

enum lprint_preason_e			// printer-state-reasons bit values
{
  LPRINT_PREASON_NONE = 0x0000,		// none
  LPRINT_PREASON_OTHER = 0x0001,	// other
  LPRINT_PREASON_COVER_OPEN = 0x0002,	// cover-open
  LPRINT_PREASON_MEDIA_EMPTY = 0x0004,	// media-empty
  LPRINT_PREASON_MEDIA_JAM = 0x0008,	// media-jam
  LPRINT_PREASON_MEDIA_LOW = 0x0010,	// media-low
  LPRINT_PREASON_MEDIA_NEEDED = 0x0020	// media-needed
};
typedef unsigned int lprint_preason_t;	// Bitfield for printer-state-reasons
VAR const char * const lprint_preason_strings[]
VALUE({					// Strings for each bit
  // "none" is implied for no bits set
  "other",
  "cover-open",
  "input-tray-missing",
  "marker-supply-empty",
  "marker-supply-low",
  "marker-waste-almost-full",
  "marker-waste-full",
  "media-empty",
  "media-jam",
  "media-low",
  "media-needed",
  "moving-to-paused",
  "paused",
  "spool-area-full",
  "toner-empty",
  "toner-low"
});


//
// Types and structures...
//

#  ifdef HAVE_DNSSD
typedef DNSServiceRef lprint_srv_t;	// Service reference
typedef TXTRecordRef lprint_txt_t;	// TXT record

#elif defined(HAVE_AVAHI)
typedef AvahiEntryGroup *lprint_srv_t;	// Service reference
typedef AvahiStringList *lprint_txt_t;	// TXT record

#else
typedef void *lprint_srv_t;		// Service reference
typedef void *lprint_txt_t;		// TXT record
#endif // HAVE_DNSSD

typedef struct lprint_filter_s		// Attribute filter
{
  cups_array_t		*ra;		// Requested attributes
  ipp_tag_t		group_tag;	// Group to copy
} lprint_filter_t;

typedef struct lprint_printer_s lprint_printer_t;
typedef struct lprint_job_s lprint_job_t;

typedef struct lprint_system_s		// System data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  char			*hostname;	// Hostname
  int			port;		// Port number, if any
  char			*directory;	// Spool directory
  int			domain,		// UNIX domain socket listener
			ipv4,		// IPv4 listener, if any
			ipv6;		// IPv6 listener, if any
  cups_array_t		*clients;	// Array of client connections
  cups_array_t		*printers;	// Array of printers
  int			default_printer,// Default printer-id
			next_printer_id;// Next printer-id
  cups_array_t		*active_jobs,	// Array of active jobs
			*jobs;		// Array of all jobs
  int			next_job_id;	// Next job-id
} lprint_system_t;

struct lprint_printer_s			// Printer data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  lprint_system_t	*system;	// Containing system
  lprint_srv_t		ipp_ref,	// Bonjour IPP service
			ipps_ref,	// Bonjour IPPS service
			http_ref,	// Bonjour HTTP service
			printer_ref;	// Bonjour LPD service
  char			*dnssd_name,	// printer-dnssd-name
			*name,		// printer-name
			*icon,		// Icon filename
			*uri;		// printer-uri-supported
  size_t		urilen;		// Length of printer URI
  char			*device_uri,	// Device URI (if any)
			*output_format;	// Output format
  ipp_t			*attrs;		// Static attributes
  time_t		start_time;	// Startup time
  time_t		config_time;	// printer-config-change-time
  ipp_pstate_t		state;		// printer-state value
  lprint_preason_t	state_reasons;	// printer-state-reasons values
  time_t		state_time;	// printer-state-change-time
} lprint_printer_t;

struct lprint_job_s			// Job data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  lprint_system_t	*system;	// Containing system
  lprint_printer_t	*printer;	// Printer
  int			id;		// Job ID
  const char		*name,		// job-name
			*username,	// job-originating-user-name
			*format;	// document-format
  ipp_jstate_t		state;		// job-state value
  char			*message;	// job-state-message value
  int			msglevel;	// job-state-message log level (0=error, 1=info)
  time_t		created,	// [date-]time-at-creation value
			processing,	// [date-]time-at-processing value
			completed;	// [date-]time-at-completed value
  int			impressions,	// job-impressions value
			impcompleted;	// job-impressions-completed value
  ipp_t			*attrs;		// Static attributes
  int			cancel;		// Non-zero when job canceled
  char			*filename;	// Print file name
  int			fd;		// Print file descriptor
};

typedef struct lprint_client_s		// Client data
{
  lprint_system_t	*system;	// Containing system
  http_t		*http;		// HTTP connection
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  time_t		start;		// Request start time
  http_state_t		operation;	// Request operation
  ipp_op_t		operation_id;	// IPP operation-id
  char			uri[1024],	// Request URI
			*options;	// URI options
  http_addr_t		addr;		// Client address
  char			hostname[256];	// Client hostname
  lprint_printer_t	*printer;	// Printer, if any
  lprint_job_t		*job;		// Job, if any
} lprint_client_t;


//
// Functions...
//

static void		lprintCleanJobs(lprint_system_t *system);
static lprint_client_t	*lprintCreateClient(lprint_system_t *system, int sock);
static lprint_job_t	*lprintCreateJob(lprint_client_t *client);
static lprint_printer_t	*lprintCreatePrinter(lprint_client_t *client);
static int		lprintCreateJobFile(lprint_job_t *job, char *fname, size_t fnamesize, const char *dir, const char *ext);
static void		lprintLogAttributes(const char *title, ipp_t *ipp, int is_response);
static void		lprintDeleteClient(lprint_client_t *client);
static void		lprintDeleteJob(lprint_job_t *job);
static void		lprintDeletePrinter(lprint_printer_t *printer);
static void		lprintInitDNSSD(void);
static lprint_job_t	*lprintFindJob(lprint_client_t *client);
static lprint_printer_t	*lprintFindPrinter(lprint_client_t *client);
static void		finish_document_data(lprint_client_t *client, lprint_job_t *job);
static void		finish_document_uri(lprint_client_t *client, lprint_job_t *job);

static ipp_t		*load_ippserver_attributes(const char *servername, int serverport, const char *filename, cups_array_t *docformats);
static ipp_t		*load_legacy_attributes(const char *make, const char *model, int ppm, int ppm_color, int duplex, cups_array_t *docformats);
#if !CUPS_LITE
static ipp_t		*load_ppd_attributes(const char *ppdfile, cups_array_t *docformats);
#endif // !CUPS_LITE
static int		parse_options(lprint_client_t *client, cups_option_t **options);
static void		process_attr_message(lprint_job_t *job, char *message);
static void		*process_client(lprint_client_t *client);
static int		process_http(lprint_client_t *client);
static int		process_ipp(lprint_client_t *client);
static void		*process_job(lprint_job_t *job);
static void		process_state_message(lprint_job_t *job, char *message);
static int		register_printer(lprint_printer_t *printer, const char *subtypes);
static int		respond_http(lprint_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length);
static void		respond_ipp(lprint_client_t *client, ipp_status_t status, const char *message, ...) _CUPS_FORMAT(3, 4);
static void		respond_unsupported(lprint_client_t *client, ipp_attribute_t *attr);
static void		run_printer(lprint_printer_t *printer);
static int		show_media(lprint_client_t *client);
static int		show_status(lprint_client_t *client);
static int		show_supplies(lprint_client_t *client);
static char		*time_string(time_t tv, char *buffer, size_t bufsize);
static void		usage(int status) _CUPS_NORETURN;
static int		valid_doc_attributes(lprint_client_t *client);
static int		valid_job_attributes(lprint_client_t *client);

#endif // !_LPRINT_H_
