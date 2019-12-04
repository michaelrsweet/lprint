//
// Main entry for LPrint, a Label Printer Utility
//
// Copyright © 2019 by Michael R Sweet.
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
  lprint_path = argv[0];

  if (argc > 1)
  {
    if (!strcmp(argv[1], "--help"))
    {
      usage(0);
    }
    else if (!strcmp(argv[1], "--version"))
    {
      puts(LPRINT_VERSION);
      return (0);
    }
  }

  if (argc == 1 || !strcmp(argv[1], "submit") || argv[1][0] == '-')
    return (lprintDoSubmit(argc, argv));
  else if (!strcmp(argv[1], "add"))
    return (lprintDoAdd(argc, argv));
  else if (!strcmp(argv[1], "cancel"))
    return (lprintDoCancel(argc, argv));
  else if (!strcmp(argv[1], "default"))
    return (lprintDoDefault(argc, argv));
  else if (!strcmp(argv[1], "delete"))
    return (lprintDoDelete(argc, argv));
  else if (!strcmp(argv[1], "devices"))
    return (lprintDoDevices(argc, argv));
  else if (!strcmp(argv[1], "drivers"))
    return (lprintDoDrivers(argc, argv));
  else if (!strcmp(argv[1], "jobs"))
    return (lprintDoJobs(argc, argv));
  else if (!strcmp(argv[1], "modify"))
    return (lprintDoModify(argc, argv));
  else if (!strcmp(argv[1], "printers"))
    return (lprintDoPrinters(argc, argv));
  else if (!strcmp(argv[1], "server"))
    return (lprintDoServer(argc, argv));
  else if (!strcmp(argv[1], "shutdown"))
    return (lprintDoShutdown(argc, argv));
  else if (!strcmp(argv[1], "status"))
    return (lprintDoStatus(argc, argv));

  fprintf(stderr, "lprint: Unknown sub-command '%s'.\n", argv[1]);
  usage(1);
}


//
// 'lprintConnect()' - Connect to the local server.
//

http_t *				// O - HTTP connection
lprintConnect(void)
{
  char	sockname[1024];			// Socket filename


  // See if the server is running...
  if (access(lprintGetServerPath(sockname, sizeof(sockname)), 0))
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
  }

  return (httpConnect2(sockname, 0, NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL));
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

  snprintf(buffer, bufsize, "%s/lprint%d", tmpdir, (int)getuid());

  return (buffer);
}


//
// 'lprintIsServerRunning()' - Determine whether the local server is running.
//

int					// O - 1 if running, 0 otherwise
lprintIsServerRunning(void)
{
  char	sockname[1024];			// Socket filename


  return (!access(lprintGetServerPath(sockname, sizeof(sockname)), 0));
}


//
// 'usage()' - Show program usage.
//

static void
usage(int status)			// O - Exit status
{
  puts("Usage: lprint command [target] [options]");
  puts("       lprint [options] filename");
  puts("       lprint [options] -");
  puts("");
  puts("Commands:");
  puts("  add PRINTER      Add a printer.");
  puts("  cancel JOB-ID    Cancel one or more jobs.");
  puts("  default PRINTER  Set the default printer.");
  puts("  delete PRINTER   Delete a printer.");
  puts("  devices          List devices.");
  puts("  drivers          List drivers.");
  puts("  jobs             List jobs.");
  puts("  modify PRINTER   Modify a printer.");
  puts("  printers         List printers.");
  puts("  server           Run a server.");
  puts("  shutdown         Shutdown a running server.");
  puts("  status           Show server/printer/job status.");
  puts("  submit FILENAME  Submit a file for printing.");
  puts("");
  puts("Options:");
  puts("  -d PRINTER       Specify printer (submit).");
  puts("  -m DRIVER        Specify driver (add/modify).");
  puts("  -o NAME=VALUE    Specify printer option.");
  puts("  -v DEVICE-URI    Specify socket: or usb: device (add/modify).");

  exit(status);
}
