//
// IPP processing for LPrint, a Label Printer Utility
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


//
// Local type...
//

typedef struct lprint_attr_s		// Input attribute structure
{
  const char	*name;			// Attribute name
  ipp_tag_t	value_tag;		// Value tag
  int		max_count;		// Max number of values
} lprint_attr_t;


//
// Local functions...
//

static void		copy_job_attributes(lprint_client_t *client, lprint_job_t *job, cups_array_t *ra);
static void		copy_printer_attributes(lprint_client_t *client, lprint_printer_t *printer, cups_array_t *ra);
static void		copy_printer_state(ipp_t *ipp, lprint_printer_t *printer, cups_array_t *ra);
static int		filter_cb(lprint_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);
static void		finish_document_data(lprint_client_t *client, lprint_job_t *job);

static void		ipp_cancel_job(lprint_client_t *client);
static void		ipp_cancel_jobs(lprint_client_t *client);
static void		ipp_close_job(lprint_client_t *client);
static void		ipp_create_job(lprint_client_t *client);
static void		ipp_create_printer(lprint_client_t *client);
static void		ipp_delete_printer(lprint_client_t *client);
static void		ipp_get_job_attributes(lprint_client_t *client);
static void		ipp_get_jobs(lprint_client_t *client);
static void		ipp_get_printer_attributes(lprint_client_t *client);
static void		ipp_get_printers(lprint_client_t *client);
static void		ipp_get_system_attributes(lprint_client_t *client);
static void		ipp_identify_printer(lprint_client_t *client);
static void		ipp_print_job(lprint_client_t *client);
static void		ipp_send_document(lprint_client_t *client);
static void		ipp_set_printer_attributes(lprint_client_t *client);
static void		ipp_set_system_attributes(lprint_client_t *client);
static void		ipp_shutdown_all_printers(lprint_client_t *client);
static void		ipp_validate_job(lprint_client_t *client);

static int		is_domain_connection(lprint_client_t *client);

static void		respond_unsupported(lprint_client_t *client, ipp_attribute_t *attr);

static int		set_printer_attributes(lprint_client_t *client, lprint_printer_t *printer);

static int		valid_doc_attributes(lprint_client_t *client);
static int		valid_job_attributes(lprint_client_t *client);


//
// 'lprintCopyAttributes()' - Copy attributes from one message to another.
//

void
lprintCopyAttributes(
    ipp_t        *to,			// I - Destination request
    ipp_t        *from,			// I - Source request
    cups_array_t *ra,			// I - Requested attributes
    ipp_tag_t    group_tag,		// I - Group to copy
    int          quickcopy)		// I - Do a quick copy?
{
  lprint_filter_t	filter;		// Filter data


  filter.ra        = ra;
  filter.group_tag = group_tag;

  ippCopyAttributes(to, from, quickcopy, (ipp_copycb_t)filter_cb, &filter);
}


//
// 'lprintProcessIPP()' - Process an IPP request.
//

int					// O - 1 on success, 0 on error
lprintProcessIPP(
    lprint_client_t *client)		// I - Client
{
  ipp_tag_t		group;		// Current group tag
  ipp_attribute_t	*attr;		// Current attribute
  ipp_attribute_t	*charset;	// Character set attribute
  ipp_attribute_t	*language;	// Language attribute
  ipp_attribute_t	*uri;		// Printer URI attribute
  int			major, minor;	// Version number
  ipp_op_t		op;		// Operation code
  const char		*name;		// Name of attribute


  lprintLogAttributes(client, "Request", client->request, 1);

  // First build an empty response message for this request...
  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

  // Then validate the request header and required attributes...
  major = ippGetVersion(client->request, &minor);
  op    = ippGetOperation(client->request);

  if (major < 1 || major > 2)
  {
    // Return an error, since we only support IPP 1.x and 2.x.
    lprintRespondIPP(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED, "Bad request version number %d.%d.", major, minor);
  }
  else if (ippGetRequestId(client->request) <= 0)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.", ippGetRequestId(client->request));
  }
  else if (!ippFirstAttribute(client->request))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No attributes in request.");
  }
  else
  {
    // Make sure that the attributes are provided in the correct order and
    // don't repeat groups...
    for (attr = ippFirstAttribute(client->request), group = ippGetGroupTag(attr); attr; attr = ippNextAttribute(client->request))
    {
      if (ippGetGroupTag(attr) < group && ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
        // Out of order; return an error...
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Attribute groups are out of order (%x < %x).", ippGetGroupTag(attr), group);
	break;
      }
      else
	group = ippGetGroupTag(attr);
    }

    if (!attr)
    {
      // Then make sure that the first three attributes are:
      //
      //   attributes-charset
      //   attributes-natural-language
      //   system-uri/printer-uri/job-uri
      attr = ippFirstAttribute(client->request);
      name = ippGetName(attr);
      if (attr && name && !strcmp(name, "attributes-charset") && ippGetValueTag(attr) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      attr = ippNextAttribute(client->request);
      name = ippGetName(attr);

      if (attr && name && !strcmp(name, "attributes-natural-language") && ippGetValueTag(attr) == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(client->request, "system-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      client->printer = NULL;
      client->job     = NULL;

      if (charset && strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") && strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
      {
        // Bad character set...
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unsupported character set \"%s\".", ippGetString(charset, 0, NULL));
      }
      else if (!charset || !language || (!uri && op != IPP_OP_CUPS_GET_DEFAULT && op != IPP_OP_CUPS_GET_PRINTERS))
      {
        // Return an error, since attributes-charset,
	// attributes-natural-language, and system/printer/job-uri are required
	// for all operations.
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required attributes.");
      }
      else
      {
        if (uri)
        {
	  char	scheme[32],		// URI scheme
		userpass[32],		// Username/password in URI
		host[256],		// Host name in URI
		resource[256],		// Resource path in URI
		*resptr;		// Pointer into resource
	  int	port,			// Port number in URI
		job_id;			// Job ID

	  name = ippGetName(uri);

	  if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
	  {
	    lprintRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
	  }
	  else if (!strcmp(name, "system-uri"))
	  {
	    if (strcmp(resource, "/ipp/system"))
	    {
	      lprintRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
	    }
	    else
	      client->printer = lprintFindPrinter(client->system, NULL, ippGetInteger(ippFindAttribute(client->request, "printer-id", IPP_TAG_INTEGER), 0));
	  }
	  else if ((client->printer = lprintFindPrinter(client->system, resource, 0)) != NULL)
	  {
	    if (!strcmp(name, "job-uri") && (resptr = strrchr(resource, '/')) != NULL)
	      job_id = atoi(resptr + 1);
	    else
	      job_id = ippGetInteger(ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER), 0);

	    if (job_id)
	      client->job = lprintFindJob(client->printer, job_id);
	  }
	  else
	  {
	    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "%s %s not found.", name, ippGetString(uri, 0, NULL));
	  }
        }

	if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
	{
	  if (client->printer)
	  {
	    // Try processing the printer operation...
	    switch (ippGetOperation(client->request))
	    {
	      case IPP_OP_PRINT_JOB :
		  ipp_print_job(client);
		  break;

	      case IPP_OP_VALIDATE_JOB :
		  ipp_validate_job(client);
		  break;

	      case IPP_OP_CREATE_JOB :
		  ipp_create_job(client);
		  break;

	      case IPP_OP_SEND_DOCUMENT :
		  ipp_send_document(client);
		  break;

	      case IPP_OP_CANCEL_JOB :
	      case IPP_OP_CANCEL_CURRENT_JOB :
		  ipp_cancel_job(client);
		  break;

	      case IPP_OP_CANCEL_JOBS :
	      case IPP_OP_CANCEL_MY_JOBS :
		  ipp_cancel_jobs(client);
		  break;

	      case IPP_OP_GET_JOB_ATTRIBUTES :
		  ipp_get_job_attributes(client);
		  break;

	      case IPP_OP_GET_JOBS :
		  ipp_get_jobs(client);
		  break;

	      case IPP_OP_GET_PRINTER_ATTRIBUTES :
		  ipp_get_printer_attributes(client);
		  break;

	      case IPP_OP_SET_PRINTER_ATTRIBUTES :
		  ipp_set_printer_attributes(client);
		  break;

	      case IPP_OP_CLOSE_JOB :
		  ipp_close_job(client);
		  break;

	      case IPP_OP_IDENTIFY_PRINTER :
		  ipp_identify_printer(client);
		  break;

	      default :
		  lprintRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
		  break;
	    }
	  }
	  else
	  {
	    // Try processing the system operation...
	    switch (ippGetOperation(client->request))
	    {
	      case IPP_OP_CREATE_PRINTER :
	          ipp_create_printer(client);
	          break;

	      case IPP_OP_DELETE_PRINTER :
	          ipp_delete_printer(client);
	          break;

              case IPP_OP_GET_PRINTERS :
              case IPP_OP_CUPS_GET_PRINTERS :
		  ipp_get_printers(client);
		  break;

	      case IPP_OP_GET_PRINTER_ATTRIBUTES :
	      case IPP_OP_CUPS_GET_DEFAULT :
                  client->printer = lprintFindPrinter(client->system, NULL, client->system->default_printer);
		  ipp_get_printer_attributes(client);
		  break;

	      case IPP_OP_GET_SYSTEM_ATTRIBUTES :
		  ipp_get_system_attributes(client);
		  break;

	      case IPP_OP_SET_SYSTEM_ATTRIBUTES :
		  ipp_set_system_attributes(client);
		  break;

	      case IPP_OP_SHUTDOWN_ALL_PRINTERS :
	          ipp_shutdown_all_printers(client);
	          break;

	      default :
		  lprintRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
		  break;
	    }
	  }
	}
      }
    }
  }

  // Send the HTTP header and return...
  if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
    httpFlush(client->http);		// Flush trailing (junk) data

  return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "application/ipp", ippLength(client->response)));
}


