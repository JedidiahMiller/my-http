#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

int server_fd;
struct sockaddr_in server_address;

int main()
{
    // TODO: Sockets need to be closed on shutdown signals
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("An error occured opening the socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        printf("Failed to bind to address");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, SOMAXCONN) == -1) {
        printf("Failed to start listening");
        exit(EXIT_FAILURE);
    }

    printf("The server is listening locally on port %d\n", PORT);

    while (true) {
        struct sockaddr_storage client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &addrlen);

        char buffer[MAX_REQUEST_SIZE];
        size_t buffer_offset = 0;
        size_t read_offset = 0;

        struct HttpRequest request;
        request.headers = NULL;
        request.state = LOOKING_FOR_METHOD;

        while (true) {

            ssize_t incoming_size =
                read(client_fd, &buffer[buffer_offset], MAX_REQUEST_SIZE - buffer_offset);

            if (incoming_size < 0) {
                exit(EXIT_FAILURE);
            }

            if (incoming_size == 0) {
                break;
            }

            buffer_offset += incoming_size;

            // TODO: This kind of needs to loop to go through each step
            // TODO: Be checking for valid characters

            bool completed_one_state;

            do {
                completed_one_state = false;

                switch (request.state) {
                case LOOKING_FOR_METHOD:

                    if (buffer_offset - read_offset >= 4) {
                        if (strncmp(buffer + read_offset, "GET ", 4) == 0) {
                            request.method = GET;

                            read_offset += 4;

                            request.state = LOOKING_FOR_URI;
                            completed_one_state = true;
                            break;
                        }
                    }

                    if (buffer_offset - read_offset >= 5) {
                        if (strncmp(buffer + read_offset, "POST ", 5) == 0) {
                            request.method = POST;
                            read_offset += 5;

                            request.state = LOOKING_FOR_URI;
                            completed_one_state = true;
                            break;
                        }
                    }

                    // There is enough data that it should have matched, but did not
                    if (buffer_offset - read_offset > 5) {
                        exit(EXIT_FAILURE);
                    }

                    break;

                case LOOKING_FOR_URI:

                    for (size_t i = 0; i < buffer_offset - read_offset; i++) {
                        if (buffer[read_offset + i] == ' ') {

                            if (i > MAX_URI_LENGTH - 1) exit(EXIT_FAILURE);

                            memcpy(request.uri, buffer + read_offset, i);
                            request.uri[i] = '\0';
                            read_offset += i + 1;

                            request.state = LOOKING_FOR_VERSION;
                            completed_one_state = true;
                            break;
                        }
                    }

                    if (buffer_offset - read_offset > MAX_URI_LENGTH) {
                        exit(EXIT_FAILURE);
                    }

                    break;

                case LOOKING_FOR_VERSION:

                    ;
                    char *version_str = "HTTP/1.1\r\n"; // ';' to allow variable

                    if (buffer_offset - read_offset >= strlen(version_str)) {
                        if (strncmp(buffer + read_offset, version_str, strlen(version_str)) == 0) {
                            read_offset += strlen(version_str);

                            request.state = LOOKING_FOR_HEADER_KEY;
                            completed_one_state = true;
                            break;
                        }
                        exit(EXIT_FAILURE);
                    }

                    break;

                case LOOKING_FOR_HEADER_KEY:

                    // Check for end of headers
                    if (buffer_offset - read_offset >= 2 &&
                        strncmp(buffer + read_offset, "\r\n", 2) == 0) {
                        read_offset += 2;

                        request.state = LOOKING_FOR_DATA;
                        completed_one_state = true;
                        break;
                    }

                    for (size_t i = 0; i < buffer_offset - read_offset; i++) {
                        if (buffer[read_offset + i] == ':') {

                            if (i > MAX_HEADER_KEY_LENGTH - 1) exit(EXIT_FAILURE);

                            // Create a new header
                            struct HttpHeader *header =
                                (struct HttpHeader *) malloc(sizeof(struct HttpHeader));
                            header->next = request.headers;
                            request.headers = header;

                            memcpy(header->key, buffer + read_offset, i + 1);
                            header->key[i] = '\0';
                            read_offset += i + 1;

                            request.state = LOOKING_FOR_HEADER_VALUE;
                            completed_one_state = true;
                            break;
                        }
                    }

                    if (buffer_offset - read_offset > MAX_HEADER_KEY_LENGTH) {
                        exit(EXIT_FAILURE);
                    }

                    break;
                case LOOKING_FOR_HEADER_VALUE:

                    if (buffer_offset - read_offset > 2) {

                        char *end_pointer =
                            memmem(buffer + read_offset, buffer_offset - read_offset, "\r\n", 2);

                        if (end_pointer != NULL) {

                            int end_offset = end_pointer - (buffer + read_offset);

                            memcpy(request.headers->value, buffer + read_offset, end_offset + 1);
                            request.headers->value[end_offset] = '\0';

                            read_offset += end_offset + 2;

                            request.state = LOOKING_FOR_HEADER_KEY;
                            completed_one_state = true;
                            break;
                        }
                    }

                    if (buffer_offset - read_offset > MAX_HEADER_VALUE_LENGTH) {
                        exit(EXIT_FAILURE);
                    }

                    break;
                case LOOKING_FOR_DATA:

                    // This may or may not exist. It is not important for a static file server
                    request.state = DONE_READING_REQUEST;

                    break;

                default:
                    break;
                }
            } while (completed_one_state);

            if (request.state == DONE_READING_REQUEST) {
                break;
            }
        }

        // Response

        char response[MAX_REQUEST_SIZE];
        char *response_pointer = response;

        char *basepath = "./public";
        char filepath[MAX_URI_LENGTH + strlen(basepath) + 1];
        snprintf(filepath, MAX_URI_LENGTH + strlen(basepath) + 1, "%s%s", basepath, request.uri);

        FILE *file = fopen(filepath, "rb"); // TODO: This URI needs to be checked for safety
        if (!file) {
            printf("Could not open file %s\n", filepath);
            exit(EXIT_FAILURE);
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        rewind(file);

        response_pointer +=
            snprintf(response_pointer, MAX_REQUEST_SIZE - (response_pointer - response),
                     "HTTP/1.1 200 OK\r\n");

        response_pointer +=
            snprintf(response_pointer, MAX_REQUEST_SIZE - (response_pointer - response),
                     "Content-Type: text/html\r\n" // THIS NEEDS TO HANDLE MORE FILE TYPES
            );
        response_pointer +=
            snprintf(response_pointer, MAX_REQUEST_SIZE - (response_pointer - response),
                     "Content-Length: %ld\r\n", file_size);
        response_pointer +=
            snprintf(response_pointer, MAX_REQUEST_SIZE - (response_pointer - response),
                     "Connection: close\r\n");

        response_pointer +=
            snprintf(response_pointer, MAX_REQUEST_SIZE - (response_pointer - response), "\r\n");

        response_pointer += fread(response_pointer, 1, file_size, file);
        fclose(file);

        ssize_t data_remaining = response_pointer - response;
        while (data_remaining > 0) {
            data_remaining -= write(client_fd, response, data_remaining);
        }

        close(client_fd);

        printf("A client was served %s\n", filepath);
    }

    return 0;
}
