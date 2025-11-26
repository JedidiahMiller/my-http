#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>

#include "http.h"

HttpServer server;

void kill_server()
{
    if (free_server(&server)) {
        printf("Server shut down. Attempt to close socket failed.\n");
    } else {
        printf("Server shut down. Socket closed.\n");
    };
    exit(0);
}


int main() {
    if (initalize_server(&server) != 0) {
            printf("Could not launch server\n");
            kill_server();
        }

    signal(SIGINT, kill_server);

    while (true) {
        HttpRequest request;
        if (server_accept(&server, &request) != 0) {
            printf("Could not process request\n");
            kill_server();
        }

        printf("Path: %s\n", request.target);

        // Initialize response
        HttpResponse response = { 0 };

        response.header_list = create_header_list();
        response.status_code = 200;
        response.reason_phrase = "OK";
        response.body = NULL;

        // Get the requested file
        if (strcmp(request.target, "/") == 0) {
            free(request.target);
            request.target = strdup("/index.html");
        }
        char *path = (char*)malloc(strlen(request.target) + strlen("./public") + 1);
        sprintf(path, "./public%s", request.target);

        FILE *file = fopen(path, "r");
        free(path);
        if (!file) {
            response.status_code = 404;
            response.reason_phrase = "File not found";
        } else {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            rewind(file);
    
            char *buf = malloc(size + 1);
            fread(buf, 1, size, file);
            buf[size] = '\0';

            response.body = buf;
        }

        if (send_response(request.client_fd, &response) == -1) {
            printf("Failed to send response\n");
            free_http_request(&request);
            free(response.body);
            kill_server();
        }
        
        printf("Sent response\n");

        free(response.body);
        free_http_request(&request);

        close(request.client_fd);
    }
}
