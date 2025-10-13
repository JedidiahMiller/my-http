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

        HttpResponse response = { 0 };

        response.header_list = create_header_list();
        response.status_code = 200;
        response.reason_phrase = "OK";
        response.body = "Hello world!\n";

        if (send_response(request.client_fd, &response) == -1) {
            printf("Failed to send response\n");
            free_http_request(&request);
            kill_server();
        } else {
            printf("Sent response\n");
        }

        free_http_request(&request);

        close(request.client_fd);
    }
}
