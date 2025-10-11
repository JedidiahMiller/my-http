#include <sys/socket.h>     // socket(), bind(), listen(), accept()
#include <netinet/in.h>     // struct sockaddr_in, htons(), htonl()
#include <arpa/inet.h>      // inet_addr() (optional)
#include <unistd.h>         // close()
#include <string.h>         // memset() (optional)
#include <stdio.h>          // perror(), printf() (optional)
#include <stdlib.h>         // exit() or NULL
#include <stdbool.h>
#include <signal.h>

#include "http.h"

int server_fd;
const int PORT = 1235;
const int MAX_MESSAGE_SIZE = 2048;

void kill_server()
{
    if (close(server_fd) == -1) {
        printf("Server shut down. Attempt to close socket failed.\n");
    } else {
        printf("Server shut down. Socket closed.\n");
    };
    exit(0);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    signal(SIGINT, kill_server);
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Could not attach to port %d\n", PORT);
        kill_server();
    }
    listen(server_fd, 10);

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);

        char buffer[MAX_MESSAGE_SIZE + 1]; // Adding 1 for string termination

        int message_length = recv(client_fd, buffer, MAX_MESSAGE_SIZE + 1, 0);

        if (message_length < 0) {
            printf("Failed to read message from client\n");
            kill_server();
        }
        if (message_length > MAX_MESSAGE_SIZE) {
            printf("Client's message was too big\n");
            kill_server();
        }

        buffer[message_length] = '\0';

        /**
         * Process raw request
         */

        HttpRequest request = { 0 };

        if (parse_http_request(buffer, &request) == -1) {
            printf("Failed to parse request\n");
            kill_server();
        }

        /**
         * Respond
         */

        char *response = "HTTP/1.1 200 ALRIGHT_ALRIGHT_ALRIGHT\r\n\r\n";
        if(send(client_fd, response, strlen(response), 0) == -1) {
            printf("Failed to respond to client\n");
            kill_server();
        }
        printf("Sent response\n");

        close(client_fd);
    }
}
