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

int free_server(HttpServer *server)
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

int free_header_list_item(HttpHeaderListItem *item)
{
    free(item->header.name);
    free(item->header.value);
    free(item);

    return 0;
}

int free_header_list(HttpHeaderList *list) 
{
    while (list->head != NULL) {
        HttpHeaderListItem *tmp_header = list->head;
        list->head = list->head->next;
        free_header_list_item(tmp_header);
    }

    return 0;
}

int free_http_request(HttpRequest *request)
{
    free_header_list(&request->header_list);
    free(request->method);
    free(request->target);

    // Free NULL does nothing, so this is fine
    free(request->body);

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

        char *response_str = http_response_to_string(&response);
        if (response_str == NULL) return -1;

        send(client_fd, response_str, strlen(response_str), 0); // Could fail
        free(response_str);
    
        return -1;
    }
    
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
    HttpHeaderList header_list = create_header_list();

    char *line_cursor;
    char *line;

    /**
     * Read first line
     */

    // strtok_r needs the source string to get started. Null makes it progress
    line = strtok_r(text, "\r\n", &line_cursor);

    char *request_line_cursor;

    char *method_pointer = strtok_r(line, " ", &request_line_cursor);
    if (method_pointer == NULL) return 400;
    request->method = strdup(method_pointer);

    char *target_pointer = strtok_r(NULL, " ", &request_line_cursor);
    if (target_pointer == NULL) return 400;
    if (strcmp(target_pointer, "HTTP/1.1") == 0) return 400;
    request->target = strdup(target_pointer);

    char *http_version = strtok_r(NULL, "", &request_line_cursor);
    if (http_version == NULL) return 400;
    http_version = strdup(http_version);

    /**
     * Read headers
     */

    line = strtok_r(NULL, "\r\n", &line_cursor);

    while (line != NULL) {
        if (strlen(line) == 0) {
            char *rest_of_message = strtok(NULL, "");
            request->body = strdup(rest_of_message);
            break;
        }

        char *key;
        char *value;
        char *header_cursor;

        char *key_pointer = strtok_r(line, ":", &header_cursor);
        if (key_pointer == NULL) return 400;
        key = strdup(key_pointer);

        char *value_pointer = strtok_r(NULL, "", &header_cursor);
        if (value_pointer == NULL) return 400;
        value = strdup(value_pointer);

        add_header(&header_list, key, value);

        line = strtok_r(NULL, "\r\n", &line_cursor);
    }

    char *body_pointer = strtok_r(NULL, "", &line_cursor);
    if (body_pointer == NULL) {
        request->body = NULL;
    } else {
        request->body = strdup(body_pointer);
    }
    
    request->header_list = header_list;

    return 0;
}

/**
 * Turn a http response struct into a string that can be returned. Returns a string pointer that
 * will need to be freed at some point
 * 
 * The content length header will be automatically appended to the list of headers
 */
char *http_response_to_string(HttpResponse *response)
{
    // Check errors
    if (response->status_code < 100 || response->status_code > 599) return NULL;
    if (response->reason_phrase == NULL) return NULL;
    
    // Initialize string
    char *result = (char*)malloc(MAX_MESSAGE_SIZE + 1);
    result[0] = '\0';

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

    return result;
}

int send_response(int client_fd, HttpResponse *response)
{
    char *response_str = http_response_to_string(response);

    if (response_str == NULL) {
        return -1;
    }

    int send_status = send(client_fd, response_str, strlen(response_str), 0);
    int return_status = send_status < (int)strlen(response_str) ? -1 : 0;

    free(response_str);

    return return_status;
}
