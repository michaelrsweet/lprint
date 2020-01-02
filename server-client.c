//
// Client processing code for LPrint, a Label Printer Utility
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
#include "lprint-png.h"
#include <ctype.h>


//
// Local functions...
//

//static void		html_escape(lprint_client_t *client, const char *s, size_t slen);
//static void		html_footer(lprint_client_t *client);
//static void		html_header(lprint_client_t *client, const char *title, int refresh);
//static void		html_printf(lprint_client_t *client, const char *format, ...) LPRINT_FORMAT(2, 3);
//static int		parse_options(lprint_client_t *client, cups_option_t **options);
//static int		show_media(lprint_client_t *client);
//static int		show_status(lprint_client_t *client);
//static int		show_supplies(lprint_client_t *client);
//static char		*time_string(time_t tv, char *buffer, size_t bufsize);


//
// 'lprintCreateClient()' - Accept a new network connection and create a client object.
//

lprint_client_t *			// O - Client
lprintCreateClient(
    lprint_system_t *system,		// I - Printer
    int             sock)		// I - Listen socket
{
  lprint_client_t	*client;	// Client


  if ((client = calloc(1, sizeof(lprint_client_t))) == NULL)
  {
    lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to allocate memory for client connection: %s", strerror(errno));
    return (NULL);
  }

  client->system = system;

  // Accept the client and get the remote address...
  if ((client->http = httpAcceptConnection(sock, 1)) == NULL)
  {
    lprintLog(system, LPRINT_LOGLEVEL_ERROR, "Unable to accept client connection: %s", strerror(errno));
    free(client);
    return (NULL);
  }

  httpGetHostname(client->http, client->hostname, sizeof(client->hostname));

  lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Accepted connection from '%s'.", client->hostname);

  return (client);
}


//
// 'lprintDeleteClient()' - Close the socket and free all memory used by a client object.
//

void
lprintDeleteClient(
    lprint_client_t *client)		// I - Client
{
  lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Closing connection from '%s'.", client->hostname);

  // Flush pending writes before closing...
  httpFlushWrite(client->http);

  // Free memory...
  httpClose(client->http);

  ippDelete(client->request);
  ippDelete(client->response);

  free(client);
}


//
// 'lprintProcessClient()' - Process client requests on a thread.
//

void *					// O - Exit status
lprintProcessClient(
    lprint_client_t *client)		// I - Client
{
  int first_time = 1;			// First time request?


  // Loop until we are out of requests or timeout (30 seconds)...
  while (httpWait(client->http, 30000))
  {
    if (first_time)
    {
      // See if we need to negotiate a TLS connection...
      char buf[1];			// First byte from client

      if (recv(httpGetFd(client->http), buf, 1, MSG_PEEK) == 1 && (!buf[0] || !strchr("DGHOPT", buf[0])))
      {
        lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Starting HTTPS session.");

	if (httpEncryption(client->http, HTTP_ENCRYPTION_ALWAYS))
	{
          lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	  break;
        }

        lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Connection now encrypted.");
      }

      first_time = 0;
    }

    if (!lprintProcessHTTP(client))
      break;
  }

  // Close the conection to the client and return...
  lprintDeleteClient(client);

  return (NULL);
}


//
// 'lprintProcessHTTP()' - Process a HTTP request.
//

