#ifndef HTTP_H
#define HTTP_H

#define MAX_HEADER_LINES 32
#define MAX_LINE_LENGTH 256

// Just the common ones
typedef enum HttpMethod {
  GET,
  POST,
  PUT,
  DELETE,
  PATCH
} HttpMethod;


/* Headers */
typedef struct HttpHeader {
  char *name;
  char *value;
} HttpHeader;

typedef struct HttpHeaderListItem {
  HttpHeader header;
  struct HttpHeaderListItem *next;
} HttpHeaderListItem;

typedef struct HttpHeaderList {
  int length;
  HttpHeaderListItem *head;
} HttpHeaderList;

typedef struct HttpRequest
{
  HttpMethod method;
  HttpHeaderList header_list;
  int content_length;
  void *body;
} HttpRequest;

HttpHeaderList create_header_list();
int parse_http_request(char *text, HttpRequest *request);
int add_header(HttpHeaderList *list, HttpHeader header);
int free_header_list(HttpHeaderList *list);

#endif
