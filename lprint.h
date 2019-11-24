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

#  include "device-common.h"
#  include "driver-common.h"

#  include <limits.h>
#  include <poll.h>
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
#    define VAR
#    define VALUE(...)	= __VA_ARGS__
#  else
#    define VAR		extern
#    define VALUE(...)
#  endif // _LPRINT_C_


//
// Constants...
//

typedef enum lprint_loglevel_e		// Log levels
{
  LPRINT_LOGLEVEL_DEBUG,
  LPRINT_LOGLEVEL_INFO,
  LPRINT_LOGLEVEL_WARN,
  LPRINT_LOGLEVEL_ERROR
} lprint_loglevel_t;

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
			*driver_name;	// Driver name (if any)
  lprint_driver_t	*driver;	// Driver
  ipp_t			*attrs;		// Static attributes
  time_t		start_time;	// Startup time
  time_t		config_time;	// printer-config-change-time
  ipp_pstate_t		state;		// printer-state value
  lprint_preason_t	state_reasons;	// printer-state-reasons values
  time_t		state_time;	// printer-state-change-time
  time_t		status_time;	// Last time status was updated
  int			impcompleted;	// printer-impressions-completed
};

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

extern void		lprintCleanJobs(lprint_system_t *system);
extern http_t		*lprintConnect(void);
extern lprint_client_t	*lprintCreateClient(lprint_system_t *system, int sock);
extern lprint_job_t	*lprintCreateJob(lprint_client_t *client);
extern int		lprintCreateJobFile(lprint_job_t *job, char *fname, size_t fnamesize, const char *dir, const char *ext);
extern lprint_printer_t	*lprintCreatePrinter(lprint_client_t *client);
extern void		lprintDeleteClient(lprint_client_t *client);
extern void		lprintDeleteJob(lprint_job_t *job);
extern void		lprintDeletePrinter(lprint_printer_t *printer);
extern int		lprintDoCancel(int argc, char *argv[]);
extern int		lprintDoConfig(int argc, char *argv[]);
extern int		lprintDoJobs(int argc, char *argv[]);
extern int		lprintDoPrinters(int argc, char *argv[]);
extern int		lprintDoServer(int argc, char *argv[]);
extern int		lprintDoShutdown(int argc, char *argv[]);
extern int		lprintDoSubmit(int argc, char *argv[]);
extern char		*lprintGetServerPath(char *buffer, size_t bufsize);
extern void		lprintInitDNSSD(void);
extern int		lprintIsServerRunning(void);
extern lprint_job_t	*lprintFindJob(lprint_client_t *client);
extern lprint_printer_t	*lprintFindPrinter(lprint_client_t *client);
extern void		lprintLog(lprint_system_t *system, lprint_loglevel_t level, const char *message, ...);
extern void		lprintLogAttributes(lprint_client_t *client, const char *title, ipp_t *ipp, int is_response);
extern void		lprintLogClient(lprint_client_t *client, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintLogJob(lprint_job_t *job, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintLogPrinter(lprint_printer_t *printer, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		*lprintProcessClient(lprint_client_t *client);
extern int		lprintProcessHTTP(lprint_client_t *client);
extern int		lprinterProcessIPP(lprint_client_t *client);
extern void		*lprintProcessJob(lprint_job_t *job);
extern int		lprintRegisterDNSSD(lprint_printer_t *printer, const char *subtypes);
extern int		lprintRespondHTTP(lprint_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length);
extern void		lprintRespondIPP(lprint_client_t *client, ipp_status_t status, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintRespondUnsupported(lprint_client_t *client, ipp_attribute_t *attr);
extern void		lprintRunSystem(lprint_system_t *system);

#endif // !_LPRINT_H_