int					// O - 1 on success, 0 on failure
lprintProcessHTTP(
    lprint_client_t *client)		// I - Client connection
{
  char			uri[1024];	// URI
  http_state_t		http_state;	// HTTP state
  http_status_t		http_status;	// HTTP status
  ipp_state_t		ipp_state;	// State of IPP transfer
  char			scheme[32],	// Method/scheme
			userpass[128],	// Username:password
			hostname[HTTP_MAX_HOST];
					// Hostname
  int			port;		// Port number
  static const char * const http_states[] =
  {					// Strings for logging HTTP method
    "WAITING",
    "OPTIONS",
    "GET",
    "GET_SEND",
    "HEAD",
    "POST",
    "POST_RECV",
    "POST_SEND",
    "PUT",
    "PUT_RECV",
    "DELETE",
    "TRACE",
    "CONNECT",
    "STATUS",
    "UNKNOWN_METHOD",
    "UNKNOWN_VERSION"
  };


  // Clear state variables...
  ippDelete(client->request);
  ippDelete(client->response);

  client->request   = NULL;
  client->response  = NULL;
  client->operation = HTTP_STATE_WAITING;

  // Read a request from the connection...
  while ((http_state = httpReadRequest(client->http, uri, sizeof(uri))) == HTTP_STATE_WAITING)
    usleep(1);

  // Parse the request line...
  if (http_state == HTTP_STATE_ERROR)
  {
    if (httpError(client->http) == EPIPE)
      lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Client closed connection.");
    else
      lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "Bad request line (%s).", strerror(httpError(client->http)));

    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_METHOD)
  {
    lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "Bad/unknown operation.");
    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_VERSION)
  {
    lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "Bad HTTP version.");
    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  lprintLogClient(client, LPRINT_LOGLEVEL_DEBUG, "%s %s", http_states[http_state], uri);

  // Separate the URI into its components...
  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK && (http_state != HTTP_STATE_OPTIONS || strcmp(uri, "*")))
  {
    lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "Bad URI '%s'.", uri);
    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if ((client->options = strchr(client->uri, '?')) != NULL)
    *(client->options)++ = '\0';

  // Process the request...
  client->start     = time(NULL);
  client->operation = httpGetState(client->http);

  // Parse incoming parameters until the status changes...
  while ((http_status = httpUpdate(client->http)) == HTTP_STATUS_CONTINUE);

  if (http_status != HTTP_STATUS_OK)
  {
    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if (!httpGetField(client->http, HTTP_FIELD_HOST)[0] && httpGetVersion(client->http) >= HTTP_VERSION_1_1)
  {
    // HTTP/1.1 and higher require the "Host:" field...
    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  // Handle HTTP Upgrade...
  if (!strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION), "Upgrade"))
  {
    if (strstr(httpGetField(client->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(client->http))
    {
      if (!lprintRespondHTTP(client, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL, 0))
        return (0);

      lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Upgrading to encrypted connection.");

      if (httpEncryption(client->http, HTTP_ENCRYPTION_REQUIRED))
      {
	lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	return (0);
      }

      lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "Connection now encrypted.");
    }
    else if (!lprintRespondHTTP(client, HTTP_STATUS_NOT_IMPLEMENTED, NULL, NULL, 0))
      return (0);
  }

  // Handle HTTP Expect...
  if (httpGetExpect(client->http) && (client->operation == HTTP_STATE_POST || client->operation == HTTP_STATE_PUT))
  {
    if (httpGetExpect(client->http) == HTTP_STATUS_CONTINUE)
    {
      // Send 100-continue header...
      if (!lprintRespondHTTP(client, HTTP_STATUS_CONTINUE, NULL, NULL, 0))
	return (0);
    }
    else
    {
      // Send 417-expectation-failed header...
      if (!lprintRespondHTTP(client, HTTP_STATUS_EXPECTATION_FAILED, NULL, NULL, 0))
	return (0);
    }
  }

  // Handle new transfers...
  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
        // Do OPTIONS command...
	return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, NULL, 0));

    case HTTP_STATE_HEAD :
        if (!strcmp(client->uri, "/lprint.png"))
	  return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "image/png", 0));
#if 0
	else if (!strcmp(client->uri, "/") || !strcmp(client->uri, "/media") || !strcmp(client->uri, "/supplies"))
	  return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0));
#endif // 0
	else
	  return (lprintRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_GET :
        if (!strcmp(client->uri, "/lprint.png"))
	{
	  // Send PNG icon file.
	  if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "image/png", sizeof(lprint_png)))
	    return (0);

	  httpWrite2(client->http, (const char *)lprint_png, sizeof(lprint_png));
	  httpFlushWrite(client->http);
	}
#if 0
	else if (!strcmp(client->uri, "/"))
	{
	 /*
	  * Show web status page...
	  */

          return (show_status(client));
	}
	else if (!strcmp(client->uri, "/media"))
	{
	 /*
	  * Show web media page...
	  */

          return (show_media(client));
	}
	else if (!strcmp(client->uri, "/supplies"))
	{
	 /*
	  * Show web supplies page...
	  */

          return (show_supplies(client));
	}
#endif // 0
	else
	  return (lprintRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
	break;

    case HTTP_STATE_POST :
	if (strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE),
	           "application/ipp"))
        {
	 /*
	  * Not an IPP request...
	  */

	  return (lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
	}

       /*
        * Read the IPP request...
	*/

	client->request = ippNew();

        while ((ipp_state = ippRead(client->http,
                                    client->request)) != IPP_STATE_DATA)
	{
	  if (ipp_state == IPP_STATE_ERROR)
	  {
            lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "IPP read error (%s).", cupsLastErrorString());
	    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
	    return (0);
	  }
	}

       /*
        * Now that we have the IPP request, process the request...
	*/

        return (lprintProcessIPP(client));

    default :
        break; /* Anti-compiler-warning-code */
  }

  return (1);
}


/*
 * 'lprintRespondHTTP()' - Send a HTTP response.
 */

int					/* O - 1 on success, 0 on failure */
lprintRespondHTTP(
    lprint_client_t *client,		/* I - Client */
    http_status_t code,			/* I - HTTP status of response */
    const char    *content_encoding,	/* I - Content-Encoding of response */
    const char    *type,		/* I - MIME media type of response */
    size_t        length)		/* I - Length of response */
{
  char	message[1024];			/* Text message */


  lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "%s", httpStatus(code));

  if (code == HTTP_STATUS_CONTINUE)
  {
   /*
    * 100-continue doesn't send any headers...
    */

    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE) == 0);
  }

 /*
  * Format an error message...
  */

  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

 /*
  * Send the HTTP response header...
  */

  httpClearFields(client->http);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED ||
      client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE,
                   "text/html; charset=utf-8");
    else
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, type);

    if (content_encoding)
      httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, content_encoding);
  }

  httpSetLength(client->http, length);

  if (httpWriteResponse(client->http, code) < 0)
    return (0);

 /*
  * Send the response data...
  */

  if (message[0])
  {
   /*
    * Send a plain text message.
    */

    if (httpPrintf(client->http, "%s", message) < 0)
      return (0);

    if (httpWrite2(client->http, "", 0) < 0)
      return (0);
  }
  else if (client->response)
  {
   /*
    * Send an IPP response...
    */

    lprintLogAttributes(client, "Response", client->response, 2);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (0);
  }

  return (1);
}


