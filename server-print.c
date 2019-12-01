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


//
// 'lprintProcessJob()' - Process a print job.
//

void *					// O - Thread exit status
lprintProcessJob(lprint_job_t *job)	// I - Job
{
  job->state          = IPP_JSTATE_PROCESSING;
  job->printer->state = IPP_PSTATE_PROCESSING;
  job->processing     = time(NULL);

  while (job->printer->state_reasons & LPRINT_PREASON_MEDIA_EMPTY)
  {
    job->printer->state_reasons |= LPRINT_PREASON_MEDIA_NEEDED;

    sleep(1);
  }

  job->printer->state_reasons &= (lprint_preason_t)~LPRINT_PREASON_MEDIA_NEEDED;

  // TODO: Actually process job
  sleep(2);

  if (job->cancel)
    job->state = IPP_JSTATE_CANCELED;
  else if (job->state == IPP_JSTATE_PROCESSING)
    job->state = IPP_JSTATE_COMPLETED;

  job->completed           = time(NULL);
  job->printer->state      = IPP_PSTATE_IDLE;
  job->printer->active_job = NULL;

  return (NULL);
}
