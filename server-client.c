//
// Client processing code for LPrint, a Label Printer Application
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
#include "static-resources/lprint-png.h"
#include "static-resources/lprint-large-png.h"
#include "static-resources/lprint-de-strings.h"
#include "static-resources/lprint-en-strings.h"
#include "static-resources/lprint-es-strings.h"
#include "static-resources/lprint-fr-strings.h"
#include "static-resources/lprint-it-strings.h"
#include <ctype.h>


//
// Local types...
//

typedef struct lprint_resource_s	// Resource data
{
  const char	*path,			// Resource path
		*content_type;		// Content-Type value
  const void	*data;			// Pointer to resource data
  size_t	length;			// Size of resource
} lprint_resource_t;


//
// Local functions...
//

static int		device_cb(const char *device_uri, lprint_client_t *client);
static int		get_form_data(lprint_client_t *client, cups_option_t **form);
static void		html_escape(lprint_client_t *client, const char *s, size_t slen);
static void		html_footer(lprint_client_t *client);
static void		html_header(lprint_client_t *client, const char *title, int refresh);
static void		html_printf(lprint_client_t *client, const char *format, ...) LPRINT_FORMAT(2, 3);
static int		show_add(lprint_client_t *client);
static int		show_default(lprint_client_t *client, int printer_id);
static int		show_delete(lprint_client_t *client, int printer_id);
static int		show_modify(lprint_client_t *client, int printer_id);
static int		show_status(lprint_client_t *client);
static char		*time_string(time_t tv, char *buffer, size_t bufsize);


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

  pthread_rwlock_wrlock(&system->rwlock);
  client->number = system->next_client ++;
  pthread_rwlock_unlock(&system->rwlock);

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
  int			i;		// Looping var
  const lprint_resource_t *resource;	// Current resource
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
  const lprint_resource_t	resources[] =
  {
    { "/lprint-de.strings",	"text/strings",	lprint_de_strings, 0 },
    { "/lprint-en.strings",	"text/strings",	lprint_en_strings, 0 },
    { "/lprint-es.strings",	"text/strings",	lprint_es_strings, 0 },
    { "/lprint-fr.strings",	"text/strings",	lprint_fr_strings, 0 },
    { "/lprint-it.strings",	"text/strings",	lprint_it_strings, 0 },
    { "/lprint.png",		"image/png",	lprint_png, sizeof(lprint_png) },
    { "/lprint-large.png",	"image/png",	lprint_large_png, sizeof(lprint_large_png) }
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
	if (!strcmp(client->uri, "/"))
	  return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0));
	else if (client->system->auth_service && (!strcmp(client->uri, "/add") || !strncmp(client->uri, "/default/", 9) || !strncmp(client->uri, "/delete/", 8) || !strncmp(client->uri, "/modify/", 8)))
	  return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0));

        for (i = (int)(sizeof(resources) / sizeof(resources[0])), resource = resources; i > 0; i --, resource ++)
        {
          if (!strcmp(resource->path, client->uri))
          {
	    return (lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, resource->content_type, 0));
          }
        }

	return (lprintRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_GET :
	if (!strcmp(client->uri, "/"))
	{
	  // Show web status page...
          return (show_status(client));
	}
	else if (client->system->auth_service)
	{
	  // Show management pages?
	  if (!strcmp(client->uri, "/add"))
	    return (show_add(client));
	  else if (!strncmp(client->uri, "/default/", 9))
	    return (show_default(client, atoi(client->uri + 9)));
	  else if (!strncmp(client->uri, "/delete/", 8))
	    return (show_delete(client, atoi(client->uri + 8)));
	  else if (!strncmp(client->uri, "/modify/", 8))
	    return (show_modify(client, atoi(client->uri + 8)));
	}

        for (i = (int)(sizeof(resources) / sizeof(resources[0])), resource = resources; i > 0; i --, resource ++)
        {
          if (!strcmp(resource->path, client->uri))
          {
	    // Send resource file...
	    size_t length;		// Length

            if ((length = resource->length) == 0)
              length = strlen((char *)resource->data);

	    if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, resource->content_type, length))
	      return (0);

	    httpWrite2(client->http, (const char *)resource->data, length);
	    httpFlushWrite(client->http);
	    return (1);
          }
	}

	return (lprintRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_POST :
	if (client->system->auth_service)
	{
	  // Process management pages?
	  if (!strcmp(client->uri, "/add"))
	    return (show_add(client));
	  else if (!strncmp(client->uri, "/default/", 9))
	    return (show_default(client, atoi(client->uri + 9)));
	  else if (!strncmp(client->uri, "/delete/", 8))
	    return (show_delete(client, atoi(client->uri + 8)));
	  else if (!strncmp(client->uri, "/modify/", 8))
	    return (show_modify(client, atoi(client->uri + 8)));
	}

	if (strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE), "application/ipp"))
        {
	  // Not an IPP request...
	  return (lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
	}

        // Read the IPP request...
	client->request = ippNew();

        while ((ipp_state = ippRead(client->http, client->request)) != IPP_STATE_DATA)
	{
	  if (ipp_state == IPP_STATE_ERROR)
	  {
            lprintLogClient(client, LPRINT_LOGLEVEL_ERROR, "IPP read error (%s).", cupsLastErrorString());
	    lprintRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
	    return (0);
	  }
	}

        // Now that we have the IPP request, process the request...
        return (lprintProcessIPP(client));

    default :
        break; // Anti-compiler-warning-code
  }

  return (1);
}