#if 0
/*
 * 'html_escape()' - Write a HTML-safe string.
 */

static void
html_escape(lprint_client_t *client,	/* I - Client */
	    const char    *s,		/* I - String to write */
	    size_t        slen)		/* I - Number of characters to write */
{
  const char	*start,			/* Start of segment */
		*end;			/* End of string */


  start = s;
  end   = s + (slen > 0 ? slen : strlen(s));

  while (*s && s < end)
  {
    if (*s == '&' || *s == '<')
    {
      if (s > start)
        httpWrite2(client->http, start, (size_t)(s - start));

      if (*s == '&')
        httpWrite2(client->http, "&amp;", 5);
      else
        httpWrite2(client->http, "&lt;", 4);

      start = s + 1;
    }

    s ++;
  }

  if (s > start)
    httpWrite2(client->http, start, (size_t)(s - start));
}


/*
 * 'html_footer()' - Show the web interface footer.
 *
 * This function also writes the trailing 0-length chunk.
 */

static void
html_footer(lprint_client_t *client)	/* I - Client */
{
  html_printf(client,
	      "</div>\n"
	      "</body>\n"
	      "</html>\n");
  httpWrite2(client->http, "", 0);
}


/*
 * 'html_header()' - Show the web interface header and title.
 */

static void
html_header(lprint_client_t *client,	/* I - Client */
            const char    *title,	/* I - Title */
            int           refresh)	/* I - Refresh timer, if any */
{
  html_printf(client,
	      "<!doctype html>\n"
	      "<html>\n"
	      "<head>\n"
	      "<title>%s</title>\n"
	      "<link rel=\"shortcut icon\" href=\"/lprint.png\" type=\"image/png\">\n"
	      "<link rel=\"apple-touch-icon\" href=\"/lprint.png\" type=\"image/png\">\n"
	      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title);
  if (refresh > 0)
    html_printf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  html_printf(client,
	      "<meta name=\"viewport\" content=\"width=device-width\">\n"
	      "<style>\n"
	      "body { font-family: sans-serif; margin: 0; }\n"
	      "div.body { padding: 0px 10px 10px; }\n"
	      "span.badge { background: #090; border-radius: 5px; color: #fff; padding: 5px 10px; }\n"
	      "span.bar { box-shadow: 0px 1px 5px #333; font-size: 75%%; }\n"
	      "table.form { border-collapse: collapse; margin-left: auto; margin-right: auto; margin-top: 10px; width: auto; }\n"
	      "table.form td, table.form th { padding: 5px 2px; }\n"
	      "table.form td.meter { border-right: solid 1px #ccc; padding: 0px; width: 400px; }\n"
	      "table.form th { text-align: right; }\n"
	      "table.striped { border-bottom: solid thin black; border-collapse: collapse; width: 100%%; }\n"
	      "table.striped tr:nth-child(even) { background: #fcfcfc; }\n"
	      "table.striped tr:nth-child(odd) { background: #f0f0f0; }\n"
	      "table.striped th { background: white; border-bottom: solid thin black; text-align: left; vertical-align: bottom; }\n"
	      "table.striped td { margin: 0; padding: 5px; vertical-align: top; }\n"
	      "table.nav { border-collapse: collapse; width: 100%%; }\n"
	      "table.nav td { margin: 0; text-align: center; }\n"
	      "td.nav a, td.nav a:active, td.nav a:hover, td.nav a:hover:link, td.nav a:hover:link:visited, td.nav a:link, td.nav a:link:visited, td.nav a:visited { background: inherit; color: inherit; font-size: 80%%; text-decoration: none; }\n"
	      "td.nav { background: #333; color: #fff; padding: 4px 8px; width: 33%%; }\n"
	      "td.nav.sel { background: #fff; color: #000; font-weight: bold; }\n"
	      "td.nav:hover { background: #666; color: #fff; }\n"
	      "td.nav:active { background: #000; color: #ff0; }\n"
	      "</style>\n"
	      "</head>\n"
	      "<body>\n"
	      "<table class=\"nav\"><tr>"
	      "<td class=\"nav%s\"><a href=\"/\">Status</a></td>"
	      "<td class=\"nav%s\"><a href=\"/supplies\">Supplies</a></td>"
	      "<td class=\"nav%s\"><a href=\"/media\">Media</a></td>"
	      "</tr></table>\n"
	      "<div class=\"body\">\n", !strcmp(client->uri, "/") ? " sel" : "", !strcmp(client->uri, "/supplies") ? " sel" : "", !strcmp(client->uri, "/media") ? " sel" : "");
}


/*
 * 'html_printf()' - Send formatted text to the client, quoting as needed.
 */

