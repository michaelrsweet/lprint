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
// Functions...
//

extern bool	lprintDriverCallback(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern bool	lprintDYMO(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern bool	lprintZPL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);

// Future planned drivers that are not yet implemented
//extern bool	lprintCPCL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintEPL1(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintEPL2(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintFGL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);
//extern bool	lprintPCL(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);


#endif // !LPRINT_H
