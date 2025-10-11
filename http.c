#include "http.h"

#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h> 


HttpHeaderList create_header_list() 
{
    HttpHeaderList list = { 0 };
    list.length = 0;
    list.head = NULL;

    return list;
}

int add_header(HttpHeaderList *list, HttpHeader header) 
{
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

/**
 * Parses a string message into a HttpRequest. Returns -1 on failure.
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
               return -1;
            }
        } else {
            if (strlen(line) == 0) {
                break;
            }

            HttpHeader header = { 0 };
            header.name = (char*)malloc(MAX_LINE_LENGTH);
            header.value = (char*)malloc(MAX_LINE_LENGTH);

            if (sscanf(line, " %[^:]: %[^\n]", header.name, header.value) != 2) {
                return -1;
            }

            add_header(&header_list, header);
        }

        // strtok has internal state. Null makes it go to the next token
        line = strtok(NULL, "\r\n");
        line_number++;
    }

    request->header_list = header_list;

    return 0;
}