//
// 'lprintRespondHTTP()' - Send a HTTP response.
//

int					// O - 1 on success, 0 on failure
lprintRespondHTTP(
    lprint_client_t *client,		// I - Client
    http_status_t code,			// I - HTTP status of response
    const char    *content_encoding,	// I - Content-Encoding of response
    const char    *type,		// I - MIME media type of response
    size_t        length)		// I - Length of response
{
  char	message[1024];			// Text message


  lprintLogClient(client, LPRINT_LOGLEVEL_INFO, "%s %s %d", httpStatus(code), type, (int)length);

  if (code == HTTP_STATUS_CONTINUE)
  {
    // 100-continue doesn't send any headers...
    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE) == 0);
  }

  // Format an error message...
  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

  // Send the HTTP response header...
  httpClearFields(client->http);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED || client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (code == HTTP_STATUS_UNAUTHORIZED)
    httpSetField(client->http, HTTP_FIELD_WWW_AUTHENTICATE, "Basic realm=\"LPrint\"");

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

  // Send the response data...
  if (message[0])
  {
    // Send a plain text message.
    if (httpPrintf(client->http, "%s", message) < 0)
      return (0);

    if (httpWrite2(client->http, "", 0) < 0)
      return (0);
  }
  else if (client->response)
  {
    // Send an IPP response...
    lprintLogAttributes(client, "Response", client->response, 2);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (0);
  }

  return (1);
}


//
// 'device_cb()' - Device callback.
//

static int				// O - 1 to continue
device_cb(const char      *device_uri,	// I - Device URI
          lprint_client_t *client)	// I - Client
{
  char	scheme[32],			// URI scheme
	userpass[32],			// Username/password (unused)
	make[64],			// Make from URI
	model[256],			// Model from URI
	*serial;			// Pointer to serial number
  int	port;				// Port number (unused)


  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), make, sizeof(make), &port, model, sizeof(model)) >= HTTP_URI_STATUS_OK)
  {
    if ((serial = strstr(model, "?serial=")) != NULL)
    {
      *serial = '\0';
      serial += 8;
    }

    if (serial)
      html_printf(client, "<option value=\"%s\">%s %s (%s)</option>", device_uri, make, model, serial);
    else
      html_printf(client, "<option value=\"%s\">%s %s</option>", device_uri, make, model);
  }

  return (1);
}


//
// 'get_form_data()' - Get POST form data from the web client.
//