static void
html_printf(lprint_client_t *client,	/* I - Client */
	    const char    *format,	/* I - Printf-style format string */
	    ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to arguments */
  const char	*start;			/* Start of string */
  char		size,			/* Size character (h, l, L) */
		type;			/* Format type character */
  int		width,			/* Width of field */
		prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for sprintf() */
		*tptr,			/* Pointer into temporary format */
		temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */


 /*
  * Loop through the format string, formatting as needed...
  */

  va_start(ap, format);
  start = format;

  while (*format)
  {
    if (*format == '%')
    {
      if (format > start)
        httpWrite2(client->http, start, (size_t)(format - start));

      tptr    = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        httpWrite2(client->http, "%", 1);
        format ++;
	start = format;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        *tptr++ = *format++;

      if (*format == '*')
      {
       /*
        * Get width from argument...
	*/

	format ++;
	width = va_arg(ap, int);

	snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", width);
	tptr += strlen(tptr);
      }
      else
      {
	width = 0;

	while (isdigit(*format & 255))
	{
	  if (tptr < (tformat + sizeof(tformat) - 1))
	    *tptr++ = *format;

	  width = width * 10 + *format++ - '0';
	}
      }

      if (*format == '.')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        format ++;

        if (*format == '*')
	{
         /*
	  * Get precision from argument...
	  */

	  format ++;
	  prec = va_arg(ap, int);

	  snprintf(tptr, sizeof(tformat) - (size_t)(tptr - tformat), "%d", prec);
	  tptr += strlen(tptr);
	}
	else
	{
	  prec = 0;

	  while (isdigit(*format & 255))
	  {
	    if (tptr < (tformat + sizeof(tformat) - 1))
	      *tptr++ = *format;

	    prec = prec * 10 + *format++ - '0';
	  }
	}
      }

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

	if (tptr < (tformat + sizeof(tformat) - 2))
	{
	  *tptr++ = 'l';
	  *tptr++ = 'l';
	}

	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
	if (tptr < (tformat + sizeof(tformat) - 1))
	  *tptr++ = *format;

        size = *format++;
      }
      else
        size = 0;


      if (!*format)
      {
        start = format;
        break;
      }

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';
      start = format;

      switch (type)
      {
	case 'E' : /* Floating point formats */
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, double));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'B' : /* Integer formats */
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

#  ifdef HAVE_LONG_LONG
            if (size == 'L')
	      sprintf(temp, tformat, va_arg(ap, long long));
	    else
#  endif /* HAVE_LONG_LONG */
            if (size == 'l')
	      sprintf(temp, tformat, va_arg(ap, long));
	    else
	      sprintf(temp, tformat, va_arg(ap, int));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

	case 'p' : /* Pointer value */
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : /* Character or character array */
            if (width <= 1)
            {
              temp[0] = (char)va_arg(ap, int);
              temp[1] = '\0';
              html_escape(client, temp, 1);
            }
            else
              html_escape(client, va_arg(ap, char *), (size_t)width);
	    break;

	case 's' : /* String */
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

            html_escape(client, s, strlen(s));
	    break;
      }
    }
    else
      format ++;
  }

  if (format > start)
    httpWrite2(client->http, start, (size_t)(format - start));

  va_end(ap);
}
#endif // 0


#if 0
/*
 * 'parse_options()' - Parse URL options into CUPS options.
 *
 * The client->options string is destroyed by this function.
 */

static int				/* O - Number of options */
parse_options(lprint_client_t *client,	/* I - Client */
              cups_option_t   **options)/* O - Options */
{
  char	*name,				/* Name */
      	*value,				/* Value */
	*next;				/* Next name=value pair */
  int	num_options = 0;		/* Number of options */


  *options = NULL;

  for (name = client->options; name && *name; name = next)
  {
    if ((value = strchr(name, '=')) == NULL)
      break;

    *value++ = '\0';
    if ((next = strchr(value, '&')) != NULL)
      *next++ = '\0';

    num_options = cupsAddOption(name, value, num_options, options);
  }

  return (num_options);
}


/*
 * 'show_media()' - Show media load state.
 */

