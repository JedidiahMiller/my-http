
#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdbool.h>
#include <stdint.h>

// 1 Gb
#define MAX_WEBSOCKET_PAYLOAD_SIZE 1000000000

typedef struct Packet {
    bool is_final;
    size_t length;
    uint8_t opcode;
    bool mask;
    uint32_t masking_key;
    uint8_t *payload;
} Packet;

int send_packet(void *payload, size_t size, bool is_final, int client_fd);
int read_packet(int client_fd, size_t size, Packet *packet);

#endif