static int				// O - Number of form variables read
get_form_data(lprint_client_t *client,	// I - Client
              cups_option_t   **form)	// O - Form variables
{
  int		num_form = 0;		// Number of form variables
  char		body[8192],		// Message body data string
		*ptr,			// Pointer into string
		name[64],		// Variable name
		*nameptr,		// Pointer into name
		value[1024],		// Variable value
		*valptr;		// Pointer into value
  ssize_t	bytes;			// Bytes read
  http_state_t	initial_state;		// Initial HTTP state


  // Read form data...
  initial_state = httpGetState(client->http);

  for (ptr = body; ptr < (body + sizeof(body) - 1); ptr += bytes)
  {
    if ((bytes = httpRead2(client->http, ptr, sizeof(body) - (size_t)(ptr - body + 1))) <= 0)
      break;
  }

  *ptr = '\0';

  if (httpGetState(client->http) == initial_state)
    httpFlush(client->http);		// Flush remainder...

  // Parse the form data...
  *form = NULL;

  for (ptr = body; *ptr;)
  {
    // Get the name...
    nameptr = name;
    while (*ptr && *ptr != '=')
    {
      int ch = *ptr++;			// Name character

      if (ch == '%' && isxdigit(ptr[0] & 255) && isxdigit(ptr[1] & 255))
      {
        // Hex-encoded character
        if (isdigit(*ptr))
          ch = (*ptr++ - '0') << 4;
	else
	  ch = (tolower(*ptr++) - 'a' + 10) << 4;

        if (isdigit(*ptr))
          ch |= *ptr++ - '0';
	else
	  ch |= tolower(*ptr++) - 'a' + 10;
      }
      else if (ch == '+')
        ch = ' ';

      if (nameptr < (name + sizeof(name) - 1))
        *nameptr++ = ch;
    }
    *nameptr = '\0';

    if (!*ptr)
      break;

    // Get the value...
    ptr ++;
    valptr = value;
    while (*ptr && *ptr != '&')
    {
      int ch = *ptr++;			// Name character

      if (ch == '%' && isxdigit(ptr[0] & 255) && isxdigit(ptr[1] & 255))
      {
        // Hex-encoded character
        if (isdigit(*ptr))
          ch = (*ptr++ - '0') << 4;
	else
	  ch = (tolower(*ptr++) - 'a' + 10) << 4;

        if (isdigit(*ptr))
          ch |= *ptr++ - '0';
	else
	  ch |= tolower(*ptr++) - 'a' + 10;
      }
      else if (ch == '+')
        ch = ' ';

      if (valptr < (value + sizeof(value) - 1))
        *valptr++ = ch;
    }
    *valptr = '\0';

    if (*ptr)
      ptr ++;

    // Add the name + value to the option array...
    num_form = cupsAddOption(name, value, num_form, form);
  }

  return (num_form);
}


//
// 'html_escape()' - Write a HTML-safe string.
//

static void
html_escape(lprint_client_t *client,	// I - Client
	    const char    *s,		// I - String to write
	    size_t        slen)		// I - Number of characters to write
{
  const char	*start,			// Start of segment
		*end;			// End of string


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


//
// 'html_footer()' - Show the web interface footer.
//
// This function also writes the trailing 0-length chunk.
//

static void
html_footer(lprint_client_t *client)	// I - Client
{
  html_printf(client,
	      "</div>\n"
	      "</body>\n"
	      "</html>\n");
  httpWrite2(client->http, "", 0);
}


//
// 'html_header()' - Show the web interface header and title.
//

static void
html_header(lprint_client_t *client,	// I - Client
            const char    *title,	// I - Title
            int           refresh)	// I - Refresh timer, if any
{
  html_printf(client,
	      "<!doctype html>\n"
	      "<html>\n"
	      "<head>\n"
	      "<title>%s%sLPrint v" LPRINT_VERSION "</title>\n"
	      "<link rel=\"shortcut icon\" href=\"/lprint.png\" type=\"image/png\">\n"
	      "<link rel=\"apple-touch-icon\" href=\"/lprint.png\" type=\"image/png\">\n"
	      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\">\n", title, title ? " - " : "");
  if (refresh > 0)
    html_printf(client, "<meta http-equiv=\"refresh\" content=\"%d\">\n", refresh);
  html_printf(client,
	      "<meta name=\"viewport\" content=\"width=device-width\">\n"
	      "<style>\n"
	      "body { font-family: sans-serif; margin: 10px 20px; }\n"
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
	      "</style>\n"
	      "</head>\n"
	      "<body>\n"
	      "<h1>%s</h1>\n", title);
}


//
// 'html_printf()' - Send formatted text to the client, quoting as needed.
//

static void
html_printf(lprint_client_t *client,	// I - Client
	    const char    *format,	// I - Printf-style format string
	    ...)			// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments
  const char	*start;			// Start of string
  char		size,			// Size character (h, l, L)
		type;			// Format type character
  int		width,			// Width of field
		prec;			// Number of characters of precision
  char		tformat[100],		// Temporary format string for sprintf()
		*tptr,			// Pointer into temporary format
		temp[1024];		// Buffer for formatted numbers
  char		*s;			// Pointer to string


  // Loop through the format string, formatting as needed...
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
        // Get width from argument...
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
          // Get precision from argument...
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
	case 'E' : // Floating point formats
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, double));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'B' : // Integer formats
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
#  endif // HAVE_LONG_LONG
            if (size == 'l')
	      sprintf(temp, tformat, va_arg(ap, long));
	    else
	      sprintf(temp, tformat, va_arg(ap, int));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

	case 'p' : // Pointer value
	    if ((size_t)(width + 2) > sizeof(temp))
	      break;

	    sprintf(temp, tformat, va_arg(ap, void *));

            httpWrite2(client->http, temp, strlen(temp));
	    break;

        case 'c' : // Character or character array
            if (width <= 1)
            {
              temp[0] = (char)va_arg(ap, int);
              temp[1] = '\0';
              html_escape(client, temp, 1);
            }
            else
              html_escape(client, va_arg(ap, char *), (size_t)width);
	    break;

	case 's' : // String
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


