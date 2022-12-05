//
// Header file for LPrint, a Label Printer Application
//
// Copyright © 2019-2021 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef LPRINT_H
#  define LPRINT_H

//
// Include necessary headers...
//

#  include "config.h"
#  include <pappl/pappl.h>
#  include <math.h>


//
// Debug macro...
//

#  ifdef DEBUG
#    define LPRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#  else
#    define LPRINT_DEBUG(...)
#  endif // DEBUG


//
// Function annotations...
//

#  if defined(__GNUC__) || defined(__has_extension)
#    define LPRINT_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#    define LPRINT_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define LPRINT_NORETURN	__attribute__ ((noreturn))
#  else
#    define LPRINT_FORMAT(a,b)
#    define LPRINT_NONNULL(...)
#    define LPRINT_NORETURN
#  endif // __GNUC__ || __has_extension


//
// Constants...
//

#  define LPRINT_TESTPAGE_MIMETYPE	"application/vnd.lprint-test"
#  define LPRINT_TESTPAGE_HEADER	"T*E*S*T*P*A*G*E*"

#  define LPRINT_EPL2_MIMETYPE		"application/vnd.eltron-epl"
#  define LPRINT_ZPL_MIMETYPE		"application/vnd.zebra-zpl"


//
// Functions...
//

extern bool	lprintDYMO(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern bool	lprintEPL2(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern bool	lprintTestFilterCB(pappl_job_t *job, pappl_device_t *device, void *data);
extern const char *lprintTestPageCB(pappl_printer_t *printer, char *buffer, size_t bufsize);
extern bool	lprintZPL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern void	lprintZPLQueryDriver(pappl_system_t *system, const char *device_uri, char *name, size_t namesize);

// Future planned drivers that are not yet implemented
//extern bool	lprintCPCL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintEPL1(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintFGL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintPCL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);


#endif // !LPRINT_H
