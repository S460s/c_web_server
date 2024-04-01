#include "../include/parser.h"
#include "../include/util.h"

#include <stdlib.h>
#include <string.h>

void free_http_req(struct http_req *req) {
  free(req->method);
  free(req->path);
  free(req->version);
  struct header_list *current = req->headers;
  while (current != NULL) {
    free(current->type);
    free(current->value);
    struct header_list *next = current->next_header;
    free(current);
    current = next;
  }
  free(req);
}

struct header_list *parse_header(char *header_data) {
  struct header_list *header = malloc(sizeof(struct header_list));

  char *ptr = NULL;
  char *delim = ":";

  char *type = strtok_r(header_data, delim, &ptr);

  char *value = strtok_r(NULL, delim, &ptr);
  if (value[0] == ' ') // HeaderType: HeaderValue -> whitespace inbetween
    value = value + 1;

  header->type = heap_copy(type);
  header->value = heap_copy(value);
  header->next_header = NULL;

  return header;
}

struct http_req *parse_req(char *data) {
  struct http_req *req = malloc(sizeof(struct http_req));

  char *end_of_headers = strstr(data, "\r\n\r\n");
  end_of_headers[4] = '\0'; // separate body from headers

  char *line_delim = "\r\n";
  char *saveptr_line = NULL;
  char *status_line = strtok_r(data, line_delim, &saveptr_line);
  unsigned int status_line_length = strlen(status_line);

  char *delim = " ";
  char *saveptr = NULL;

  char *method = strtok_r(status_line, delim, &saveptr);
  req->method = heap_copy(method);

  char *path = strtok_r(NULL, delim, &saveptr);
  req->path = heap_copy(path);

  char *http_version = strtok_r(NULL, delim, &saveptr);
  req->version = heap_copy(http_version);

  char *headers = status_line + status_line_length +
                  2; // skip null char and newline char (+2)

  char *header_ptr = NULL;

  char *header_data = strtok_r(headers, line_delim, &header_ptr);
  struct header_list *header = NULL;
  struct header_list *last_header = NULL;

  if (header_data != NULL) {
    header = parse_header(header_data);

    last_header = header;
    req->headers = last_header;
  }

  while ((header_data = strtok_r(NULL, line_delim, &header_ptr)) != NULL &&
         strlen(header_data) > 3) {
    struct header_list *next_header = parse_header(header_data);
    last_header->next_header = next_header;
    last_header = next_header;
  }

  return req;
}