//
// 'show_add()' - Show the add printer page.
//

static int				// O - 1 on success, 0 on failure
show_add(lprint_client_t *client)	// I - Client connection
{
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*printer_name = NULL,	// Printer name
		*lprint_driver = NULL,	// Driver name
		*device_uri = NULL,	// Device URI
		*socket_address = NULL,	// Socket device address
		*ptr,			// Pointer into value
		*error = NULL;		// Error message, if any
  int		i,			// Looping var
		num_drivers;		// Number of drivers in list
  const char * const *drivers;		// Driver list


  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (lprintRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int	valid = 1;

    num_form       = get_form_data(client, &form);
    session        = cupsGetOption("session-key", num_form, form);
    printer_name   = cupsGetOption("printer-name", num_form, form);
    lprint_driver  = cupsGetOption("lprint-driver", num_form, form);
    device_uri     = cupsGetOption("device-uri", num_form, form);
    socket_address = cupsGetOption("socket-address", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid && printer_name)
    {
      if (!*printer_name)
      {
	valid = 0;
	error = "Empty printer name.";
      }
      else if (strlen(printer_name) > 127)
      {
        valid = 0;
        error = "Printer name too long.";
      }

      for (ptr = printer_name; valid && *ptr; ptr ++)
      {
        if (!isalnum(*ptr & 255) && *ptr != '.' && *ptr != '-')
        {
          valid = 0;
          error = "Bad printer name - use only letters, numbers, '.', and '-'.";
          break;
        }
      }

      if (valid)
      {
        char	resource[1024];		// Resource path for printer

        snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);
        if (lprintFindPrinter(client->system, resource, 0))
        {
          valid = 0;
          error = "A printer with that name already exists.";
	}
      }
    }

    if (valid && lprint_driver && !lprintGetMakeAndModel(lprint_driver))
    {
      valid = 0;
      error = "Bad driver.";
    }

    if (device_uri && strncmp(device_uri, "usb://", 6))
    {
      if (strcmp(device_uri, "socket"))
      {
        valid = 0;
        error = "Bad device.";
      }
      else if (!socket_address || !*socket_address)
      {
        valid = 0;
        error = "Bad network address.";
      }
    }

    if (valid)
    {
      // Add the printer...
      lprint_printer_t	*printer;	// Printer
      char		uri[1024];	// Socket URI

      if (!strcmp(device_uri, "socket"))
      {
        httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "socket", NULL, socket_address, 9100, "/");
        device_uri = uri;
      }

      printer = lprintCreatePrinter(client->system, 0, printer_name, lprint_driver, device_uri, NULL, NULL, NULL, NULL);

      if (printer)
      {
	if (!client->system->save_time)
	  client->system->save_time = time(NULL) + 1;

	lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

	html_header(client, "Printer Added", 0);
	html_printf(client, "<p><button onclick=\"window.location.href='/';\">Return to Printers</button> <button onclick=\"window.location.href='/modify/%d';\">Modify Printer</button></p>\n", printer->printer_id);
	html_footer(client);
	return (1);
      }
      else
        error = "Printer creation failed.";
    }
  }

  lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  html_header(client, "Add Printer", 0);
  html_printf(client, "<p><button onclick=\"window.location.href='/';\">Return to Printers</button></p>\n");

  if (error)
    html_printf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  html_printf(client, "<form method=\"POST\" action=\"/add\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n"
                      "<tr><th>Name:</th><td><input name=\"printer-name\" value=\"%s\" size=\"32\" placeholder=\"Letters, numbers, '.', and '-'.\"></td></tr>\n"
                      "<tr><th>Device:</th><td><select name=\"device-uri\">", printer_name ? printer_name : "", client->system->session_key);
  lprintListDevices((lprint_device_cb_t)device_cb, NULL, NULL, NULL);
  html_printf(client, "<option value=\"socket\">Network Printer</option></select><br>\n"
                      "<input name=\"socket-address\" value=\"%s\" size=\"32\" placeholder=\"IP address or hostname\"></td></tr>\n", socket_address ? socket_address : "");
  html_printf(client, "<tr><th>Driver:</th><td><select name=\"lprint-driver\">");
  drivers = lprintGetDrivers(&num_drivers);
  for (i = 0; i < num_drivers; i ++)
  {
    html_printf(client, "<option value=\"%s\"%s>%s</option>", drivers[i], (lprint_driver && !strcmp(drivers[i], lprint_driver)) ? " selected" : "", lprintGetMakeAndModel(drivers[i]));
  }
  html_printf(client, "</select></td></tr>\n");
  html_printf(client, "<tr><th></th><td><input type=\"submit\" value=\"Add Printer\"></td></tr>\n"
                      "</table></form>\n");
  html_footer(client);

  cupsFreeOptions(num_form, form);

  return (1);
}


