//
// Job object for LPrint, a Label Printer Application
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


/*
 * 'lprintCheckJobs()' - Check for new jobs to process.
 */

void
lprintCheckJobs(
    lprint_printer_t *printer)		// I - Printer
{
  lprint_job_t	*job;			// Current job


  lprintLogPrinter(printer, LPRINT_LOGLEVEL_DEBUG, "Checking for new jobs to process.");

  if (printer->processing_job)
  {
    lprintLogPrinter(printer, LPRINT_LOGLEVEL_DEBUG, "Printer is already processing job %d.", printer->processing_job->id);
    return;
  }
  else if (printer->is_deleted)
  {
    lprintLogPrinter(printer, LPRINT_LOGLEVEL_DEBUG, "Printer is being deleted.");
    return;
  }

  pthread_rwlock_wrlock(&printer->rwlock);

  for (job = (lprint_job_t *)cupsArrayFirst(printer->active_jobs);
       job;
       job = (lprint_job_t *)cupsArrayNext(printer->active_jobs))
  {
    if (job->state == IPP_JSTATE_PENDING)
    {
      pthread_t	t;			// Thread

      lprintLogPrinter(printer, LPRINT_LOGLEVEL_DEBUG, "Starting job %d.", job->id);

      if (pthread_create(&t, NULL, (void *(*)(void *))lprintProcessJob, job))
      {
	job->state     = IPP_JSTATE_ABORTED;
	job->completed = time(NULL);

	pthread_rwlock_wrlock(&printer->rwlock);

	cupsArrayRemove(printer->active_jobs, job);
	cupsArrayAdd(printer->completed_jobs, job);

	if (!printer->system->clean_time)
	  printer->system->clean_time = time(NULL) + 60;

	pthread_rwlock_unlock(&printer->rwlock);
      }
      else
	pthread_detach(t);
      break;
    }
  }

  if (!job)
    lprintLogPrinter(printer, LPRINT_LOGLEVEL_DEBUG, "No jobs to process at this time.");

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'lprintCleanJobs()' - Clean out old (completed) jobs.
//

void
lprintCleanJobs(
    lprint_system_t *system)		// I - System
{
  lprint_printer_t	*printer;	// Current printer
  lprint_job_t		*job;		// Current job
  time_t		cleantime;	// Clean time


  cleantime = time(NULL) - 60;

  pthread_rwlock_rdlock(&system->rwlock);

  for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    if (cupsArrayCount(printer->completed_jobs) == 0)
      continue;

    pthread_rwlock_wrlock(&printer->rwlock);

    for (job = (lprint_job_t *)cupsArrayFirst(printer->completed_jobs); job; job = (lprint_job_t *)cupsArrayNext(printer->completed_jobs))
    {
      if (job->completed && job->completed < cleantime)
      {
	cupsArrayRemove(printer->completed_jobs, job);
	cupsArrayRemove(printer->jobs, job);
      }
      else
	break;
    }

    pthread_rwlock_unlock(&printer->rwlock);
  }

  pthread_rwlock_unlock(&system->rwlock);
}


//
// 'lprintCreateJob()' - Create a new job object from a Print-Job or Create-Job request.
//

