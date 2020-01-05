//
// Main entry for LPrint, a Label Printer Utility
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#define _LPRINT_C_
#include "lprint.h"
#include <spawn.h>


//
// Local globals...
//

static char	*lprint_path = NULL;	// Path to self


//
// Local functions...
//

static void	usage(int status) LPRINT_NORETURN;


//
// 'main()' - Main entry for LPrint.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i, j;			// Looping vars
  const char	*opt,			// Current option character
		*subcommand = NULL;	// Sub-command name
  int		num_files = 0;		// Number of files
  char		*files[1000];		// Files
  int		num_options = 0;	// Number of options
  cups_option_t	*options = NULL;	// Options
  static const char * const subcommands[] =
  {					// List of sub-commands
    "add",
    "cancel",
    "default",
    "delete",
    "devices",
    "drivers",
    "jobs",
    "modify",
    "options",
    "printers",
    "server",
    "shutdown",
    "status",
    "submit"
  };


  lprint_path = argv[0];

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      usage(0);
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(LPRINT_VERSION);
      return (0);
    }
    else if (!strcmp(argv[i], "--"))
    {
      // Filename follows
      i ++;

      if (i >= argc)
      {
        fputs("lprint: Missing filename after '--'.\n", stderr);
        usage(1);
      }
      else if (num_files >= (int)(sizeof(files) / sizeof(files[0])))
      {
        fputs("lprint: Too many files.\n", stderr);
        return (1);
      }

      files[num_files ++] = argv[i];
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      fprintf(stderr, "lprint: Unknown option '%s'.\n", argv[i]);
      usage(1);
    }
    else if (!strcmp(argv[i], "-") || argv[i][0] != '-')
    {
      for (j = 0; j < (int)(sizeof(subcommands) / sizeof(subcommands[0])); j ++)
      {
        if (!strcmp(argv[i], subcommands[j]))
        {
          subcommand = argv[i];
          break;
	}
      }

      if (j >= (int)(sizeof(subcommands) / sizeof(subcommands[0])))
      {
	if (num_files >= (int)(sizeof(files) / sizeof(files[0])))
	{
	  fputs("lprint: Too many files.\n", stderr);
	  return (1);
	}

	files[num_files ++] = argv[i];
      }
    }
    else
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'a' : // All
              num_options = cupsAddOption("cancel-all", "true", num_options, &options);
              break;

          case 'd' : // printer name
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing printer name.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("printer-name", argv[i], num_options, &options);
              break;

          case 'j' : // job ID
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing job ID.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("job-id", argv[i], num_options, &options);
              break;

          case 'm' : // driver name
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing driver name.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("lprint-driver", argv[i], num_options, &options);
              break;

          case 'n' : // copies
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing copy count.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("copies", argv[i], num_options, &options);
              break;

          case 'o' : // name=value
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing option.\n", stderr);
                usage(1);
	      }

              num_options = cupsParseOptions(argv[i], num_options, &options);
              break;

          case 't' : // title
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing title.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("job-name", argv[i], num_options, &options);
              break;

          case 'u' : // printer-uri
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing printer URI.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("printer-uri", argv[i], num_options, &options);
              break;

          case 'v' : // device-uri
              i ++;
              if (i >= argc)
              {
                fputs("lprint: Missing device URI.\n", stderr);
                usage(1);
	      }

              num_options = cupsAddOption("device-uri", argv[i], num_options, &options);
              break;

          default :
              fprintf(stderr, "lprint: Unknown option '-%c'.\n", *opt);
              usage(1);
        }
      }
    }
  }

#ifdef DEBUG
  LPRINT_DEBUG("subcommand='%s'\n", subcommand);
  LPRINT_DEBUG("num_options=%d\n", num_options);
  for (i = 0; i < num_options; i ++)
    LPRINT_DEBUG("options[%d].name='%s', value='%s'\n", i, options[i].name, options[i].value);
  LPRINT_DEBUG("num_files=%d\n", num_files);
  for (i = 0; i < num_files; i ++)
    LPRINT_DEBUG("files[%d]='%s'\n", i, files[i]);
