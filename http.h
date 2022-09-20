#include "event.h"

int http_response(struct ntyevent *ev)
{
	if (ev == NULL)
		return -1;

	memset(ev->buffer, 0, BUFFER_LENGTH);

#if 1
	const char *html =
		"<html><head><title>webserver<title/><head/><body><h1>reactor web server<h1/><body/><html/>";

	ev->length = sprintf(ev->buffer, "HTTP/1.1 200 OK\r\n\
         Date: Sun, 11 Sep 2022 11:24:51 GMT\r\n\
         Content-Type: text/html; charset=utf-8\r\n\
         Content-Length: %ld\r\n\r\n",
			     strlen(html), html);

#elif 0

	const char *html =
		"<html><head><title>hello http</title></head><body><H1>King</H1></body></html>\r\n\r\n";

	ev->length = sprintf(ev->buffer, "HTTP/1.1 200 OK\r\n\
		 Date: Thu, 11 Nov 2021 12:28:52 GMT\r\n\
		 Content-Type: text/html;charset=ISO-8859-1\r\n\
		 Content-Length: 83\r\n\r\n%s",
			     html);
#endif

	return ev->length;
}

int http_request(struct ntyevent *ev)
{
	return 0;
}