lprint_job_t *				// O - Job
lprintCreateJob(
    lprint_client_t *client)		// I - Client
{
  lprint_job_t		*job;		// Job
  ipp_attribute_t	*attr;		// Job attribute
  char			job_printer_uri[1024],
					// job-printer-uri value
			job_uri[1024],	// job-uri value
			job_uuid[64];	// job-uuid value


  // Allocate and initialize the job object...
  if ((job = calloc(1, sizeof(lprint_job_t))) == NULL)
  {
    lprintLog(client->system, LPRINT_LOGLEVEL_ERROR, "Unable to allocate memory for job: %s", strerror(errno));
    return (NULL);
  }

  job->system     = client->system;
  job->printer    = client->printer;
  job->attrs      = ippNew();
  job->state      = IPP_JSTATE_HELD;
  job->fd         = -1;

  // Copy all of the job attributes...
  lprintCopyAttributes(job->attrs, client->request, NULL, IPP_TAG_JOB, 0);

  // Get the requesting-user-name, document format, and priority...
  if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    job->username = ippGetString(attr, 0, NULL);
  else
    job->username = "anonymous";

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);

  if (ippGetOperation(client->request) != IPP_OP_CREATE_JOB)
  {
    if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else if ((attr = ippFindAttribute(job->attrs, "document-format-supplied", IPP_TAG_MIMETYPE)) != NULL)
      job->format = ippGetString(attr, 0, NULL);
    else
      job->format = "application/octet-stream";
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_INTEGER)) != NULL)
    job->impressions = ippGetInteger(attr, 0);

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_NAME)) != NULL)
    job->name = ippGetString(attr, 0, NULL);

  // Add job description attributes and add to the jobs array...
  pthread_rwlock_wrlock(&client->printer->rwlock);

  job->id = client->printer->next_job_id ++;

  if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
  {
    strlcpy(job_printer_uri, ippGetString(attr, 0, NULL), sizeof(job_printer_uri));

    snprintf(job_uri, sizeof(job_uri), "%s/%d", ippGetString(attr, 0, NULL), job->id);
  }
  else
  {
    httpAssembleURI(HTTP_URI_CODING_ALL, job_printer_uri, sizeof(job_printer_uri), "ipps", NULL, client->system->hostname, client->system->port, client->printer->resource);
    httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri), "ipps", NULL, client->system->hostname, client->system->port, "%s/%d", client->printer->resource, job->id);
  }

  lprintMakeUUID(client->system, client->printer->printer_name, job->id, job_uuid, sizeof(job_uuid));

  ippAddDate(job->attrs, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(time(&job->created)));
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, job_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, job_uuid);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, job_printer_uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  cupsArrayAdd(client->printer->jobs, job);
  cupsArrayAdd(client->printer->active_jobs, job);

  pthread_rwlock_unlock(&client->printer->rwlock);

  return (job);
}


//
// 'lprintCreateJobFile()' - Create a file for the document in a job.
//

int					// O - File descriptor or -1 on error
lprintCreateJobFile(
    lprint_job_t     *job,		// I - Job
    char             *fname,		// I - Filename buffer
    size_t           fnamesize,		// I - Size of filename buffer
    const char       *directory,	// I - Directory to store in
    const char       *ext)		// I - Extension (`NULL` for default)
{
  char			name[256],	// "Safe" filename
			*nameptr;	// Pointer into filename
  const char		*job_name;	// job-name value


  // Make a name from the job-name attribute...
  if ((job_name = ippGetString(ippFindAttribute(job->attrs, "job-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    job_name = "untitled";

  for (nameptr = name; *job_name && nameptr < (name + sizeof(name) - 1); job_name ++)
  {
    if (isalnum(*job_name & 255) || *job_name == '-')
    {
      *nameptr++ = (char)tolower(*job_name & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (job_name[1] && !isalnum(job_name[1] & 255) && job_name[1] != '-')
        job_name ++;
    }
  }

  *nameptr = '\0';

  // Figure out the extension...
  if (!ext)
  {
    if (!strcasecmp(job->format, "image/jpeg"))
      ext = "jpg";
    else if (!strcasecmp(job->format, "image/png"))
      ext = "png";
    else if (!strcasecmp(job->format, "image/pwg-raster"))
      ext = "pwg";
    else if (!strcasecmp(job->format, "image/urf"))
      ext = "urf";
    else if (!strcasecmp(job->format, "application/pdf"))
      ext = "pdf";
    else if (!strcasecmp(job->format, "application/postscript"))
      ext = "ps";
    else
      ext = "prn";
  }

  // Create a filename with the job-id, job-name, and document-format (extension)...
  snprintf(fname, fnamesize, "%s/%d-%s.%s", directory, job->id, name, ext);

  return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600));
}


//
// 'lprintDeleteJob()' - Remove a job from the system and free its memory.
//

void
lprintDeleteJob(lprint_job_t *job)	// I - Job
{
  lprintLogJob(job, LPRINT_LOGLEVEL_INFO, "Removing job from history.");

  ippDelete(job->attrs);

  if (job->message)
    free(job->message);

  if (job->filename)
  {
    unlink(job->filename);
    free(job->filename);
  }

  free(job);
}


//
// 'lprintFindJob()' - Find a job specified in a request.
//

lprint_job_t *				// O - Job or `NULL`
lprintFindJob(lprint_printer_t *printer,// I - Printer
              int              job_id)	// I - Job ID
{
  lprint_job_t		key,		// Job search key
			*job;		// Matching job, if any


  key.id = job_id;

  pthread_rwlock_rdlock(&(printer->rwlock));
  job = (lprint_job_t *)cupsArrayFind(printer->jobs, &key);
  pthread_rwlock_unlock(&(printer->rwlock));

  return (job);
}