static int				/* O - 1 on success, 0 on failure */
show_media(lprint_client_t  *client)	/* I - Client connection */
{
  lprint_printer_t *printer = client->printer;
					/* Printer */
  int			i, j,		/* Looping vars */
                        num_ready,	/* Number of ready media */
                        num_sizes,	/* Number of media sizes */
			num_sources,	/* Number of media sources */
                        num_types;	/* Number of media types */
  ipp_attribute_t	*media_col_ready,/* media-col-ready attribute */
                        *media_ready,	/* media-ready attribute */
                        *media_sizes,	/* media-supported attribute */
                        *media_sources,	/* media-source-supported attribute */
                        *media_types,	/* media-type-supported attribute */
                        *input_tray;	/* printer-input-tray attribute */
  ipp_t			*media_col;	/* media-col value */
  const char            *media_size,	/* media value */
                        *media_source,	/* media-source value */
                        *media_type,	/* media-type value */
                        *ready_size,	/* media-col-ready media-size[-name] value */
                        *ready_source,	/* media-col-ready media-source value */
                        *ready_tray,	/* printer-input-tray value */
                        *ready_type;	/* media-col-ready media-type value */
  char			tray_str[1024],	/* printer-input-tray string value */
			*tray_ptr;	/* Pointer into value */
  int			tray_len;	/* Length of printer-input-tray value */
  int			ready_sheets;	/* printer-input-tray sheets value */
  int			num_options = 0;/* Number of form options */
  cups_option_t		*options = NULL;/* Form options */
  static const int	sheets[] =	/* Number of sheets */
  {
    250,
    125,
    50,
    25,
    5,
    0,
    -2
  };


  if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, 0);

  if ((media_col_ready = ippFindAttribute(printer->attrs, "media-col-ready", IPP_TAG_BEGIN_COLLECTION)) == NULL)
  {
    html_printf(client, "<p>Error: No media-col-ready defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  media_ready = ippFindAttribute(printer->attrs, "media-ready", IPP_TAG_ZERO);

  if ((media_sizes = ippFindAttribute(printer->attrs, "media-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((media_sources = ippFindAttribute(printer->attrs, "media-source-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-source-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((media_types = ippFindAttribute(printer->attrs, "media-type-supported", IPP_TAG_ZERO)) == NULL)
  {
    html_printf(client, "<p>Error: No media-type-supported defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if ((input_tray = ippFindAttribute(printer->attrs, "printer-input-tray", IPP_TAG_STRING)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-input-tray defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  num_ready   = ippGetCount(media_col_ready);
  num_sizes   = ippGetCount(media_sizes);
  num_sources = ippGetCount(media_sources);
  num_types   = ippGetCount(media_types);

  if (num_sources != ippGetCount(input_tray))
  {
    html_printf(client, "<p>Error: Different number of trays in media-source-supported and printer-input-tray defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

 /*
  * Process form data if present...
  */

  if (printer->web_forms)
    num_options = parse_options(client, &options);

  if (num_options > 0)
  {
   /*
    * WARNING: A real printer/server implementation MUST NOT implement
    * media updates via a GET request - GET requests are supposed to be
    * idempotent (without side-effects) and we obviously are not
    * authenticating access here.  This form is provided solely to
    * enable testing and development!
    */

    char	name[255];		/* Form name */
    const char	*val;			/* Form value */
    pwg_media_t	*media;			/* Media info */

    _cupsRWLockWrite(&printer->rwlock);

    ippDeleteAttribute(printer->attrs, media_col_ready);
    media_col_ready = NULL;

    if (media_ready)
    {
      ippDeleteAttribute(printer->attrs, media_ready);
      media_ready = NULL;
    }

    printer->state_reasons &= (lprint_preason_t)~(LPRINT_PREASON_MEDIA_LOW | LPRINT_PREASON_MEDIA_EMPTY | LPRINT_PREASON_MEDIA_NEEDED);

    for (i = 0; i < num_sources; i ++)
    {
      media_source = ippGetString(media_sources, i, NULL);

      if (!strcmp(media_source, "auto") || !strcmp(media_source, "manual") || strstr(media_source, "-man") != NULL)
	continue;

      snprintf(name, sizeof(name), "size%d", i);
      if ((media_size = cupsGetOption(name, num_options, options)) != NULL && (media = pwgMediaForPWG(media_size)) != NULL)
      {
        snprintf(name, sizeof(name), "type%d", i);
        if ((media_type = cupsGetOption(name, num_options, options)) != NULL && !*media_type)
          media_type = NULL;

        if (media_ready)
          ippSetString(printer->attrs, &media_ready, ippGetCount(media_ready), media_size);
        else
          media_ready = ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "media-ready", NULL, media_size);

        media_col = create_media_col(media_size, media_source, media_type, media->width, media->length, -1, -1, -1, -1);

        if (media_col_ready)
          ippSetCollection(printer->attrs, &media_col_ready, ippGetCount(media_col_ready), media_col);
        else
          media_col_ready = ippAddCollection(printer->attrs, IPP_TAG_PRINTER, "media-col-ready", media_col);
        ippDelete(media_col);
      }
      else
        media = NULL;

      snprintf(name, sizeof(name), "level%d", i);
      if ((val = cupsGetOption(name, num_options, options)) != NULL)
        ready_sheets = atoi(val);
      else
        ready_sheets = 0;

      snprintf(tray_str, sizeof(tray_str), "type=sheetFeedAuto%sRemovableTray;mediafeed=%d;mediaxfeed=%d;maxcapacity=%d;level=%d;status=0;name=%s;", !strcmp(media_source, "by-pass-tray") ? "Non" : "", media ? media->length : 0, media ? media->width : 0, strcmp(media_source, "by-pass-tray") ? 250 : 25, ready_sheets, media_source);

      ippSetOctetString(printer->attrs, &input_tray, i, tray_str, (int)strlen(tray_str));

      if (ready_sheets == 0)
      {
        printer->state_reasons |= LPRINT_PREASON_MEDIA_EMPTY;
        if (printer->active_job)
          printer->state_reasons |= LPRINT_PREASON_MEDIA_NEEDED;
      }
      else if (ready_sheets < 25 && ready_sheets > 0)
        printer->state_reasons |= LPRINT_PREASON_MEDIA_LOW;
    }

    if (!media_col_ready)
      media_col_ready = ippAddOutOfBand(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "media-col-ready");

    if (!media_ready)
      media_ready = ippAddOutOfBand(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "media-ready");

    _cupsRWUnlock(&printer->rwlock);
  }

  if (printer->web_forms)
    html_printf(client, "<form method=\"GET\" action=\"/media\">\n");

  html_printf(client, "<table class=\"form\" summary=\"Media\">\n");
  for (i = 0; i < num_sources; i ++)
  {
    media_source = ippGetString(media_sources, i, NULL);

    if (!strcmp(media_source, "auto") || !strcmp(media_source, "manual") || strstr(media_source, "-man") != NULL)
      continue;

    for (j = 0, ready_size = NULL, ready_type = NULL; j < num_ready; j ++)
    {
      media_col    = ippGetCollection(media_col_ready, j);
      ready_size   = ippGetString(ippFindAttribute(media_col, "media-size-name", IPP_TAG_ZERO), 0, NULL);
      ready_source = ippGetString(ippFindAttribute(media_col, "media-source", IPP_TAG_ZERO), 0, NULL);
      ready_type   = ippGetString(ippFindAttribute(media_col, "media-type", IPP_TAG_ZERO), 0, NULL);

      if (ready_source && !strcmp(ready_source, media_source))
        break;

      ready_source = NULL;
      ready_size   = NULL;
      ready_type   = NULL;
    }

    html_printf(client, "<tr><th>%s:</th>", media_source);

   /*
    * Media size...
    */

    if (printer->web_forms)
    {
      html_printf(client, "<td><select name=\"size%d\"><option value=\"\">None</option>", i);
      for (j = 0; j < num_sizes; j ++)
      {
	media_size = ippGetString(media_sizes, j, NULL);

	html_printf(client, "<option%s>%s</option>", (ready_size && !strcmp(ready_size, media_size)) ? " selected" : "", media_size);
      }
      html_printf(client, "</select>");
    }
    else
      html_printf(client, "<td>%s", ready_size);

   /*
    * Media type...
    */

    if (printer->web_forms)
    {
      html_printf(client, " <select name=\"type%d\"><option value=\"\">None</option>", i);
      for (j = 0; j < num_types; j ++)
      {
	media_type = ippGetString(media_types, j, NULL);

	html_printf(client, "<option%s>%s</option>", (ready_type && !strcmp(ready_type, media_type)) ? " selected" : "", media_type);
      }
      html_printf(client, "</select>");
    }
    else if (ready_type)
      html_printf(client, ", %s", ready_type);

   /*
    * Level/sheets loaded...
    */

    if ((ready_tray = ippGetOctetString(input_tray, i, &tray_len)) != NULL)
    {
      if (tray_len > (int)(sizeof(tray_str) - 1))
        tray_len = (int)sizeof(tray_str) - 1;
      memcpy(tray_str, ready_tray, (size_t)tray_len);
      tray_str[tray_len] = '\0';

      if ((tray_ptr = strstr(tray_str, "level=")) != NULL)
        ready_sheets = atoi(tray_ptr + 6);
      else
        ready_sheets = 0;
    }
    else
      ready_sheets = 0;

    if (printer->web_forms)
    {
      html_printf(client, " <select name=\"level%d\">", i);
      for (j = 0; j < (int)(sizeof(sheets) / sizeof(sheets[0])); j ++)
      {
	if (!strcmp(media_source, "by-pass-tray") && sheets[j] > 25)
	  continue;

	if (sheets[j] < 0)
	  html_printf(client, "<option value=\"%d\"%s>Unknown</option>", sheets[j], sheets[j] == ready_sheets ? " selected" : "");
	else
	  html_printf(client, "<option value=\"%d\"%s>%d sheets</option>", sheets[j], sheets[j] == ready_sheets ? " selected" : "", sheets[j]);
      }
      html_printf(client, "</select></td></tr>\n");
    }
    else if (ready_sheets == 1)
      html_printf(client, ", 1 sheet</td></tr>\n");
    else if (ready_sheets > 0)
      html_printf(client, ", %d sheets</td></tr>\n", ready_sheets);
    else
      html_printf(client, "</td></tr>\n");
  }

  if (printer->web_forms)
  {
    html_printf(client, "<tr><td></td><td><input type=\"submit\" value=\"Update Media\">");
    if (num_options > 0)
      html_printf(client, " <span class=\"badge\" id=\"status\">Media updated.</span>\n");
    html_printf(client, "</td></tr></table></form>\n");

    if (num_options > 0)
      html_printf(client, "<script>\n"
			  "setTimeout(hide_status, 3000);\n"
			  "function hide_status() {\n"
			  "  var status = document.getElementById('status');\n"
			  "  status.style.display = 'none';\n"
			  "}\n"
			  "</script>\n");
  }
  else
    html_printf(client, "</table>\n");

  html_footer(client);

  return (1);
}


/*
 * 'show_status()' - Show printer/system state.
 */

static int				/* O - 1 on success, 0 on failure */
show_status(lprint_client_t  *client)	/* I - Client connection */
{
  lprint_printer_t *printer = client->printer;
					/* Printer */
  lprint_job_t		*job;		/* Current job */
  int			i;		/* Looping var */
  lprint_preason_t	reason;		/* Current reason */
  static const char * const reasons[] =	/* Reason strings */
  {
    "Other",
    "Cover Open",
    "Input Tray Missing",
    "Marker Supply Empty",
    "Marker Supply Low",
    "Marker Waste Almost Full",
    "Marker Waste Full",
    "Media Empty",
    "Media Jam",
    "Media Low",
    "Media Needed",
    "Moving to Paused",
    "Paused",
    "Spool Area Full",
    "Toner Empty",
    "Toner Low"
  };
  static const char * const state_colors[] =
  {					/* State colors */
    "#0C0",				/* Idle */
    "#EE0",				/* Processing */
    "#C00"				/* Stopped */
  };


  if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, printer->state == IPP_PSTATE_PROCESSING ? 5 : 15);
  html_printf(client, "<h1><img style=\"background: %s; border-radius: 10px; float: left; margin-right: 10px; padding: 10px;\" src=\"/icon.png\" width=\"64\" height=\"64\">%s Jobs</h1>\n", state_colors[printer->state - IPP_PSTATE_IDLE], printer->name);
  html_printf(client, "<p>%s, %d job(s).", printer->state == IPP_PSTATE_IDLE ? "Idle" : printer->state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", cupsArrayCount(printer->jobs));
  for (i = 0, reason = 1; i < (int)(sizeof(reasons) / sizeof(reasons[0])); i ++, reason <<= 1)
    if (printer->state_reasons & reason)
      html_printf(client, "\n<br>&nbsp;&nbsp;&nbsp;&nbsp;%s", reasons[i]);
  html_printf(client, "</p>\n");

  if (cupsArrayCount(printer->jobs) > 0)
  {
    _cupsRWLockRead(&(printer->rwlock));

    html_printf(client, "<table class=\"striped\" summary=\"Jobs\"><thead><tr><th>Job #</th><th>Name</th><th>Owner</th><th>Status</th></tr></thead><tbody>\n");
    for (job = (lprint_job_t *)cupsArrayFirst(printer->jobs); job; job = (lprint_job_t *)cupsArrayNext(printer->jobs))
    {
      char	when[256],		/* When job queued/started/finished */
	      hhmmss[64];		/* Time HH:MM:SS */

      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	case IPP_JSTATE_HELD :
	    snprintf(when, sizeof(when), "Queued at %s", time_string(job->created, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_PROCESSING :
	case IPP_JSTATE_STOPPED :
	    snprintf(when, sizeof(when), "Started at %s", time_string(job->processing, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_ABORTED :
	    snprintf(when, sizeof(when), "Aborted at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_CANCELED :
	    snprintf(when, sizeof(when), "Canceled at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
	case IPP_JSTATE_COMPLETED :
	    snprintf(when, sizeof(when), "Completed at %s", time_string(job->completed, hhmmss, sizeof(hhmmss)));
	    break;
      }

      html_printf(client, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", job->id, job->name, job->username, when);
    }
    html_printf(client, "</tbody></table>\n");

    _cupsRWUnlock(&(printer->rwlock));
  }

  html_footer(client);

  return (1);
}


/*
 * 'show_supplies()' - Show printer supplies.
 */

static int				/* O - 1 on success, 0 on failure */
show_supplies(
    lprint_client_t  *client)		/* I - Client connection */
{
  lprint_printer_t *printer = client->printer;
					/* Printer */
  int		i,			/* Looping var */
		num_supply;		/* Number of supplies */
  ipp_attribute_t *supply,		/* printer-supply attribute */
		*supply_desc;		/* printer-supply-description attribute */
  int		num_options = 0;	/* Number of form options */
  cups_option_t	*options = NULL;	/* Form options */
  int		supply_len,		/* Length of supply value */
		level;			/* Supply level */
  const char	*supply_value;		/* Supply value */
  char		supply_text[1024],	/* Supply string */
		*supply_ptr;		/* Pointer into supply string */
  static const char * const printer_supply[] =
  {					/* printer-supply values */
    "index=1;class=receptacleThatIsFilled;type=wasteToner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=unknown;",
    "index=2;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=black;",
    "index=3;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=cyan;",
    "index=4;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=magenta;",
    "index=5;class=supplyThatIsConsumed;type=toner;unit=percent;"
        "maxcapacity=100;level=%d;colorantname=yellow;"
  };
  static const char * const backgrounds[] =
  {					/* Background colors for the supply-level bars */
    "#777 linear-gradient(#333,#777)",
    "#000 linear-gradient(#666,#000)",
    "#0FF linear-gradient(#6FF,#0FF)",
    "#F0F linear-gradient(#F6F,#F0F)",
    "#CC0 linear-gradient(#EE6,#EE0)"
  };
  static const char * const colors[] =	/* Text colors for the supply-level bars */
  {
    "#fff",
    "#fff",
    "#000",
    "#000",
    "#000"
  };


  if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  html_header(client, printer->name, 0);

  if ((supply = ippFindAttribute(printer->attrs, "printer-supply", IPP_TAG_STRING)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-supply defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  num_supply = ippGetCount(supply);

  if ((supply_desc = ippFindAttribute(printer->attrs, "printer-supply-description", IPP_TAG_TEXT)) == NULL)
  {
    html_printf(client, "<p>Error: No printer-supply-description defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if (num_supply != ippGetCount(supply_desc))
  {
    html_printf(client, "<p>Error: Different number of values for printer-supply and printer-supply-description defined for printer.</p>\n");
    html_footer(client);
    return (1);
  }

  if (printer->web_forms)
    num_options = parse_options(client, &options);

  if (num_options > 0)
  {
   /*
    * WARNING: A real printer/server implementation MUST NOT implement
    * supply updates via a GET request - GET requests are supposed to be
    * idempotent (without side-effects) and we obviously are not
    * authenticating access here.  This form is provided solely to
    * enable testing and development!
    */

    char	name[64];		/* Form field */
    const char	*val;			/* Form value */

    _cupsRWLockWrite(&printer->rwlock);

    ippDeleteAttribute(printer->attrs, supply);
    supply = NULL;

    printer->state_reasons &= (lprint_preason_t)~(LPRINT_PREASON_MARKER_SUPPLY_EMPTY | LPRINT_PREASON_MARKER_SUPPLY_LOW | LPRINT_PREASON_MARKER_WASTE_ALMOST_FULL | LPRINT_PREASON_MARKER_WASTE_FULL | LPRINT_PREASON_TONER_EMPTY | LPRINT_PREASON_TONER_LOW);

    for (i = 0; i < num_supply; i ++)
    {
      snprintf(name, sizeof(name), "supply%d", i);
      if ((val = cupsGetOption(name, num_options, options)) != NULL)
      {
        level = atoi(val);      /* New level */

        snprintf(supply_text, sizeof(supply_text), printer_supply[i], level);
        if (supply)
          ippSetOctetString(printer->attrs, &supply, ippGetCount(supply), supply_text, (int)strlen(supply_text));
        else
          supply = ippAddOctetString(printer->attrs, IPP_TAG_PRINTER, "printer-supply", supply_text, (int)strlen(supply_text));

        if (i == 0)
        {
          if (level == 100)
            printer->state_reasons |= LPRINT_PREASON_MARKER_WASTE_FULL;
          else if (level > 90)
            printer->state_reasons |= LPRINT_PREASON_MARKER_WASTE_ALMOST_FULL;
        }
        else
        {
          if (level == 0)
            printer->state_reasons |= LPRINT_PREASON_TONER_EMPTY;
          else if (level < 10)
            printer->state_reasons |= LPRINT_PREASON_TONER_LOW;
        }
      }
    }

    _cupsRWUnlock(&printer->rwlock);
  }

  if (printer->web_forms)
    html_printf(client, "<form method=\"GET\" action=\"/supplies\">\n");

  html_printf(client, "<table class=\"form\" summary=\"Supplies\">\n");
  for (i = 0; i < num_supply; i ++)
  {
    supply_value = ippGetOctetString(supply, i, &supply_len);
    if (supply_len > (int)(sizeof(supply_text) - 1))
      supply_len = (int)sizeof(supply_text) - 1;

    memcpy(supply_text, supply_value, (size_t)supply_len);
    supply_text[supply_len] = '\0';

    if ((supply_ptr = strstr(supply_text, "level=")) != NULL)
      level = atoi(supply_ptr + 6);
    else
      level = 50;

    if (printer->web_forms)
      html_printf(client, "<tr><th>%s:</th><td><input name=\"supply%d\" size=\"3\" value=\"%d\"></td>", ippGetString(supply_desc, i, NULL), i, level);
    else
      html_printf(client, "<tr><th>%s:</th>", ippGetString(supply_desc, i, NULL));

    if (level < 10)
      html_printf(client, "<td class=\"meter\"><span class=\"bar\" style=\"background: %s; padding: 5px %dpx;\"></span>&nbsp;%d%%</td></tr>\n", backgrounds[i], level * 2, level);
    else
      html_printf(client, "<td class=\"meter\"><span class=\"bar\" style=\"background: %s; color: %s; padding: 5px %dpx;\">%d%%</span></td></tr>\n", backgrounds[i], colors[i], level * 2, level);
  }

  if (printer->web_forms)
  {
    html_printf(client, "<tr><td></td><td colspan=\"2\"><input type=\"submit\" value=\"Update Supplies\">");
    if (num_options > 0)
      html_printf(client, " <span class=\"badge\" id=\"status\">Supplies updated.</span>\n");
    html_printf(client, "</td></tr>\n</table>\n</form>\n");

    if (num_options > 0)
      html_printf(client, "<script>\n"
			  "setTimeout(hide_status, 3000);\n"
			  "function hide_status() {\n"
			  "  var status = document.getElementById('status');\n"
			  "  status.style.display = 'none';\n"
			  "}\n"
			  "</script>\n");
  }
  else
    html_printf(client, "</table>\n");

  html_footer(client);

  return (1);
}


/*
 * 'time_string()' - Return the local time in hours, minutes, and seconds.
 */

static char *
time_string(time_t tv,			/* I - Time value */
            char   *buffer,		/* I - Buffer */
	    size_t bufsize)		/* I - Size of buffer */
{
  struct tm	date;			/* Local time and date */

  localtime_r(&tv, &date);

  strftime(buffer, bufsize, "%X", &date);

  return (buffer);
}
#endif // 0
