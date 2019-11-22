//
// ??? for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
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

static void		copy_attributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy);
static void		copy_job_attributes(lprint_client_t *client, lprint_job_t *job, cups_array_t *ra);
static int		filter_cb(lprint_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);

static void		ipp_cancel_job(lprint_client_t *client);
static void		ipp_close_job(lprint_client_t *client);
static void		ipp_create_job(lprint_client_t *client);
static void		ipp_get_job_attributes(lprint_client_t *client);
static void		ipp_get_jobs(lprint_client_t *client);
static void		ipp_get_printer_attributes(lprint_client_t *client);
static void		ipp_identify_printer(lprint_client_t *client);
static void		ipp_print_job(lprint_client_t *client);
static void		ipp_print_uri(lprint_client_t *client);
static void		ipp_send_document(lprint_client_t *client);
static void		ipp_send_uri(lprint_client_t *client);
static void		ipp_validate_job(lprint_client_t *client);
