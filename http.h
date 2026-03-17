#ifndef HTTP_H
#define HTTP_H

#include <netinet/in.h>

#define MAX_HEADER_LINES 32
#define MAX_LINE_LENGTH 256
#define PORT 80
#define MAX_REQUEST_SIZE 65536
#define MAX_METHOD_SIZE 8
#define MAX_URI_LENGTH 256
#define MAX_HEADER_KEY_LENGTH 256
#define MAX_HEADER_VALUE_LENGTH 512

enum RequestState {
    LOOKING_FOR_METHOD,
    LOOKING_FOR_URI,
    LOOKING_FOR_VERSION,
    LOOKING_FOR_HEADER_KEY,
    LOOKING_FOR_HEADER_VALUE,
    LOOKING_FOR_DATA,
    DONE_READING_REQUEST,
};

enum HttpMethod {
    GET,
    POST,
};

struct HttpRequest {
    char buffer[MAX_REQUEST_SIZE];
    int buffer_offset;
    enum RequestState state;

    enum HttpMethod method;
    char uri[MAX_URI_LENGTH];

    struct HttpHeader *headers;
};

struct HttpHeader {
    char key[MAX_HEADER_KEY_LENGTH];
    char value[MAX_HEADER_VALUE_LENGTH];
    struct HttpHeader *next;
};

#endif