//
// 'show_default()' - Show the set default printer page.
//

static int				// O - 1 on success, 0 on failure
show_default(lprint_client_t *client,	// I - Client connection
             int             printer_id)// I - Printer ID
{
  return (1);
}


//
// 'show_delete()' - Show the delete printer page.
//

static int				// O - 1 on success, 0 on failure
show_delete(lprint_client_t *client,	// I - Client connection
            int             printer_id)	// I - Printer ID
{
  return (1);
}


//
// 'show_modify()' - Show the modify printer page.
//

static int				// O - 1 on success, 0 on failure
show_modify(lprint_client_t *client,	// I - Client connection
	    int             printer_id)	// I - Printer ID
{
  lprint_printer_t *printer;		// Printer
  http_status_t	status;			// Authorization status
  int		num_form = 0;		// Number of form variables
  cups_option_t	*form = NULL;		// Form variables
  const char	*session = NULL,	// Session key
		*location = NULL,	// Human-readable location
		*latitude = NULL,	// Latitude
		*longitude = NULL,	// Longitude
		*organization = NULL,	// Organization
		*org_unit = NULL,	// Organizational unit
//		*ptr,			// Pointer into value
		*error = NULL;		// Error message, if any
//  int		i;			// Looping var
  char		title[1024];		// Title for page
  float		latval = 0.0f,		// Latitude in degrees
		lonval = 0.0f;		// Longitude in degrees


  if ((printer = lprintFindPrinter(client->system, NULL, printer_id)) == NULL)
  {
    // Printer not found...
    return (lprintRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));
  }

  if ((status = lprintIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
  {
    // Need authentication...
    return (lprintRespondHTTP(client, status, NULL, NULL, 0));
  }

  if (client->operation == HTTP_STATE_POST)
  {
    // Get form data...
    int		valid = 1;		// Is form data valid?

    num_form     = get_form_data(client, &form);
    session      = cupsGetOption("session-key", num_form, form);
    location     = cupsGetOption("printer-location", num_form, form);
    latitude     = cupsGetOption("latitude", num_form, form);
    longitude    = cupsGetOption("longitude", num_form, form);
    organization = cupsGetOption("printer-organization", num_form, form);
    org_unit     = cupsGetOption("printer-organizational-unit", num_form, form);

    if (!session || strcmp(session, client->system->session_key))
    {
      valid = 0;
      error = "Bad or missing session key.";
    }

    if (valid && latitude)
    {
      latval = atof(latitude);

      if (*latitude && (!strchr("0123456789.-+", *latitude) || latval < -90.0f || latval > 90.0f))
      {
        valid = 0;
        error = "Bad latitude value.";
      }
    }

    if (valid && longitude)
    {
      lonval = atof(longitude);

      if (*longitude && (!strchr("0123456789.-+", *longitude) || lonval < -180.0f || lonval > 180.0f))
      {
        valid = 0;
        error = "Bad longitude value.";
      }
    }

    if (valid && latitude && longitude && !*latitude != !*longitude)
    {
      valid = 0;
      error = "Both latitude and longitude must be specified.";
    }

    if (valid)
    {
      pthread_rwlock_wrlock(&printer->rwlock);

      if (location)
      {
        free(printer->location);
        printer->location = strdup(location);
      }

      if (latitude && *latitude && longitude && *longitude)
      {
        char geo[1024];			// geo: URI

        snprintf(geo, sizeof(geo), "geo:%g,%g", atof(latitude), atof(longitude));
        free(printer->geo_location);
        printer->geo_location = strdup(geo);
      }
      else if (latitude && longitude)
      {
        free(printer->geo_location);
        printer->geo_location = NULL;
      }

      if (organization)
      {
        free(printer->organization);
        printer->organization = strdup(organization);
      }

      if (org_unit)
      {
        free(printer->org_unit);
        printer->org_unit = strdup(org_unit);
      }

      printer->config_time = time(NULL);

      pthread_rwlock_unlock(&printer->rwlock);

      if (!client->system->save_time)
	client->system->save_time = time(NULL) + 1;

      lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

      html_header(client, "Printer Modified", 0);
      html_printf(client, "<p><button onclick=\"window.location.href='/';\">Return to Printers</button></p>\n");
      html_footer(client);
      return (1);
    }
  }

  if (!location)
    location = printer->location;

  if (latitude && longitude)
  {
    latval = atof(latitude);
    lonval = atof(longitude);
  }
  else if (printer->geo_location)
    sscanf(printer->geo_location, "geo:%f,%f", &latval, &lonval);

  if (!organization)
    organization = printer->organization;

  if (!org_unit)
    org_unit = printer->org_unit;

  lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0);

  snprintf(title, sizeof(title), "Modify Printer %s", printer->printer_name);
  html_header(client, title, 0);
  html_printf(client, "<p><button onclick=\"window.location.href='/';\">Return to Printers</button></p>\n");

  if (error)
    html_printf(client, "<blockquote><em>Error:</em> %s</blockquote>\n", error);

  html_printf(client, "<form method=\"POST\" action=\"/modify/%d\">"
                      "<input name=\"session-key\" type=\"hidden\" value=\"%s\">"
		      "<table class=\"form\">\n", printer_id, client->system->session_key);
  html_printf(client, "<tr><th>Location:</th><td><input name=\"printer-location\" value=\"%s\" size=\"32\" placeholder=\"Human-readable location\"><br>\n"
                      "<input name=\"latitude\" type=\"number\" value=\"%g\" min=\"-90\" max=\"90\" step=\"0.000001\" size=\"10\" placeholder=\"Latitude Degrees\">"
                      "<input name=\"longitude\" type=\"number\" value=\"%g\" min=\"-180\" max=\"180\" step=\"0.000001\" size=\"11\" placeholder=\"Longitude Degrees\">"
                      "</td></tr>\n", location ? location : "", latval, lonval);
  html_printf(client, "<tr><th>Organization:</th><td><input name=\"printer-organization\" value=\"%s\" size=\"32\" placeholder=\"Organization name\"></td></tr>\n", organization ? organization : "");
  html_printf(client, "<tr><th>Organizational Unit:</th><td><input name=\"printer-organizational-unit\" value=\"%s\" size=\"32\" placeholder=\"Unit/division/group\"></td></tr>\n", org_unit ? org_unit : "");
  html_printf(client, "<tr><th></th><td><input type=\"submit\" value=\"Modify Printer\"></td></tr>\n"
                      "</table></form>\n");
  html_footer(client);

  cupsFreeOptions(num_form, form);

  return (1);
}


