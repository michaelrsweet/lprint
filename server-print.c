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
#include "clustered.h"


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
  int	first_open = 1;			// Is this the first time we try to open the device?


  // Move the job to the processing state...
  pthread_rwlock_wrlock(&job->rwlock);

  job->state                   = IPP_JSTATE_PROCESSING;
  job->processing              = time(NULL);
  job->printer->processing_job = job;

  pthread_rwlock_wrlock(&job->rwlock);

  // Open the output device...
  pthread_rwlock_wrlock(&job->printer->driver->rwlock);

  while (!job->printer->driver->device)
  {
    job->printer->driver->device = lprintOpenDevice(job->printer->device_uri);

    if (!job->printer->driver->device)
    {
      // Log that the printer is unavailable then sleep for 5 seconds to retry.
      if (first_open)
      {
        lprintLogPrinter(job->printer, LPRINT_LOGLEVEL_ERROR, "Unable to open device '%s', pausing queue until printer becomes available.", job->printer->device_uri);
        first_open = 0;

	job->printer->state      = IPP_PSTATE_STOPPED;
	job->printer->state_time = time(NULL);
      }

      sleep(5);
    }
  }

  pthread_rwlock_unlock(&job->printer->driver->rwlock);

  // Process the job...
  job->printer->state      = IPP_PSTATE_PROCESSING;
  job->printer->state_time = time(NULL);

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
  job->printer->state_time     = time(NULL);
  job->printer->processing_job = NULL;

  pthread_rwlock_wrlock(&job->rwlock);

  pthread_rwlock_wrlock(&job->printer->rwlock);

  cupsArrayRemove(job->printer->active_jobs, job);
  cupsArrayAdd(job->printer->completed_jobs, job);

  if (!job->system->clean_time)
    job->system->clean_time = time(NULL) + 60;

  pthread_rwlock_unlock(&job->printer->rwlock);

  if (job->printer->is_deleted)
  {
    lprintDeletePrinter(job->printer);
  }
  else if (cupsArrayCount(job->printer->active_jobs) > 0)
  {
    lprintCheckJobs(job->printer);
  }
  else
  {
    pthread_rwlock_wrlock(&job->printer->driver->rwlock);

    lprintCloseDevice(job->printer->driver->device);
    job->printer->driver->device = NULL;

    pthread_rwlock_unlock(&job->printer->driver->rwlock);
  }

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
