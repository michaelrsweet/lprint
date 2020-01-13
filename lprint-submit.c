//
// Submit sub-command for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
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

static char	*copy_stdin(char *name, size_t namesize);


//
// 'lprintDoSubmit()' - Do the submit sub-command.
//

int					// O - Exit status
lprintDoSubmit(
    int           num_files,		// I - Number of files
    char          **files,		// I - Files
    int           num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name,		// Printer name
		*filename,		// Current print filename
		*document_format,	// Document format
		*document_name,		// Document name
		*job_name;		// Job name
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  char		default_printer[256],	// Default printer name
		resource[1024],		// Resource path
		tempfile[1024];		// Temporary file
  int		i;			// Looping var
  char		*stdin_file;		// Dummy filename for passive stdin jobs
  ipp_attribute_t *job_id;		// job-id for created job


  // If there are no input files and stdin is not a TTY, treat that as an
  // implicit request to print from stdin...
  if (num_files == 0 && !isatty(0))
  {
    stdin_file = (char *)"-";
    files      = &stdin_file;
    num_files  = 1;
  }

  if (num_files == 0)
  {
    fputs("lprint: No files to print.\n", stderr);
    return (1);
  }

  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = lprintConnectURI(printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    http = lprintConnect(1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = lprintGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
	fputs("lprint: No default printer available.\n", stderr);
	httpClose(http);
	return (1);
      }
    }
  }

  // Loop through the print files
  job_name        = cupsGetOption("job-name", num_options, options);
  document_format = cupsGetOption("document-format", num_options, options);

  for (i = 0; i < num_files; i ++)
  {
    // Get the current print file...
    if (!strcmp(files[i], "-"))
    {
      if (!copy_stdin(tempfile, sizeof(tempfile)))
        break;

      filename      = tempfile;
      document_name = "(stdin)";
    }
    else
    {
      filename = files[i];

      if ((document_name = strrchr(filename, '/')) != NULL)
        document_name ++;
      else
        document_name = filename;
    }

    // Send a Print-Job request...
    request = ippNewRequest(IPP_OP_PRINT_JOB);
    if (printer_uri)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
    else
      lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, job_name ? job_name : document_name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name", NULL, document_name);
    if (document_format)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, document_format);
    lprintAddOptions(request, num_options, options);

    response = cupsDoFileRequest(http, request, resource, filename);

    if ((job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      fprintf(stderr, "lprint: Unable to print '%s' - %s\n", filename, cupsLastErrorString());
      ippDelete(response);
      break;
    }

    if (printer_uri)
      printf("%d\n", ippGetInteger(job_id, 0));
    else
      printf("%s-%d\n", printer_name, ippGetInteger(job_id, 0));

    ippDelete(response);

    if (filename == tempfile)
      unlink(tempfile);
  }

  httpClose(http);

  return (i < num_files);
}


//
// 'copy_stdin()' - Copy print data from the standard input...
//

static char *				// O - Temporary filename or `NULL` on error
copy_stdin(char   *name,		// I - Filename buffer
           size_t namesize)		// I - Size of filename buffer
{
  int		tempfd;			// Temporary file descriptor
  size_t	total = 0;		// Total bytes read/written
  ssize_t	bytes;			// Number of bytes read/written
  char		buffer[65536];		// Copy buffer


  if ((tempfd = cupsTempFd(name, (int)namesize)) < 0)
  {
    perror("lprint: Unable to create temporary file");
    return (NULL);
  }

  while ((bytes = read(0, buffer, sizeof(buffer))) > 0)
  {
    if (write(tempfd, buffer, (size_t)bytes) < 0)
    {
      perror("lprint: Unable to write to temporary file");
      close(tempfd);
      unlink(name);
      *name = '\0';
      return (NULL);
    }

    total += (size_t)bytes;
  }

  close(tempfd);

  if (total == 0)
  {
    fputs("lprint: No print data received on the standard input.\n", stderr);
    unlink(name);
    *name = '\0';
    return (NULL);
  }

  return (name);
}