//
// 'show_status()' - Show printer/system state.
//

static int				// O - 1 on success, 0 on failure
show_status(lprint_client_t  *client)	// I - Client connection
{
  lprint_system_t	*system = client->system;
					// System
  lprint_printer_t	*printer;	// Printer
  lprint_job_t		*job;		// Current job
  int			i;		// Looping var
  lprint_preason_t	reason;		// Current reason
  static const char * const reasons[] =	// Reason strings
  {
    "Other",
    "Cover Open",
    "Media Empty",
    "Media Jam",
    "Media Low",
    "Media Needed"
  };
  static const char * const state_colors[] =
  {					// State colors
    "#0C0",				// Idle
    "#EE0",				// Processing
    "#C00"				// Stopped
  };


  if (!lprintRespondHTTP(client, HTTP_STATUS_OK, NULL, "text/html", 0))
    return (0);

  pthread_rwlock_rdlock(&system->rwlock);

  for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    if (printer->state == IPP_PSTATE_PROCESSING)
      break;
  }

  html_header(client, "Printers", printer ? 5 : 15);

  if (client->system->auth_service)
    html_printf(client, "<p><button onclick=\"window.location.href='/add';\">Add Printer</button></p>\n");

  for (printer = (lprint_printer_t *)cupsArrayFirst(system->printers); printer; printer = (lprint_printer_t *)cupsArrayNext(system->printers))
  {
    html_printf(client, "<h2><img style=\"background: %s; border-radius: 10px; float: left; margin-right: 10px; padding: 5px;\" src=\"/lprint-large.png\" width=\"64\" height=\"64\">%s%s</h2>\n", state_colors[printer->state - IPP_PSTATE_IDLE], printer->printer_name, printer->printer_id == client->system->default_printer ? " (Default)" : "");
    html_printf(client, "<p>%s, %d job(s).", printer->state == IPP_PSTATE_IDLE ? "Idle" : printer->state == IPP_PSTATE_PROCESSING ? "Printing" : "Stopped", cupsArrayCount(printer->jobs));
    for (i = 0, reason = 1; i < (int)(sizeof(reasons) / sizeof(reasons[0])); i ++, reason <<= 1)
      if (printer->state_reasons & reason)
	html_printf(client, "\n<br>&nbsp;&nbsp;&nbsp;&nbsp;%s", reasons[i]);
    if (client->system->auth_service)
    {
      html_printf(client, "\n<br><button onclick=\"window.location.href='/modify/%d';\">Modify</button> <button onclick=\"window.location.href='/delete/%d';\">Delete</button>", printer->printer_id, printer->printer_id);
      if (printer->printer_id != client->system->default_printer)
        html_printf(client, " <button onclick=\"window.location.href='/default/%d';\">Set As Default</button>", printer->printer_id);
    }
    html_printf(client, "</p>\n");

    if (cupsArrayCount(printer->jobs) > 0)
    {
      pthread_rwlock_rdlock(&printer->rwlock);

      html_printf(client, "<table class=\"striped\" summary=\"Jobs\"><thead><tr><th>Job #</th><th>Name</th><th>Owner</th><th>Status</th></tr></thead><tbody>\n");
      for (job = (lprint_job_t *)cupsArrayFirst(printer->jobs); job; job = (lprint_job_t *)cupsArrayNext(printer->jobs))
      {
	char	when[256],		// When job queued/started/finished
		hhmmss[64];		// Time HH:MM:SS

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

      pthread_rwlock_unlock(&printer->rwlock);
    }
  }

  html_footer(client);

  pthread_rwlock_unlock(&system->rwlock);

  return (1);
}


//
// 'time_string()' - Return the local time in hours, minutes, and seconds.
//

static char *
time_string(time_t tv,			// I - Time value
            char   *buffer,		// I - Buffer
	    size_t bufsize)		// I - Size of buffer
{
  struct tm	date;			// Local time and date

  localtime_r(&tv, &date);

  strftime(buffer, bufsize, "%X", &date);

  return (buffer);
}
