//
// Logging functions for LPrint, a Label Printer Utility
//
// Note: Log format strings currently only support %d and %s!
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
#include <stdarg.h>
#include <syslog.h>


//
// Local functions...
//

static void	write_log(lprint_system_t *system, lprint_loglevel_t level, const char *message, va_list ap);


//
// Local globals...
//

static const int	syslevels[] =	// Mapping of log levels to syslog
{
  LOG_DEBUG | LOG_PID | LOG_LPR,
  LOG_INFO | LOG_PID | LOG_LPR,
  LOG_WARNING | LOG_PID | LOG_LPR,
  LOG_ERR | LOG_PID | LOG_LPR,
  LOG_CRIT | LOG_PID | LOG_LPR
};


//
// 'lprintLog()' - Log a message for the system.
//

void
lprintLog(lprint_system_t   *system,	// I - System
          lprint_loglevel_t level,	// I - Log level
          const char        *message,	// I - Printf-style message string
          ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments


  if (level < system->loglevel)
    return;

  va_start(ap, message);

  if (system->logfd >= 0)
    write_log(system, level, message, ap);
  else
    vsyslog(syslevels[level], message, ap);

  va_end(ap);
}


//
// 'lprintLogAttributes()' - Log attributes for a client connection.
//

void
lprintLogAttributes(
    lprint_client_t *client,		// I - Client
    const char      *title,		// I - Title for attributes
    ipp_t           *ipp,		// I - IPP message
    int             is_response)	// I - 1 if a response, 0 if a request
{
  if (client->system->loglevel > LPRINT_LOGLEVEL_DEBUG)
    return;
}


//
// 'lprintLogClient()' - Log a message for a client.
//

void
lprintLogClient(
    lprint_client_t   *client,		// I - Client
    lprint_loglevel_t level,		// I - Log level
    const char        *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		cmessage[1024];		// Message with client prefix
  va_list	ap;			// Pointer to arguments


  if (level < client->system->loglevel)
    return;

  snprintf(cmessage, sizeof(cmessage), "[Client %d] %s", client->number, message);
  va_start(ap, message);

  if (client->system->logfd >= 0)
    write_log(client->system, level, cmessage, ap);
  else
    vsyslog(syslevels[level], cmessage, ap);

  va_end(ap);
}


//
// 'lprintLogJob()' - Log a message for a job.
//

void
lprintLogJob(
    lprint_job_t      *job,		// I - Job
    lprint_loglevel_t level,		// I - Log level
    const char        *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		jmessage[1024];		// Message with job prefix
  va_list	ap;			// Pointer to arguments


  if (level < job->system->loglevel)
    return;

  snprintf(jmessage, sizeof(jmessage), "[Job %d] %s", job->id, message);
  va_start(ap, message);

  if (job->system->logfd >= 0)
    write_log(job->system, level, jmessage, ap);
  else
    vsyslog(syslevels[level], jmessage, ap);

  va_end(ap);
}


//
// 'lprintLogPrinter()' - Log a message for a printer.
//

void
lprintLogPrinter(
    lprint_printer_t  *printer,		// I - Printer
    lprint_loglevel_t level,		// I - Log level
    const char        *message,		// I - Printf-style message string
    ...)				// I - Additional arguments as needed
{
  char		pmessage[1024];		// Message with printer prefix
  va_list	ap;			// Pointer to arguments


  if (level < printer->system->loglevel)
    return;

  snprintf(pmessage, sizeof(pmessage), "[Printer %s] %s", printer->printer_name, message);
  va_start(ap, message);

  if (printer->system->logfd >= 0)
    write_log(printer->system, level, pmessage, ap);
  else
    vsyslog(syslevels[level], pmessage, ap);

  va_end(ap);
}


//
// 'write_log()' - Write a line to the log file...
//

static void
write_log(lprint_system_t   *system,	// I - System
          lprint_loglevel_t level,	// I - Log level
          const char        *message,	// I - Printf-style message string
          va_list           ap)		// I - Pointer to additional arguments
{
  char		buffer[2048],		// Output buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// Pointer to end of buffer
  time_t	curtime;		// Current time
  struct tm	curdate;		// Current date
  static const char *prefix = "DIWEF";	// Message prefix
  const char	*sval;			// String value


  // Each log line starts with a standard prefix of log level and date/time...
  time(&curtime);
  gmtime_r(&curtime, &curdate);

  snprintf(buffer, sizeof(buffer), "%c [%04d-%02d-%02dT%02d:%02d:%02dZ] ", prefix[level], curdate.tm_year + 1900, curdate.tm_mon + 1, curdate.tm_mday, curdate.tm_hour, curdate.tm_min, curdate.tm_sec);
  bufptr = buffer + 25;			// Skip level/date/time
  bufend = buffer + sizeof(buffer) - 1;	// Leave room for newline on end

  // Then format the message line using a subset of printf format sequences...
  while (*message && bufptr < bufend)
  {
    if (*message == '%')
    {
      message ++;

      switch (*message)
      {
        case 'd' : // Log an integer
            snprintf(bufptr, bufptr - bufend + 1, "%d", va_arg(ap, int));
            bufptr += strlen(bufptr);
            break;

        case 'p' : // Log a pointer
            snprintf(bufptr, bufptr - bufend + 1, "%p", va_arg(ap, void *));
            bufptr += strlen(bufptr);
            break;

        case 's' : // Log a string
            if ((sval = va_arg(ap, char *)) == NULL)
              sval = "(null)";

            while (*sval && bufptr < bufend)
            {
              int val = (*sval++) & 255;

              if (val < ' ' || val == 0x7f || val == '\\' || val == '\'' || val == '\"')
              {
                // Escape control and special characters in the string...
                if (bufptr > (bufend - 4))
                  break;

                *bufptr++ = '\\';

                if (val == '\\')
                  *bufptr++ = '\\';
                else if (val == '\'')
                  *bufptr++ = '\'';
                else if (val == '\"')
                  *bufptr++ = '\"';
                else if (val == '\n')
                  *bufptr++ = 'n';
                else if (val == '\r')
                  *bufptr++ = 'r';
                else if (val == '\t')
                  *bufptr++ = 't';
                else
                {
                  // Use octal escape for other control characters...
                  *bufptr++ = '0' + (val / 64);
                  *bufptr++ = '0' + ((val / 8) & 7);
                  *bufptr++ = '0' + (val & 7);
                }
              }
              else
                *bufptr++ = val;
            }
            break;

        case 'u' : // Log an unsigned integer
            snprintf(bufptr, bufptr - bufend + 1, "%u", va_arg(ap, unsigned));
            bufptr += strlen(bufptr);
            break;

        default : // Something else we don't support
            *bufptr++ = '%';

            if (bufptr < bufend)
              *bufptr++ = *message;
            break;
      }
    }
    else
      *bufptr++ = *message;

    message ++;
  }

  // Add a newline and write it out...
  *bufptr++ = '\n';

  write(system->logfd, buffer, bufptr - buffer);
}
