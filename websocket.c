#include <stddef.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include "websocket.h"
#include "http.h"



/**
 * Process HTTP upgrade request
 * 
 * @return Client fd
 */
int process_http_upgrade_request(HttpRequest *request)
{
    char *upgrade_header = get_header(&request->header_list, "Upgrade");
    if (upgrade_header == NULL) {
        return -1; // No client response, upgrade was a false alarm
    }


    // Do the whole Sec-WebSocket-Accept song and dance
    char *sec_websocket_key = get_header(&request->header_list, "Sec-WebSocket-Key");
    if (sec_websocket_key == NULL) {
        HttpResponse response = new_400_response("Missing Sec-Websocket-Accept");
        send_response(request->client_fd, &response);
        free_http_request(&request);
        return -1;
    }

    // Concat the magic string
    char *ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char sec_string[MAX_LINE_LENGTH];
    sec_string[0] = '\0';
    strcpy(sec_string, sec_websocket_key);
    strcat(sec_string, ws_magic_string);

    // SHA1
    char sha_result[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)sec_string, strlen(sec_string), (unsigned char*)sha_result);

    // Base64
    char sec_accept_response[64];
    EVP_EncodeBlock((unsigned char*)sec_accept_response, (unsigned char*)sha_result, 20);

    // Prep and send handshake response
    HttpResponse response = { 0 };

    response.header_list = create_header_list();
    add_header(&response.header_list, "Upgrade", "websocket");
    add_header(&response.header_list, "Connection", "Upgrade");
    add_header(&response.header_list, "Sec-WebSocket-Accept", (char*)sec_accept_response);

    response.status_code = 101;
    response.reason_phrase = "Switching Protocols";

    if (send_response(request->client_fd, &response) == -1) {
        free_http_request(&request);
        return -1;
    }

    int client_fd = request->client_fd;
    free_http_request(&request);

    return client_fd;
}



/**
 * Sends a single packet
 * 
 * @param data Pointer to the blob of data
 * @param size Size of the data blob
 * @param client_fd The connection's file descriptor
 */
int send_packet(void *payload, size_t size, bool is_final, int client_fd)
{
    if (payload == NULL || size > MAX_WEBSOCKET_PAYLOAD_SIZE) {
        return -1;
    }


    uint8_t opcode = 0;


    // Byte 1
    uint8_t byte1 = 0;
    byte1 = is_final ? 0b10000000 : 0;
    // Ignore the rsv stuff
    byte1 |= 0xF0 & opcode;


    // Length and mask bytes
    uint8_t len_and_mask[1 + 8];
    size_t number_of_len_and_mask_bytes;

    if (size > 0xFFFF) {
        len_and_mask[0] = 127;
        *(uint64_t*)(len_and_mask+1) = size;
        number_of_len_and_mask_bytes = 9;
    } else if (size > 125) {
        len_and_mask[0] = 126;
        *(uint16_t*)(len_and_mask+1) = size;
        number_of_len_and_mask_bytes = 3;
    } else {
        len_and_mask[0] = size;
        number_of_len_and_mask_bytes = 1;
    }


    // Payload is already ready


    // Package everything 

    // Prepare space (first byte + length and mask bytes + payload size)
    size_t raw_binary_size = 1 + number_of_len_and_mask_bytes + size;
    uint8_t *raw_binary = (uint8_t*)malloc(raw_binary_size);

    raw_binary[0] = byte1;
    for (int i = 0; i < number_of_len_and_mask_bytes; i++) {
        raw_binary[1+i] = len_and_mask[i];
    }
    for (int i = 0; i < size; i++) {
        raw_binary[1+i] = ((uint8_t*)payload)[i];
    }

    // Sent it
    if (send(client_fd, raw_binary, size, 0) != raw_binary_size) {
        free(raw_binary);
        return -1;
    }

    free(raw_binary);
    return 0;
}



/**
 * Takes the client_fd and reads an awaiting packet
 */
int read_packet(int client_fd, size_t size, Packet *packet)
{
    // Process first byte
    unsigned char byte1; 
    if (recv(client_fd, &byte1, 1, 0) != 1) return -1;

    packet->is_final = 0b10000000 & byte1; // 0
    bool rsv1 = 0b01000000 & byte1;       // 1
    bool rsv2 = 0b00100000 & byte1;       // 2
    bool rsv3 = 0b00010000 & byte1;       // 3
    packet->opcode = 0x0F & byte1;         // 4, 5, 6, 7

    // Process second byte
    unsigned char byte2; 
    if (recv(client_fd, &byte2, 1, 0) != 1) return -1;

    packet->mask = 0b10000000 & byte2;

    // Read length
    packet->length = 0b01111111 & byte2;
    if (packet->length == 126) {
        uint16_t length;
        if (recv(client_fd, &length, 2, 0) != 2) return -1;
        packet->length = ntohs(length);
    } else if (packet->length == 127) {
        uint64_t length;
        if (recv(client_fd, &length, 8, 0) != 8) return -1;
        packet->length = ntohll(length);
    }
    if (packet->length > MAX_WEBSOCKET_PAYLOAD_SIZE) {
        return -1;
    }

    // Process masking key
    if (packet->mask) {
        if (recv(client_fd, &packet->masking_key, 4, 0) != 4) return -1;
    }

    // Payload
    packet->payload = malloc(packet->length);
    if (recv(client_fd, packet->payload, packet->length, 0) != packet->length) return -1;

    // Unmask payload
    if (packet->mask) {
        for(size_t i = 0; i < packet->length; i++) {
            packet->payload[i] ^= ((uint8_t*)packet->masking_key)[i % 4];
        }
    }

    return 0;
}