#endif // DEBUG

  if (!subcommand || !strcmp(subcommand, "submit"))
  {
    return (lprintDoSubmit(num_files, files, num_options, options));
  }
  else if (num_files > 0)
  {
    fprintf(stderr, "lprint: Sub-command '%s' does not accept files.\n", subcommand);
    usage(1);
  }
  else if (!strcmp(subcommand, "add"))
    return (lprintDoAdd(num_options, options));
  else if (!strcmp(subcommand, "cancel"))
    return (lprintDoCancel(num_options, options));
  else if (!strcmp(subcommand, "default"))
    return (lprintDoDefault(num_options, options));
  else if (!strcmp(subcommand, "delete"))
    return (lprintDoDelete(num_options, options));
  else if (!strcmp(subcommand, "devices"))
    return (lprintDoDevices(num_options, options));
  else if (!strcmp(subcommand, "drivers"))
    return (lprintDoDrivers(num_options, options));
  else if (!strcmp(subcommand, "jobs"))
    return (lprintDoJobs(num_options, options));
  else if (!strcmp(subcommand, "modify"))
    return (lprintDoModify(num_options, options));
  else if (!strcmp(subcommand, "options"))
    return (lprintDoOptions(num_options, options));
  else if (!strcmp(subcommand, "printers"))
    return (lprintDoPrinters(num_options, options));
  else if (!strcmp(subcommand, "server"))
    return (lprintDoServer(num_options, options));
  else if (!strcmp(subcommand, "shutdown"))
    return (lprintDoShutdown(num_options, options));
  else if (!strcmp(subcommand, "status"))
    return (lprintDoStatus(num_options, options));
  else
  {
    fprintf(stderr, "lprint: Unhandled sub-command '%s'.\n", subcommand);
    usage(1);
  }
}


//
// 'lprintAddPrinterURI()' - Add the printer-uri attribute and return a resource path.
//

void
lprintAddPrinterURI(
    ipp_t      *request,		// I - IPP request
    const char *printer_name,		// I - Printer name
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char	uri[1024];			// printer-uri value


  snprintf(resource, rsize, "/ipp/print/%s", printer_name);
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", 0, resource);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
}


//
// 'lprintConnect()' - Connect to the local server.
//

