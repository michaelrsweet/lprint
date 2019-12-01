//
// Job object for LPrint, a Label Printer Utility
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
#include <ctype.h>


//
// Local functions...
//

static int		compare_jobs(lprint_job_t *a, lprint_job_t *b);


//
// 'lprintCleanJobs()' - Clean out old (completed) jobs.
//

void
lprintCleanJobs(
    lprint_system_t *system)		// I - System
{
  lprint_job_t	*job;			// Current job
  time_t	cleantime;		// Clean time


  if (cupsArrayCount(system->jobs) == 0)
    return;

  cleantime = time(NULL) - 60;

  pthread_rwlock_wrlock(&(system->rwlock));
  for (job = (lprint_job_t *)cupsArrayFirst(system->jobs); job; job = (lprint_job_t *)cupsArrayNext(system->jobs))
  {
    if (job->completed && job->completed < cleantime)
    {
      cupsArrayRemove(system->jobs, job);
      lprintDeleteJob(job);
    }
    else
      break;
  }
  pthread_rwlock_unlock(&(system->rwlock));
}


//
// 'lprintCreateJob()' - Create a new job object from a Print-Job or Create-Job
//                  request.
//

lprint_job_t *				// O - Job
lprintCreateJob(
    lprint_client_t *client)		// I - Client
{
  lprint_job_t		*job;		// Job
  ipp_attribute_t	*attr;		// Job attribute
  char			uri[1024],	// job-uri value
			uuid[64];	// job-uuid value


  // Allocate and initialize the job object...
  if ((job = calloc(1, sizeof(lprint_job_t))) == NULL)
  {
    lprintLog(client->system, LPRINT_LOGLEVEL_ERROR, "Unable to allocate memory for job: %s", strerror(errno));
    return (NULL);
  }

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
  pthread_rwlock_wrlock(&client->system->rwlock);

  job->id = client->system->next_job_id ++;

  snprintf(uri, sizeof(uri), "%s/%d", client->printer->uri, job->id);
  httpAssembleUUID(client->system->hostname, client->system->port, client->printer->name, job->id, uuid, sizeof(uuid));

  ippAddDate(job->attrs, IPP_TAG_JOB, "date-time-at-creation", ippTimeToDate(time(&job->created)));
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, uuid);
  if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, ippGetString(attr, 0, NULL));
  else
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL, client->printer->uri);
  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation", (int)(job->created - client->printer->start_time));

  // TODO: create jobs array, add to active_jobs
  cupsArrayAdd(client->system->jobs, job);
//  client->printer->active_job = job;

  pthread_rwlock_unlock(&client->system->rwlock);

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
    else if (!strcasecmp(job->format, "application/vnd.hp-pcl"))
      ext = "pcl";
    else
      ext = "dat";
  }

  // Create a filename with the job-id, job-name, and document-format (extension)...
  snprintf(fname, fnamesize, "%s/%d-%s.%s", directory, job->id, name, ext);

  return (open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666));
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

lprint_job_t *				// O - Job or NULL
lprintFindJob(lprint_client_t *client)	// I - Client
{
  ipp_attribute_t	*attr;		// job-id or job-uri attribute
  lprint_job_t		key,		// Job search key
			*job;		// Matching job, if any


  if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL)
  {
    const char *uri = ippGetString(attr, 0, NULL);

    if (!strncmp(uri, client->printer->uri, client->printer->urilen) && uri[client->printer->urilen] == '/')
      key.id = atoi(uri + client->printer->urilen + 1);
    else
      return (NULL);
  }
  else if ((attr = ippFindAttribute(client->request, "job-id", IPP_TAG_INTEGER)) != NULL)
    key.id = ippGetInteger(attr, 0);

  pthread_rwlock_rdlock(&(client->printer->rwlock));
  job = (lprint_job_t *)cupsArrayFind(client->system->jobs, &key);
  pthread_rwlock_unlock(&(client->printer->rwlock));

  return (job);
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
