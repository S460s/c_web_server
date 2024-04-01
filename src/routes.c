#include "../include/routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int handle_root(struct http_req *req, int fd) {
  char *response = malloc(sizeof(char) * 200);
  sprintf(response, "%s\r\n", HTTP_OK);
  unsigned int expected_bytes = strlen(HTTP_OK);
  unsigned int bytes_sent = send(fd, response, strlen(response), 0);
  free(response);

  return expected_bytes - bytes_sent;
}

// TODO: res len
int handle_echo(struct http_req *req, int fd) {
  char *response = malloc(sizeof(char) * 200);
  char *path_text = req->path + 6; // +6 to remove the "/echo/" part
  sprintf(response,
          "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
          HTTP_OK, strlen(path_text), path_text);

  int expected_bytes = strlen(response);
  printf("[INFO] RESPONSE: %s\n", response);

  int bytes_sent = send(fd, response, strlen(response), 0);
  free(response);

  return expected_bytes - bytes_sent;
}

// TODO: res len
int handle_useragent(struct http_req *req, int fd) {
  char *response = malloc(sizeof(char) * 300);

  char *user_agent = NULL;
  struct header_list *current = req->headers;
  while (current) {
    if (strcmp("User-Agent", current->type) == 0) {
      user_agent = current->value;
      break;
    }
    current = current->next_header;
  }

  sprintf(response,
          "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
          HTTP_OK, strlen(user_agent), user_agent);

  int expected_bytes = strlen(response);
  printf("[INFO] RESPONSE: %s\n", response);

  int bytes_sent = send(fd, response, strlen(response), 0);
  free(response);

  return expected_bytes - bytes_sent;
}

struct handler **get_routes(int *count) {
  struct handler *routes[] = {
      create_handler("/echo", "GET", handle_echo, 0),
      create_handler("/user-agent", "GET", handle_useragent, 1),
      create_handler("/", "GET", handle_root, 1)};

  const int route_count = sizeof(routes) / sizeof(routes[0]);
  const int route_size = sizeof(struct handler *) * route_count;

  struct handler **heap_routes = malloc(route_size);
  memcpy(heap_routes, routes, route_size);

  *count = route_count;
  return heap_routes;
}

void free_routes(struct handler *handlers[], int size) {
  for (int i = 0; i < size; i++) {
    struct handler *current = handlers[i];
    free(current->path);
    free(current->next);
    free(current);
  }
  free(handlers);
}
