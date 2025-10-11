#ifndef HTTP_H
#define HTTP_H

#include <netinet/in.h>

#define MAX_HEADER_LINES 32
#define MAX_LINE_LENGTH 256
#define PORT 80
#define MAX_MESSAGE_SIZE 2048
#define MAX_BODY_SIZE 2048

/**
 * Server
 */

typedef struct HttpServer {
  int server_fd;
  struct sockaddr_in address;
} HttpServer;


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
  int client_fd;
  char *method; // This should be an enum eventually
  char *target;
  HttpHeaderList header_list;
  void *body;
} HttpRequest;

typedef struct HttpResponse
{
  int status_code;
  char *reason_phrase;
  HttpHeaderList header_list;
  char *body;
} HttpResponse;

HttpHeaderList create_header_list();
int parse_http_request(char *text, HttpRequest *request);
int add_header(HttpHeaderList *list, char *key, char *value);
int free_header_list(HttpHeaderList *list);
int http_response_to_string(HttpResponse *response, char *result);
int initalize_server(HttpServer *server);
int server_accept(HttpServer *server, HttpRequest *request);
int delete_server(HttpServer *server);
int delete_request(HttpRequest *request);

#endif