//
// 'lprintRespondIPP()' - Send an IPP response.
//

void
lprintRespondIPP(
    lprint_client_t *client,		// I - Client
    ipp_status_t    status,		// I - status-code
    const char      *message,		// I - printf-style status-message
    ...)				// I - Additional args as needed
{
  const char	*formatted = NULL;	// Formatted message


  ippSetStatusCode(client->response, status);

  if (message)
  {
    va_list		ap;		// Pointer to additional args
    ipp_attribute_t	*attr;		// New status-message attribute

    va_start(ap, message);
    if ((attr = ippFindAttribute(client->response, "status-message", IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT, "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "%s %s (%s)", ippOpString(client->operation_id), ippErrorString(status), formatted);
  else
    lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "%s %s", ippOpString(client->operation_id), ippErrorString(status));
}


//
// 'copy_job_attrs()' - Copy job attributes to the response.
//

static void
copy_job_attributes(
    lprint_client_t *client,		// I - Client
    lprint_job_t    *job,		// I - Job
    cups_array_t  *ra)			// I - requested-attributes
{
  lprintCopyAttributes(client->response, job->attrs, ra, IPP_TAG_JOB, 0);

  if (!ra || cupsArrayFind(ra, "date-time-at-completed"))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (!ra || cupsArrayFind(ra, "date-time-at-processing"))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (!ra || cupsArrayFind(ra, "job-impressions"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (!ra || cupsArrayFind(ra, "job-impressions-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "job-state"))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", (int)job->state);

  if (!ra || cupsArrayFind(ra, "job-state-message"))
  {
    if (job->message)
    {
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_TEXT, "job-state-message", NULL, job->message);
    }
    else
    {
      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job pending.");
	    break;

	case IPP_JSTATE_HELD :
	    if (job->fd >= 0)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job incoming.");
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job held.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job created.");
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->cancel)
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceling.");
	    else
	      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job printing.");
	    break;

	case IPP_JSTATE_STOPPED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job stopped.");
	    break;

	case IPP_JSTATE_CANCELED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job canceled.");
	    break;

	case IPP_JSTATE_ABORTED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job aborted.");
	    break;

	case IPP_JSTATE_COMPLETED :
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, "Job completed.");
	    break;
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
  {
    switch (job->state)
    {
      case IPP_JSTATE_PENDING :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "none");
	  break;

      case IPP_JSTATE_HELD :
          if (job->fd >= 0)
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-incoming");
	  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-hold-until-specified");
          else
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-data-insufficient");
	  break;

      case IPP_JSTATE_PROCESSING :
	  if (job->cancel)
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "processing-to-stop-point");
	  else
	    ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-printing");
	  break;

      case IPP_JSTATE_STOPPED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-stopped");
	  break;

      case IPP_JSTATE_CANCELED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-canceled-by-user");
	  break;

      case IPP_JSTATE_ABORTED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "aborted-by-system");
	  break;

      case IPP_JSTATE_COMPLETED :
	  ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-state-reasons", NULL, "job-completed-successfully");
	  break;
    }
  }

  if (!ra || cupsArrayFind(ra, "time-at-completed"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (!ra || cupsArrayFind(ra, "time-at-processing"))
    ippAddInteger(client->response, IPP_TAG_JOB, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
}


//
// 'copy_printer_attributes()' - Copy printer attributes to a response...
//

static void
copy_printer_attributes(
    lprint_client_t  *client,		// I - Client
    lprint_printer_t *printer,		// I - Printer
    cups_array_t     *ra)		// I - Requested attributes
{
  lprintCopyAttributes(client->response, printer->attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  lprintCopyAttributes(client->response, printer->driver->attrs, ra, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
  copy_printer_state(client->response, printer, ra);

  if ((!ra || cupsArrayFind(ra, "media-col-default")) && printer->driver->default_media[0])
  {
    ipp_t *col = lprintCreateMediaCol(printer->driver->default_media, NULL, NULL, printer->driver->left_right, printer->driver->bottom_top);
					// Collection value

    ippAddCollection(client->response, IPP_TAG_PRINTER, "media-col", col);
    ippDelete(col);
  }

  if (!ra || cupsArrayFind(ra, "media-col-ready"))
  {
    int			i, j,		// Looping vars
			count;		// Number of values
    ipp_t		*col;		// Collection value
    ipp_attribute_t	*attr;		// media-col-ready attribute

    for (i = 0, count = 0; i < printer->driver->num_source; i ++)
    {
      if (printer->driver->ready_media[i][0])
        count ++;
    }

    if (count > 0)
    {
      attr = ippAddCollections(client->response, IPP_TAG_PRINTER, "media-col-ready", count, NULL);

      for (i = 0, j = 0; i < printer->driver->num_source && j < count; i ++)
      {
	if (printer->driver->ready_media[i][0])
	{
	  col = lprintCreateMediaCol(printer->driver->ready_media[i], printer->driver->source[i], NULL, printer->driver->left_right, printer->driver->bottom_top);
          ippSetCollection(client->response, &attr, j ++, col);
          ippDelete(col);
	}
      }
    }
  }

  if ((!ra || cupsArrayFind(ra, "media-default")) && printer->driver->default_media[0])
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-default", NULL, printer->driver->default_media);

  if (!ra || cupsArrayFind(ra, "media-ready"))
  {
    int			i, j,		// Looping vars
			count;		// Number of values
    ipp_attribute_t	*attr;		// media-col-ready attribute

    for (i = 0, count = 0; i < printer->driver->num_source; i ++)
    {
      if (printer->driver->ready_media[i][0])
        count ++;
    }

    if (count > 0)
    {
      attr = ippAddStrings(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", count, NULL, NULL);

      for (i = 0, j = 0; i < printer->driver->num_source && j < count; i ++)
      {
	if (printer->driver->ready_media[i][0])
	  ippSetString(client->response, &attr, j ++, printer->driver->ready_media[i]);
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));

  if (!ra || cupsArrayFind(ra, "printer-geo-location"))
  {
    if (printer->geo_location)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-geo-location", NULL, printer->geo_location);
    else
      ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_UNKNOWN, "printer-geo-location");
  }

  if (!ra || cupsArrayFind(ra, "printer-is-accepting-jobs"))
    ippAddBoolean(client->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs", !printer->system->shutdown_time);

  if (!ra || cupsArrayFind(ra, "printer-location"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL, printer->location ? printer->location : "");

  if (!ra || cupsArrayFind(ra, "printer-organization"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organization", NULL, printer->organization ? printer->organization : "");

  if (!ra || cupsArrayFind(ra, "printer-organizational-unit"))
    ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-organizational-unit", NULL, printer->org_unit ? printer->org_unit : "");

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", cupsArrayCount(printer->active_jobs));
}


//
// 'copy_printer_state()' - Copy the printer-state-xxx attributes.
//

static void
copy_printer_state(
    ipp_t            *ipp,		// I - IPP message
    lprint_printer_t *printer,		// I - Printer
    cups_array_t     *ra)		// I - Requested attributes
{
  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", (int)printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == LPRINT_PREASON_NONE)
    {
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
    }
    else
    {
      ipp_attribute_t	*attr = NULL;		// printer-state-reasons
      lprint_preason_t	bit;			// Reason bit
      int		i;			// Looping var
      char		reason[32];		// Reason string

      for (i = 0, bit = 1; i < (int)(sizeof(lprint_preason_strings) / sizeof(lprint_preason_strings[0])); i ++, bit *= 2)
      {
        if (printer->state_reasons & bit)
	{
	  snprintf(reason, sizeof(reason), "%s-%s", lprint_preason_strings[i], printer->state == IPP_PSTATE_IDLE ? "report" : printer->state == IPP_PSTATE_PROCESSING ? "warning" : "error");
	  if (attr)
	    ippSetString(ipp, &attr, ippGetCount(attr), reason);
	  else
	    attr = ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-state-reasons", NULL, reason);
	}
      }
    }
  }
}


//
// 'filter_cb()' - Filter printer attributes based on the requested array.
//

static int				// O - 1 to copy, 0 to ignore
filter_cb(lprint_filter_t   *filter,	// I - Filter parameters
          ipp_t           *dst,		// I - Destination (unused)
	  ipp_attribute_t *attr)	// I - Source attribute
{
  // Filter attributes as needed...
#ifndef _WIN32 /* Avoid MS compiler bug */
  (void)dst;
#endif /* !_WIN32 */

  ipp_tag_t group = ippGetGroupTag(attr);
  const char *name = ippGetName(attr);

  if ((filter->group_tag != IPP_TAG_ZERO && group != filter->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(filter->ra, (void *)name)))
    return (0);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}


//
// 'finish_document()' - Finish receiving a document file and start processing.
//

static void
finish_document_data(
    lprint_client_t *client,		// I - Client
    lprint_job_t    *job)		// I - Job
{
  char			filename[1024],	// Filename buffer
			buffer[4096];	// Copy buffer
  ssize_t		bytes;		// Bytes read
  cups_array_t		*ra;		// Attributes to send in response
  pthread_t		t;              // Thread


  // Create a file for the request data...
  //
  // TODO: Update code to support piping large raster data to the print command.
  if ((job->fd = lprintCreateJobFile(job, filename, sizeof(filename), client->system->directory, NULL)) < 0)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

    goto abort_job;
  }

  lprintLogJob(job, LPRINT_LOGLEVEL_DEBUG, "Created job file \"%s\", format \"%s\".", filename, job->format);

  while ((bytes = httpRead2(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		// Write error

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

      goto abort_job;
    }
  }

  if (bytes < 0)
  {
    // Got an error while reading the print data, so abort this job.
    close(job->fd);
    job->fd = -1;

    unlink(filename);

    lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to read print file.");

    goto abort_job;
  }

  if (close(job->fd))
  {
    int error = errno;			// Write error

    job->fd = -1;

    unlink(filename);

    lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));

    goto abort_job;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

  // Process the job...
  if (pthread_create(&t, NULL, (void *(*)(void *))lprintProcessJob, job))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to process job.");
    goto abort_job;
  }
  else
    pthread_detach(t);

  // Return the job info...
  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
  return;

  // If we get here we had to abort the job...
  abort_job:

  job->state     = IPP_JSTATE_ABORTED;
  job->completed = time(NULL);

  pthread_rwlock_wrlock(&client->printer->rwlock);

  cupsArrayRemove(client->printer->active_jobs, job);
  cupsArrayAdd(client->printer->completed_jobs, job);

  if (!client->system->clean_time)
    client->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&client->printer->rwlock);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_cancel_job()' - Cancel a job.
//

static void
ipp_cancel_job(lprint_client_t *client)	// I - Client
{
  lprint_job_t	*job;			// Job information


  // Get the job...
  if (ippGetOperation(client->request) == IPP_OP_CANCEL_CURRENT_JOB)
    job = client->printer->processing_job;
  else
    job = client->job;

  if (!job)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already canceled - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already aborted - can\'t cancel.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already completed - can\'t cancel.", job->id);
        break;

    default :
        // Cancel the job...
	pthread_rwlock_wrlock(&client->printer->rwlock);

	if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
	{
          job->cancel = 1;
	}
	else
	{
	  job->state     = IPP_JSTATE_CANCELED;
	  job->completed = time(NULL);

	  cupsArrayRemove(client->printer->active_jobs, job);
	  cupsArrayAdd(client->printer->completed_jobs, job);
	}

	pthread_rwlock_unlock(&client->printer->rwlock);

	lprintRespondIPP(client, IPP_STATUS_OK, NULL);

        if (!client->system->clean_time)
          client->system->clean_time = time(NULL) + 60;
        break;
  }
}


//
// 'ipp_cancel_jobs()' - Cancel all jobs.
//

static void
ipp_cancel_jobs(lprint_client_t *client)// I - Client
{
  lprint_job_t	*job;			// Job information


  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  // Loop through all jobs and cancel them...
  pthread_rwlock_wrlock(&client->printer->rwlock);

  for (job = (lprint_job_t *)cupsArrayFirst(client->printer->jobs); job; job = (lprint_job_t *)cupsArrayNext(client->printer->jobs))
  {
    // Cancel this job...
    if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
    {
      job->cancel = 1;
    }
    else
    {
      job->state     = IPP_JSTATE_CANCELED;
      job->completed = time(NULL);

      cupsArrayRemove(client->printer->active_jobs, job);
      cupsArrayAdd(client->printer->completed_jobs, job);
    }
  }

  pthread_rwlock_unlock(&client->printer->rwlock);

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  if (!client->system->clean_time)
    client->system->clean_time = time(NULL) + 60;
}


//
// 'ipp_close_job()' - Close an open job.
//

static void
ipp_close_job(lprint_client_t *client)	// I - Client
{
  lprint_job_t	*job = client->job;	// Job information


  // Get the job...
  if (!job)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  // See if the job is already completed, canceled, or aborted; if so,
  // we can't cancel...
  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is canceled - can\'t close.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is aborted - can\'t close.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is completed - can\'t close.", job->id);
        break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already closed.", job->id);
        break;

    default :
	lprintRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


//
// 'ipp_create_job()' - Create a job object.
//

static void
ipp_create_job(lprint_client_t *client)	// I - Client
{
  lprint_job_t		*job;		// New job
  cups_array_t		*ra;		// Attributes to send in response


  // Validate print job attributes...
  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  // Do we have a file to print?
  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unexpected document data following request.");
    return;
  }

  // Create the job...
  if ((job = lprintCreateJob(client)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Return the job info...
  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_create_printer()' - Create a printer.
//

static void
ipp_create_printer(
    lprint_client_t *client)		// I - Client
{
  const char	*printer_name,		// Printer name
		*device_uri,		// Device URI
		*driver_name,		// Name of driver
		*location,		// printer-location
		*geo_location,		// printer-gwo-location
		*organization,		// printer-organization
		*org_unit;		// printer-organizational-unit
  ipp_attribute_t *attr;		// Current attribute
  char		resource[256];		// Resource path
  lprint_printer_t *printer;		// Printer
  cups_array_t	*ra;			// Requested attributes


  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  // Get required attributes...
  if ((attr = ippFindAttribute(client->request, "printer-service-type", IPP_TAG_ZERO)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'printer-service-type' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || strcmp(ippGetString(attr, 0, NULL), "print"))
  {
    respond_unsupported(client, attr);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-name", IPP_TAG_ZERO)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'printer-name' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG) || ippGetCount(attr) != 1 || strlen(ippGetString(attr, 0, NULL)) > 127)
  {
    respond_unsupported(client, attr);
    return;
  }
  else
    printer_name = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "device-uri", IPP_TAG_ZERO)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'device-uri' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_URI || ippGetCount(attr) != 1)
  {
    respond_unsupported(client, attr);
    return;
  }
  else
    device_uri = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "lprint-driver", IPP_TAG_ZERO)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'lprint-driver' attribute in request.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1)
  {
    respond_unsupported(client, attr);
    return;
  }
  else
    driver_name = ippGetString(attr, 0, NULL);

  location     = ippGetString(ippFindAttribute(client->request, "printer-location", IPP_TAG_TEXT), 0, NULL);
  geo_location = ippGetString(ippFindAttribute(client->request, "printer-geo-location", IPP_TAG_TEXT), 0, NULL);
  organization = ippGetString(ippFindAttribute(client->request, "printer-organization", IPP_TAG_TEXT), 0, NULL);
  org_unit     = ippGetString(ippFindAttribute(client->request, "printer-organizational-unit", IPP_TAG_TEXT), 0, NULL);

  // See if the printer already exists...
  snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

  if (lprintFindPrinter(client->system, resource, 0))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Printer name '%s' already exists.", printer_name);
    return;
  }

  // Create the printer...
  if ((printer = lprintCreatePrinter(client->system, 0, printer_name, driver_name, device_uri, location, geo_location, organization, org_unit)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Printer name '%s' already exists.", printer_name);
    return;
  }

  if (!set_printer_attributes(client, printer))
    return;

  // Return the printer
  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);
  cupsArrayAdd(ra, "printer-id");
  cupsArrayAdd(ra, "printer-is-accepting-jobs");
  cupsArrayAdd(ra, "printer-state");
  cupsArrayAdd(ra, "printer-state-reasons");
  cupsArrayAdd(ra, "printer-uuid");
  cupsArrayAdd(ra, "printer-xri-supported");

  copy_printer_attributes(client, printer, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_delete_printer()' - Delete a printer.
//

static void
ipp_delete_printer(
    lprint_client_t *client)		// I - Client
{
  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  if (!client->printer)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Printer not found.");
    return;
  }

  if (!client->printer->processing_job)
    lprintDeletePrinter(client->printer);
  else
    client->printer->deleted = 1;

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_get_job_attributes()' - Get the attributes for a job object.
//

static void
ipp_get_job_attributes(
    lprint_client_t *client)		// I - Client
{
  lprint_job_t	*job = client->job;	// Job information
  cups_array_t	*ra;			// requested-attributes


  if (!job)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_job_attributes(client, job, ra);
  cupsArrayDelete(ra);
}


//
// 'ipp_get_jobs()' - Get a list of job objects.
//

static void
ipp_get_jobs(lprint_client_t *client)	// I - Client
{
  ipp_attribute_t	*attr;		// Current attribute
  const char		*which_jobs = NULL;
					// which-jobs values
  int			job_comparison;	// Job comparison
  ipp_jstate_t		job_state;	// job-state value
  int			first_job_id,	// First job ID
			limit,		// Maximum number of jobs to return
			count;		// Number of jobs that match
  const char		*username;	// Username
  cups_array_t		*list;		// Jobs list
  lprint_job_t		*job;		// Current job pointer
  cups_array_t		*ra;		// Requested attributes array


  // See if the "which-jobs" attribute have been specified...
  if ((attr = ippFindAttribute(client->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Get-Jobs \"which-jobs\"='%s'", which_jobs);
  }

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
    list           = client->printer->active_jobs;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
    list           = client->printer->completed_jobs;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
    list           = client->printer->jobs;
  }
  else
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "The \"which-jobs\" value '%s' is not supported.", which_jobs);
    ippAddString(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD, "which-jobs", NULL, which_jobs);
    return;
  }

  // See if they want to limit the number of jobs reported...
  if ((attr = ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Get-Jobs \"limit\"='%d'", limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id", IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = ippGetInteger(attr, 0);

    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Get-Jobs \"first-job-id\"='%d'", first_job_id);
  }
  else
    first_job_id = 1;

  // See if we only want to see jobs for a specific user...
  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Get-Jobs \"my-jobs\"='%s'", my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) == NULL)
      {
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Need \"requesting-user-name\" with \"my-jobs\".");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Get-Jobs \"requesting-user-name\"='%s'", username);
    }
  }

  // OK, build a list of jobs for this printer...
  ra = ippCreateRequestedArray(client->request);

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(client->printer->rwlock));

  for (count = 0, job = (lprint_job_t *)cupsArrayFirst(list); (limit <= 0 || count < limit) && job; job = (lprint_job_t *)cupsArrayNext(list))
  {
    // Filter out jobs that don't match...
    if (job->printer != client->printer)
      continue;

    if ((job_comparison < 0 && job->state > job_state) || (job_comparison == 0 && job->state != job_state) || (job_comparison > 0 && job->state < job_state) || job->id < first_job_id || (username && job->username && strcasecmp(username, job->username)))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;
    copy_job_attributes(client, job, ra);
  }

  cupsArrayDelete(ra);

  pthread_rwlock_unlock(&(client->printer->rwlock));
}


//
// 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
//

static void
ipp_get_printer_attributes(
    lprint_client_t *client)		// I - Client
{
  cups_array_t		*ra;		// Requested attributes array
  lprint_printer_t	*printer;	// Printer


  // TODO: Update status attributes as needed (querying printer, etc.)

  // Send the attributes...
  ra      = ippCreateRequestedArray(client->request);
  printer = client->printer;

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&(printer->rwlock));

  copy_printer_attributes(client, printer, ra);

  pthread_rwlock_unlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


//
// 'ipp_get_printers()' - Get printers.
//

static void
ipp_get_printers(
    lprint_client_t *client)		// I - Client
{
  lprint_system_t	*system = client->system;
					// System
  cups_array_t		*ra;		// Requested attributes array
  int			i,		// Looping var
			limit;		// Maximum number to return
  lprint_printer_t	*printer;	// Current printer


  // Get request attributes...
  limit = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  ra    = ippCreateRequestedArray(client->request);

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  for (i = 0, printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; i ++, printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    if (limit && i >= limit)
      break;

    if (i)
      ippAddSeparator(client->response);

    pthread_rwlock_rdlock(&printer->rwlock);
    copy_printer_attributes(client, printer, ra);
    pthread_rwlock_unlock(&printer->rwlock);
  }

  pthread_rwlock_unlock(&system->rwlock);

  cupsArrayDelete(ra);
}


//
// 'ipp_get_system_attributes()' - Get system attributes.
//

static void
ipp_get_system_attributes(
    lprint_client_t *client)		// I - Client
{
  lprint_system_t	*system = client->system;
					// System
  cups_array_t		*ra;		// Requested attributes array
  int			i;		// Looping var
  lprint_printer_t	*printer;	// Current printer
  ipp_attribute_t	*attr;		// Current attribute
  ipp_t			*col;		// configured-printers value
  time_t		config_time = 0;// system-config-change-[date-]time value
  time_t		state_time = 0;	// system-state-change-[date-]time value


  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  ra = ippCreateRequestedArray(client->request);

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&system->rwlock);

  if (!ra || cupsArrayFind(ra, "system-config-change-date-time") || cupsArrayFind(ra, "system-config-change-time"))
  {
    for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
    {
      if (config_time < printer->config_time)
        config_time = printer->config_time;
    }

    if (!ra || cupsArrayFind(ra, "system-config-change-date-time"))
      ippAddDate(client->response, IPP_TAG_SYSTEM, "system-config-change-date-time", ippTimeToDate(config_time));

    if (!ra || cupsArrayFind(ra, "system-config-change-time"))
      ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-config-change-time", (int)(config_time - system->start_time));
  }

  if (!ra || cupsArrayFind(ra, "system-configured-printers"))
  {
    attr = ippAddCollections(client->response, IPP_TAG_SYSTEM, "system-configured-printers", cupsArrayCount(system->printers), NULL);

    for (i = 0, printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; i ++, printer = (lprint_printer_t *)cupsArrayNext(system->printers))
    {
      col = ippNew();

      pthread_rwlock_rdlock(&printer->rwlock);

      ippAddInteger(col, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "printer-id", printer->printer_id);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-info", NULL, printer->printer_name);
      ippAddBoolean(col, IPP_TAG_SYSTEM, "printer-is-accepting-jobs", 1);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_TEXT, "printer-name", NULL, printer->printer_name);
      ippAddString(col, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "printer-service-type", NULL, "print");
      copy_printer_state(col, printer, NULL);
      ippCopyAttribute(col, printer->xri_supported, 0);

      pthread_rwlock_unlock(&printer->rwlock);

      ippSetCollection(client->response, &attr, i, col);
      ippDelete(col);
    }
  }

  if (!ra || cupsArrayFind(ra, "system-current-time"))
    ippAddDate(client->response, IPP_TAG_SYSTEM, "system-current-time", ippTimeToDate(time(NULL)));

  if (!ra || cupsArrayFind(ra, "system-default-printer-id"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", system->default_printer);

  if (!ra || cupsArrayFind(ra, "system-state"))
  {
    int	state = IPP_PSTATE_IDLE;	// System state

    for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
    {
      if (printer->state == IPP_PSTATE_PROCESSING)
      {
        state = IPP_PSTATE_PROCESSING;
        break;
      }
    }

    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_ENUM, "system-state", state);
  }

  if (!ra || cupsArrayFind(ra, "system-state-change-date-time") || cupsArrayFind(ra, "system-state-change-time"))
  {
    for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
    {
      if (state_time < printer->state_time)
        state_time = printer->state_time;
    }

    if (!ra || cupsArrayFind(ra, "system-state-change-date-time"))
      ippAddDate(client->response, IPP_TAG_SYSTEM, "system-state-change-date-time", ippTimeToDate(state_time));

    if (!ra || cupsArrayFind(ra, "system-state-change-time"))
      ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-state-change-time", (int)(state_time - system->start_time));
  }

  if (!ra || cupsArrayFind(ra, "system-state-reasons"))
  {
    lprint_preason_t	state_reasons = LPRINT_PREASON_NONE;

    for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
    {
      state_reasons |= printer->state_reasons;
    }

    if (state_reasons == LPRINT_PREASON_NONE)
    {
      ippAddString(client->response, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-state-reasons", NULL, "none");
    }
    else
    {
      ipp_attribute_t	*attr = NULL;		// printer-state-reasons
      lprint_preason_t	bit;			// Reason bit
      int		i;			// Looping var

      for (i = 0, bit = 1; i < (int)(sizeof(lprint_preason_strings) / sizeof(lprint_preason_strings[0])); i ++, bit *= 2)
      {
        if (state_reasons & bit)
	{
	  if (attr)
	    ippSetString(client->response, &attr, ippGetCount(attr), lprint_preason_strings[i]);
	  else
	    attr = ippAddString(client->response, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "system-state-reasons", NULL, lprint_preason_strings[i]);
	}
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "system-up-time"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - system->start_time));

  pthread_rwlock_unlock(&system->rwlock);

  cupsArrayDelete(ra);
}


//
// 'ipp_identify_printer()' - Beep or display a message.
//

static void
ipp_identify_printer(
    lprint_client_t *client)		// I - Client
{
  ipp_attribute_t	*actions,	// identify-actions
			*message;	// message


  actions = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD);
  message = ippFindAttribute(client->request, "message", IPP_TAG_TEXT);

  // TODO: FIX ME
  if (!actions || ippContainsString(actions, "sound"))
  {
    putchar(0x07);
    fflush(stdout);
  }

  if (ippContainsString(actions, "display"))
    printf("IDENTIFY from %s: %s\n", client->hostname, message ? ippGetString(message, 0, NULL) : "No message supplied");

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_print_job()' - Create a job object with an attached document.
//

static void
ipp_print_job(lprint_client_t *client)	// I - Client
{
  lprint_job_t		*job;		// New job


  // Validate print job attributes...
  if (!valid_job_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  // Do we have a file to print?
  if (httpGetState(client->http) == HTTP_STATE_POST_SEND)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

  // Create the job...
  if ((job = lprintCreateJob(client)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BUSY, "Currently printing another job.");
    return;
  }

  // Then finish getting the document data and process things...
  finish_document_data(client, job);
}


//
// 'ipp_send_document()' - Add an attached document to a job object created with
//                         Create-Job.
//

static void
ipp_send_document(
    lprint_client_t *client)		// I - Client
{
  lprint_job_t	*job = client->job;	// Job information
  ipp_attribute_t *attr;		// Current attribute


  // Get the job...
  if (!job)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

  // See if we already have a document for this job or the job has already
  // in a non-pending state...
  if (job->state > IPP_JSTATE_HELD)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED, "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  // Make sure we have the "last-document" operation attribute...
  if ((attr = ippFindAttribute(client->request, "last-document", IPP_TAG_ZERO)) == NULL)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required \"last-document\" attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The \"last-document\" attribute is not in the operation group.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 || !ippGetBoolean(attr, 0))
  {
    respond_unsupported(client, attr);
    httpFlush(client->http);
    return;
  }

  // Validate document attributes...
  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  // Then finish getting the document data and process things...
  pthread_rwlock_wrlock(&(client->printer->rwlock));

  lprintCopyAttributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

  pthread_rwlock_unlock(&(client->printer->rwlock));

  finish_document_data(client, job);
}


//
// 'ipp_set_printer_attributes()' - Set printer attributes.
//

static void
ipp_set_printer_attributes(
    lprint_client_t *client)		// I - Client
{
  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  if (!set_printer_attributes(client, client->printer))
    return;

  lprintRespondIPP(client, IPP_STATUS_OK, "Printer attributes set.");
}


//
// 'ipp_set_system_attributes()' - Set system attributes.
//

static void
ipp_set_system_attributes(
    lprint_client_t *client)		// I - Client
{
  lprint_system_t	*system = client->system;
					// System
  ipp_attribute_t	*rattr;		// Current request attribute
  ipp_tag_t		value_tag;	// Value tag
  int			count;		// Number of values
  const char		*name;		// Attribute name
  int			i;		// Looping var
  static lprint_attr_t	sattrs[] =	// Settable system attributes
  {
    { "default-printer-id",		IPP_TAG_INTEGER,	1 }
  };


  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  // Preflight request attributes...
  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s %s %s%s ...", ippTagString(ippGetGroupTag(rattr)), ippGetName(rattr), ippGetCount(rattr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(rattr)));

    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
    {
      continue;
    }
    else if (ippGetGroupTag(rattr) != IPP_TAG_SYSTEM)
    {
      respond_unsupported(client, rattr);
      continue;
    }

    name      = ippGetName(rattr);
    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    for (i = 0; i < (int)(sizeof(sattrs) / sizeof(sattrs[0])); i ++)
    {
      if (!strcmp(name, sattrs[i].name) && value_tag == sattrs[i].value_tag && count <= sattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(sattrs) / sizeof(sattrs[0])))
      respond_unsupported(client, rattr);

    if (!strcmp(name, "default-printer-id"))
    {
      if (!lprintFindPrinter(system, NULL, ippGetInteger(rattr, 0)))
      {
        respond_unsupported(client, rattr);
        break;
      }
    }
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
    return;

  // Now apply changes...
  pthread_rwlock_wrlock(&system->rwlock);

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
      continue;

    name = ippGetName(rattr);

    if (!strcmp(name, "default-printer-id"))
    {
      // Value was checked previously...
      system->default_printer = ippGetInteger(rattr, 0);
    }
  }

  pthread_rwlock_unlock(&system->rwlock);

  if (!system->save_time)
    system->save_time = time(NULL) + 1;

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_shutdown_all_printers()' - Shutdown the system.
//

static void
ipp_shutdown_all_printers(
    lprint_client_t *client)		// I - Client
{
  // Verify the connection is local...
  if (!is_domain_connection(client))
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_FORBIDDEN, "Administrative requests not supported over network connections.");
    return;
  }

  client->system->shutdown_time = time(NULL);

  lprintRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'ipp_validate_job()' - Validate job creation attributes.
//

static void
ipp_validate_job(
    lprint_client_t *client)		// I - Client
{
  if (valid_job_attributes(client))
    lprintRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'is_domain_connection()' - Return whether a client is local.
//

static int				// O - 1 if local, 0 otherwise
is_domain_connection(
    lprint_client_t *client)		// I - Client
{
  return (httpAddrFamily(httpGetAddress(client->http)) == AF_LOCAL);
}


//
// 'respond_unsupported()' - Respond with an unsupported attribute.
//

static void
respond_unsupported(
    lprint_client_t   *client,		// I - Client
    ipp_attribute_t *attr)		// I - Atribute
{
  ipp_attribute_t	*temp;		// Copy of attribute


  lprintRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


//
// 'set_printer_attributes()' - Set printer attributes.
//

static int				// O - 1 if OK, 0 on failure
set_printer_attributes(
    lprint_client_t  *client,		// I - Client
    lprint_printer_t *printer)		// I - Printer
{
  int			create_printer;	// Create-Printer request?
  ipp_attribute_t	*rattr,		// Current request attribute
			*pattr;		// Current printer attribute
  ipp_tag_t		value_tag;	// Value tag
  int			count;		// Number of values
  const char		*name;		// Attribute name
  int			i;		// Looping var
  static lprint_attr_t	pattrs[] =	// Settable printer attributes
  {
    { "copies-default",			IPP_TAG_INTEGER,	1 },
    { "document-format-default",	IPP_TAG_MIMETYPE,	1 },
    { "finishings-default",		IPP_TAG_ENUM,		1 },
    { "media-default",			IPP_TAG_KEYWORD,	1 },
    { "media-ready",			IPP_TAG_KEYWORD,	LPRINT_MAX_SOURCE },
    { "orientation-requested-default",	IPP_TAG_ENUM,		1 },
    { "print-color-mode-default",	IPP_TAG_KEYWORD,	1 },
    { "print-content-optimize-default",	IPP_TAG_KEYWORD,	1 },
    { "print-quality-default",		IPP_TAG_ENUM,		1 },
    { "printer-location",		IPP_TAG_TEXT,		1 },
    { "printer-geo-location",		IPP_TAG_URI,		1 },
    { "printer-organization",		IPP_TAG_TEXT,		1 },
    { "printer-organizational-unit",	IPP_TAG_TEXT,		1 },
    { "printer-resolution-default",	IPP_TAG_RESOLUTION,	1 }
  };


  // Preflight request attributes...
  create_printer = ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER;

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s %s %s%s ...", ippTagString(ippGetGroupTag(rattr)), ippGetName(rattr), ippGetCount(rattr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(rattr)));

    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
    {
      continue;
    }
    else if (ippGetGroupTag(rattr) != IPP_TAG_PRINTER)
    {
      respond_unsupported(client, rattr);
      continue;
    }

    name = ippGetName(rattr);

    if (create_printer && (!strcmp(name, "printer-name") || !strcmp(name, "device-uri") || !strcmp(name, "lprint-driver")))
      continue;

    value_tag = ippGetValueTag(rattr);
    count     = ippGetCount(rattr);

    for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
    {
      if (!strcmp(name, pattrs[i].name) && value_tag == pattrs[i].value_tag && count <= pattrs[i].max_count)
        break;
    }

    if (i >= (int)(sizeof(pattrs) / sizeof(pattrs[0])))
      respond_unsupported(client, rattr);
  }

  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
    return (0);

  // Now apply changes...
  pthread_rwlock_wrlock(&printer->rwlock);

  for (rattr = ippFirstAttribute(client->request); rattr; rattr = ippNextAttribute(client->request))
  {
    if (ippGetGroupTag(rattr) == IPP_TAG_OPERATION)
      continue;

    name = ippGetName(rattr);

    if (!strcmp(name, "media-ready"))
    {
      count = ippGetCount(rattr);

      for (i = 0; i < count; i ++)
      {
        const char *media = ippGetString(rattr, i, NULL);
					// Media value

        strlcpy(printer->driver->ready_media[i], media, sizeof(printer->driver->ready_media[i]));
      }

      for (; i < LPRINT_MAX_SOURCE; i ++)
        printer->driver->ready_media[i][0] = '\0';
    }
    else if (!strcmp(name, "printer-geo-location"))
    {
      free(printer->geo_location);
      printer->geo_location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-location"))
    {
      free(printer->location);
      printer->location = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-organization"))
    {
      free(printer->organization);
      printer->organization = strdup(ippGetString(rattr, 0, NULL));
    }
    else if (!strcmp(name, "printer-organization-unit"))
    {
      free(printer->org_unit);
      printer->org_unit = strdup(ippGetString(rattr, 0, NULL));
    }
    else
    {
      for (i = 0; i < (int)(sizeof(pattrs) / sizeof(pattrs[0])); i ++)
      {
	if (strcmp(name, pattrs[i].name))
	  continue;

        if ((pattr = ippFindAttribute(printer->attrs, name, IPP_TAG_ZERO)) != NULL)
          ippDeleteAttribute(printer->attrs, pattr);

        ippCopyAttribute(printer->attrs, rattr, 0);
      }
    }
  }

  printer->config_time = time(NULL);

  pthread_rwlock_unlock(&printer->rwlock);

  if (!client->system->save_time)
    client->system->save_time = time(NULL) + 1;

  return (1);
}


//
// 'valid_doc_attributes()' - Determine whether the document attributes are
//                            valid.
//
// When one or more document attributes are invalid, this function adds a
// suitable response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_doc_attributes(
    lprint_client_t *client)		// I - Client
{
  int			valid = 1;	// Valid attributes?
  ipp_op_t		op = ippGetOperation(client->request);
					// IPP operation
  const char		*op_name = ippOpString(op);
					// IPP operation name
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute
  const char		*compression = NULL,
					// compression value
			*format = NULL;	// document-format value


  // Check operation attributes...
  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
  {
    // If compression is specified, only accept a supported value in a Print-Job
    // or Send-Document request...
    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->attrs, "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetGroupTag(attr) != IPP_TAG_OPERATION || (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT && op != IPP_OP_VALIDATE_JOB) || !ippContainsString(supported, compression))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s \"compression\"='%s'", op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
      {
        lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Receiving job file with '%s' compression.", compression);
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
      }
    }
  }

  // Is it a format we support?
  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE || ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s \"document-format\"='%s'", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if (format && !strcmp(format, "application/octet-stream") && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
    // Auto-type the file using the first 8 bytes of the file...
    unsigned char	header[8];	// First 8 bytes of file

    memset(header, 0, sizeof(header));
    httpPeek(client->http, (char *)header, sizeof(header));

    if (!memcmp(header, "%PDF", 4))
      format = "application/pdf";
    else if (!memcmp(header, "%!", 2))
      format = "application/postscript";
    else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
      format = "image/jpeg";
    else if (!memcmp(header, "\211PNG", 4))
      format = "image/png";
    else if (!memcmp(header, "RAS2", 4))
      format = "image/pwg-raster";
    else if (!memcmp(header, "UNIRAST", 8))
      format = "image/urf";
    else
      format = NULL;

    if (format)
    {
      lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s Auto-typed \"document-format\"='%s'.", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  if (op != IPP_OP_CREATE_JOB && (supported = ippFindAttribute(client->printer->attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format))
  {
    respond_unsupported(client, attr);
    valid = 0;
  }

  // document-name
  if ((attr = ippFindAttribute(client->request, "document-name", IPP_TAG_NAME)) != NULL)
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "document-name-supplied", NULL, ippGetString(attr, 0, NULL));

  return (valid);
}


//
// 'valid_job_attributes()' - Determine whether the job attributes are valid.
//
// When one or more job attributes are invalid, this function adds a suitable
// response and attributes to the unsupported group.
//

static int				// O - 1 if valid, 0 if not
valid_job_attributes(
    lprint_client_t *client)		// I - Client
{
  int			i,		// Looping var
			count,		// Number of values
			valid = 1;	// Valid attributes?
  ipp_attribute_t	*attr,		// Current attribute
			*supported;	// xxx-supported attribute


  // If a shutdown is pending, do not accept more jobs...
  if (client->system->shutdown_time)
  {
    lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting new jobs.");
    return (0);
  }

  // Check operation attributes...
  valid = valid_doc_attributes(client);

  // Check the various job template attributes...
  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "no-hold"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD) || strcmp(ippGetString(attr, 0, NULL), "none"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG && ippGetValueTag(attr) != IPP_TAG_KEYWORD))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      supported = ippFindAttribute(client->printer->attrs, "media-supported", IPP_TAG_KEYWORD);

      if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		// media-col collection
			*size;		// media-size collection
    ipp_attribute_t	*member,	// Member attribute
			*x_dim,		// x-dimension
			*y_dim;		// y-dimension
    int			x_value,	// y-dimension value
			y_value;	// x-dimension value

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BEGIN_COLLECTION)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 || (ippGetValueTag(member) != IPP_TAG_NAME && ippGetValueTag(member) != IPP_TAG_NAMELANG && ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	supported = ippFindAttribute(client->printer->attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 || (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  respond_unsupported(client, attr);
	  valid = 0;
	}
	else
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  supported = ippFindAttribute(client->printer->attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    respond_unsupported(client, attr);
	    valid = 0;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD || (strcmp(ippGetString(attr, 0, NULL), "separate-documents-uncollated-copies") && strcmp(ippGetString(attr, 0, NULL), "separate-documents-collated-copies")))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_ORIENT_PORTRAIT || ippGetInteger(attr, 0) > IPP_ORIENT_REVERSE_PORTRAIT)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "page-ranges", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetValueTag(attr) != IPP_TAG_RANGE)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_ENUM || ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT || ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_RESOLUTION || !supported)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else
    {
      int	xdpi,			// Horizontal resolution for job template attribute
		ydpi,			// Vertical resolution for job template attribute
		sydpi;			// Vertical resolution for supported value
      ipp_res_t	units,			// Units for job template attribute
		sunits;			// Units for supported value

      xdpi  = ippGetResolution(attr, 0, &ydpi, &units);
      count = ippGetCount(supported);

      for (i = 0; i < count; i ++)
      {
        if (xdpi == ippGetResolution(supported, i, &sydpi, &sunits) && ydpi == sydpi && units == sunits)
          break;
      }

      if (i >= count)
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    const char *sides = ippGetString(attr, 0, NULL);
					// "sides" value...

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD)
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
    else if ((supported = ippFindAttribute(client->printer->attrs, "sides-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      if (!ippContainsString(supported, sides))
      {
	respond_unsupported(client, attr);
	valid = 0;
      }
    }
    else if (strcmp(sides, "one-sided"))
    {
      respond_unsupported(client, attr);
      valid = 0;
    }
  }

  return (valid);
}
