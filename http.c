#include "http.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

int initalize_server(HttpServer *server)
{
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(PORT);

    if (bind(server->server_fd, (struct sockaddr*)&server->address, sizeof(server->address)) == -1) {
        return -1;
    }
    listen(server->server_fd, 10);

    return 0;
}

/**
 * Accepts an incoming message and attempts to parse it. If it fails, it will attempt to send back
 * a http error message, and then return -1
 */
int server_accept(HttpServer *server, HttpRequest *request)
{
    int client_fd = accept(server->server_fd, NULL, NULL);
    request->client_fd = client_fd;

    char buffer[MAX_MESSAGE_SIZE + 1]; // Adding 1 for string termination

    int message_length = recv(client_fd, buffer, MAX_MESSAGE_SIZE + 1, 0);

    if (message_length < 0) {
        printf("Failed to read message from client\n");
        return -1;
    }
    if (message_length > MAX_MESSAGE_SIZE) {
        printf("Client's message was too big\n");
        return -1;
    }

    buffer[message_length] = '\0';

    int status = parse_http_request(buffer, request);
    if (status != 0) {
        HttpResponse response = { 0 };
        response.body = NULL;

        HttpHeaderList response_headers = { 0 };
        response_headers.head = NULL;

        response.header_list = response_headers;
        response.status_code = status;
        response.reason_phrase = "Error";

        char response_str[MAX_MESSAGE_SIZE+1];
        if (http_response_to_string(&response, response_str) == -1) {
            return -1;
        }

        send(client_fd, response_str, strlen(response_str), 0); // Could fail
    
        return -1;
    }
    
    return 0;
}

int delete_server(HttpServer *server)
{
    return close(server->server_fd);
}

HttpHeaderList create_header_list() 
{
    HttpHeaderList list = { 0 };
    list.length = 0;
    list.head = NULL;

    return list;
}

int add_header(HttpHeaderList *list, char *key, char *value) 
{
    HttpHeader header = { 0 };
    header.name = key;
    header.value = value;

    HttpHeaderListItem *wrapper = (HttpHeaderListItem*)malloc(sizeof(HttpHeaderListItem));
    wrapper->header = header;
    wrapper->next = list->head;

    list->head = wrapper;
    list->length++;

    return 0;
}

int free_header_list(HttpHeaderList *list) 
{
    while (list->head != NULL) {
        HttpHeaderListItem *item = list->head;
        list->head = list->head->next;
        free(item->header.name);
        free(item->header.value);
        free(item);
    }

    return 0;
}

int free_http_request(HttpRequest *request)
{
    free_header_list(&request->header_list);
    free(request->method);
    free(request->target);
    free(request->body);

    return 0;
}

/**
 * Parses a string message into a HttpRequest.
 * 
 * Returns
 * - 0 on success
 * - Http status code response on error
 * 
 * THIS FUNCTION WILL MUTATE THE TEXT PARAMETER
 */
int parse_http_request(char *text, HttpRequest *request)
{
    char method[MAX_LINE_LENGTH];
    char target[MAX_LINE_LENGTH];
    char http_version[MAX_LINE_LENGTH];

    HttpHeaderList header_list = create_header_list();

    int line_number = 0;
    char *line = strtok(text, "\r\n");

    while (line != NULL) {
        if (line_number == 0) {
            if (sscanf(line, "%s %s %s", method, target, http_version) != 3) {
               return 400;
            }
        } else {
            if (strlen(line) == 0) {
                char *rest_of_message = strtok(NULL, "");
                request->body = strdup(rest_of_message);
                break;
            }

            char *key = (char*)malloc(MAX_LINE_LENGTH);
            char *value = (char*)malloc(MAX_LINE_LENGTH);

            if (sscanf(line, " %[^:]: %s\r\n", key, value) != 2) {
                return -1;
            }

            add_header(&header_list, key, value);
        }

        // strtok has internal state. Null makes it go to the next token
        line = strtok(NULL, "\r\n");
        line_number++;
    }

    request->method = strdup(method);
    request->target = strdup(target);

    request->header_list = header_list;

    return 0;
}

/**
 * Turn a http response struct into a string that can be returned
 * 
 * The content length header will be automatically appended to the end
 */
// TODO: This could write off the end of the response string
int http_response_to_string(HttpResponse *response, char *result)
{
    if (response->status_code < 100 || response->status_code > 599) {
        return -1; // Status code is outside http code range (100-599)
    }
    result[0] = '\0'; // Otherwise the string might not be initialized, breaking strlen calls later

    // First line
    snprintf(
        result + strlen(result), 
        MAX_LINE_LENGTH, 
        "HTTP/1.1 %d %s\r\n", 
        response->status_code, 
        response->reason_phrase
    );

    // Headers
    HttpHeaderListItem *current = response->header_list.head;
    while (current != NULL) {
        snprintf(
            result + strlen(result),
            MAX_LINE_LENGTH,
            "%s: %s\r\n",
            current->header.name,
            current->header.value
        );
        current = current->next;
    }

    // Automatically add on the content length header
    int content_length = response->body == NULL ? 0 : strlen(response->body);
    snprintf(
        result + strlen(result),
        MAX_LINE_LENGTH,
        "Content-Length: %d\r\n",
        content_length
    );


    // End headers with empty line
    snprintf(
        result + strlen(result), 
        MAX_LINE_LENGTH, 
        "\r\n"
    );

    // Body if it exists
    if (response->body != NULL) {
        snprintf(
            result + strlen(result), 
            MAX_LINE_LENGTH, 
            "%s",
            response->body
        );
    }

    return 0;
}