http_t *				// O - HTTP connection
lprintConnect(int auto_start)		// I - 1 to start server if not running
{
  http_t	*http;			// HTTP connection
  char		sockname[1024];		// Socket filename


  // See if the server is running...
  http = httpConnect2(lprintGetServerPath(sockname, sizeof(sockname)), 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

  if (!http && auto_start)
  {
    // Nope, start it now...
    pid_t	server_pid;		// Server process ID
    posix_spawnattr_t server_attrs;	// Server process attributes
    char	* const server_argv[] =	// Server command-line
    {
      lprint_path,
      "server",
      NULL
    };

    posix_spawnattr_init(&server_attrs);
    posix_spawnattr_setpgroup(&server_attrs, 0);

    if (posix_spawn(&server_pid, lprint_path, NULL, &server_attrs, server_argv, environ))
    {
      perror("Unable to start lprint server");
      posix_spawnattr_destroy(&server_attrs);
      return (NULL);
    }

    posix_spawnattr_destroy(&server_attrs);

    // Wait for it to start...
    while (access(lprintGetServerPath(sockname, sizeof(sockname)), 0))
      usleep(250000);

    http = httpConnect2(sockname, 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

    if (!http)
      fprintf(stderr, "lprint: Unable to connect to server - %s\n", cupsLastErrorString());
  }

  return (http);
}


//
// 'lprintConnectURI()' - Connect to an IPP printer directly.
//

http_t *				// O - HTTP connection or `NULL` on error
lprintConnectURI(
    const char *printer_uri,		// I - Printer URI
    char       *resource,		// I - Resource path buffer
    size_t     rsize)			// I - Size of buffer
{
  char			scheme[32],	// Scheme (ipp or ipps)
			userpass[256],	// Username/password (unused)
			hostname[256];	// Hostname
  int			port;		// Port number
  http_encryption_t	encryption;	// Type of encryption to use
  http_t		*http;		// HTTP connection


  // First extract the components of the URI...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, printer_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, (int)rsize) < HTTP_URI_STATUS_OK)
  {
    fprintf(stderr, "lprint: Bad printer URI '%s'.\n", printer_uri);
    return (NULL);
  }

  if (strcmp(scheme, "ipp") && strcmp(scheme, "ipps"))
  {
    fprintf(stderr, "lprint: Unsupported URI scheme '%s'.\n", scheme);
    return (NULL);
  }

  if (userpass[0])
    fputs("lprint: User credentials are not supported in URIs.\n", stderr);

  if (!strcmp(scheme, "ipps") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    fprintf(stderr, "lprint: Unable to connect to printer at '%s' - %s\n", printer_uri, cupsLastErrorString());

  return (http);
}


//
// 'lprintGetServerPath()' - Get the UNIX domain socket for the server.
//

char *					// O - Socket filename
lprintGetServerPath(char   *buffer,	// I - Buffer for filenaem
                    size_t bufsize)	// I - Size of buffer
{
  const char	*tmpdir = getenv("TMPDIR");
					// Temporary directory

#ifdef __APPLE__
  if (!tmpdir)
    tmpdir = "/private/tmp";
#else
  if (!tmpdir)
    tmpdir = "/tmp";
#endif // __APPLE__

  snprintf(buffer, bufsize, "%s/lprint%d.sock", tmpdir, (int)getuid());

  return (buffer);
}


//
// 'usage()' - Show program usage.
//

static void
usage(int status)			// O - Exit status
{
  FILE	*fp = status ? stderr : stdout;	// Where to send message


  fputs("Usage: lprint SUB-COMMAND [OPTIONS] [FILENAME]\n", fp);
  fputs("       lprint [OPTIONS] [FILENAME]\n", fp);
  fputs("       lprint [OPTIONS] -\n", fp);
  fputs("\n", fp);
  fputs("Sub-commands:\n", fp);
  fputs("  add PRINTER      Add a printer.\n", fp);
  fputs("  cancel           Cancel one or more jobs.\n", fp);
  fputs("  default          Set the default printer.\n", fp);
  fputs("  delete           Delete a printer.\n", fp);
  fputs("  devices          List devices.\n", fp);
  fputs("  drivers          List drivers.\n", fp);
  fputs("  jobs             List jobs.\n", fp);
  fputs("  modify           Modify a printer.\n", fp);
  fputs("  options          List printer options.\n", fp);
  fputs("  printers         List printers.\n", fp);
  fputs("  server           Run a server.\n", fp);
  fputs("  shutdown         Shutdown a running server.\n", fp);
  fputs("  status           Show server/printer/job status.\n", fp);
  fputs("  submit           Submit a file for printing.\n", fp);
  fputs("\n", fp);
  fputs("Options:\n", fp);
  fputs("  -a               Cancel all jobs (cancel).\n", fp);
  fputs("  -d PRINTER       Specify printer.\n", fp);
  fputs("  -j JOB-ID        Specify job ID (cancel).\n", fp);
  fputs("  -m DRIVER-NAME   Specify driver (add/modify).\n", fp);
  fputs("  -n COPIES        Specify number of copies (submit).\n", fp);
  fputs("  -o NAME=VALUE    Specify option (add,modify,server,submit).\n", fp);
  fputs("  -u PRINTER-URI   Specify ipp: or ipps: printer (cancel/options/status/submit).\n", fp);
  fputs("  -v DEVICE-URI    Specify socket: or usb: device (add/modify).\n", fp);

  exit(status);
}
