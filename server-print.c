//
// Print functions for LPrint, a Label Printer Utility
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

#ifdef HAVE_LIBPNG
static void	process_png(lprint_job_t *job);
#endif // HAVE_LIBPNG
static void	process_raster(lprint_job_t *job);
static void	process_raw(lprint_job_t *job);


//
// 'lprintProcessJob()' - Process a print job.
//

void *					// O - Thread exit status
lprintProcessJob(lprint_job_t *job)	// I - Job
{
  // Move the job to the processing state...
  pthread_rwlock_wrlock(&job->rwlock);

  job->state          = IPP_JSTATE_PROCESSING;
  job->printer->state = IPP_PSTATE_PROCESSING;
  job->processing     = time(NULL);
  job->printer->processing_job = job;

  pthread_rwlock_wrlock(&job->rwlock);

  // Process the job...
  if (!strcmp(job->format, "image/pwg-raster") || !strcmp(job->format, "image/urf"))
  {
    process_raster(job);
  }
#ifdef HAVE_LIBPNG
  else if (!strcmp(job->format, "image/png"))
  {
    process_png(job);
  }
#endif // HAVE_LIBPNG
  else if (!strcmp(job->format, job->printer->driver->format))
  {
    process_raw(job);
  }
  else
  {
    // Abort a job we can't process...
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to process job with format '%s'.", job->format);
    job->state = IPP_JSTATE_ABORTED;
  }

  // Move the job to a completed state...
  pthread_rwlock_wrlock(&job->rwlock);

  if (job->cancel)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  job->completed               = time(NULL);
  job->printer->state          = IPP_PSTATE_IDLE;
  job->printer->processing_job = NULL;

  pthread_rwlock_wrlock(&job->rwlock);

  pthread_rwlock_wrlock(&job->printer->rwlock);

  cupsArrayRemove(job->printer->active_jobs, job);
  cupsArrayAdd(job->printer->completed_jobs, job);

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&job->printer->rwlock);

  if (job->printer->is_deleted)
    lprintDeletePrinter(job->printer);
  else
    lprintCheckJobs(job->printer);

  return (NULL);
}


//
// 'process_png()' - Process a PNG image file.
//

#ifdef HAVE_LIBPNG
static void
process_png(lprint_job_t *job)		// I - Job
{
}
#endif // HAVE_LIBPNG


//
// 'process_raster()' - Process an Apple/PWG Raster file.
//

static void
process_raster(lprint_job_t *job)	// I - Job
{
}


//
// 'process_raw()' - Process a raw print file.
//

static void
process_raw(lprint_job_t *job)		// I - Job
{
}
