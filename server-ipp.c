//
// IPP processing for LPrint, a Label Printer Utility
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
// Local functions...
//

static void		copy_job_attributes(lprint_client_t *client, lprint_job_t *job, cups_array_t *ra);
static void		copy_printer_attributes(lprint_client_t *client, lprint_printer_t *printer, cups_array_t *ra);
static int		filter_cb(lprint_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);
static void		finish_document_data(lprint_client_t *client, lprint_job_t *job);

static void		ipp_cancel_job(lprint_client_t *client);
static void		ipp_close_job(lprint_client_t *client);
static void		ipp_create_job(lprint_client_t *client);
static void		ipp_get_job_attributes(lprint_client_t *client);
static void		ipp_get_jobs(lprint_client_t *client);
static void		ipp_get_printer_attributes(lprint_client_t *client);
static void		ipp_identify_printer(lprint_client_t *client);
static void		ipp_print_job(lprint_client_t *client);
static void		ipp_send_document(lprint_client_t *client);
static void		ipp_validate_job(lprint_client_t *client);

static void		respond_unsupported(lprint_client_t *client, ipp_attribute_t *attr);

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
  const char		*name;		// Name of attribute


  lprintLogAttributes(client, "Request", client->request, 1);

  // First build an empty response message for this request...
  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

  // Then validate the request header and required attributes...
  major = ippGetVersion(client->request, &minor);

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
      //   printer-uri/job-uri
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

      if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      if (charset && strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") && strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
      {
        // Bad character set...
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unsupported character set \"%s\".", ippGetString(charset, 0, NULL));
      }
      else if (!charset || !language || !uri)
      {
        // Return an error, since attributes-charset,
	// attributes-natural-language, and printer-uri/job-uri are required
	// for all operations.
	lprintRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required attributes.");
      }
      else
      {
        char		scheme[32],	// URI scheme
			userpass[32],	// Username/password in URI
			host[256],	// Host name in URI
			resource[256];	// Resource path in URI
	int		port;		// Port number in URI

        name = ippGetName(uri);

        if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
        {
	  lprintRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad %s value '%s'.", name, ippGetString(uri, 0, NULL));
        }
        else if ((!strcmp(name, "job-uri") && strncmp(resource, "/ipp/print/", 11)) || (!strcmp(name, "printer-uri") && strcmp(resource, "/ipp/print")))
	{
	  lprintRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "%s %s not found.", name, ippGetString(uri, 0, NULL));
	}
	else
	{
	  // Try processing the operation...
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
		ipp_cancel_job(client);
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

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", (int)printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
  {
    if (printer->state_reasons == LPRINT_PREASON_NONE)
    {
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-state-reasons", NULL, "none");
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
	    ippSetString(client->response, &attr, ippGetCount(attr), reason);
	  else
	    attr = ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-state-reasons", NULL, reason);
	}
      }
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

#if 0 // TODO: Fix me
  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", printer->active_job && printer->active_job->state < IPP_JSTATE_CANCELED);
#endif // 0
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
  lprint_job_t		*job;		// Job information


  // Get the job...
  if ((job = lprintFindJob(client)) == NULL)
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
	pthread_rwlock_wrlock(&(client->printer->rwlock));

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

	pthread_rwlock_unlock(&(client->printer->rwlock));

	lprintRespondIPP(client, IPP_STATUS_OK, NULL);

        if (!client->system->clean_time)
          client->system->clean_time = time(NULL) + 60;
        break;
  }
}


//
// 'ipp_close_job()' - Close an open job.
//

static void
ipp_close_job(lprint_client_t *client)	// I - Client
{
  lprint_job_t		*job;		// Job information


  // Get the job...
  if ((job = lprintFindJob(client)) == NULL)
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
// 'ipp_get_job_attributes()' - Get the attributes for a job object.
//

static void
ipp_get_job_attributes(
    lprint_client_t *client)		// I - Client
{
  lprint_job_t	*job;			// Job
  cups_array_t	*ra;			// requested-attributes


  if ((job = lprintFindJob(client)) == NULL)
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


  // TODO: Update status as needed

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
  lprint_job_t		*job;		// Job information
  ipp_attribute_t	*attr;		// Current attribute


  // Get the job...
  if ((job = lprintFindJob(client)) == NULL)
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
// 'ipp_validate_job()' - Validate job creation attributes.
//

static void
ipp_validate_job(lprint_client_t *client)	// I - Client
{
  if (valid_job_attributes(client))
    lprintRespondIPP(client, IPP_STATUS_OK, NULL);
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