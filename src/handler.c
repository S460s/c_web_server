#include "../include/handler.h"
#include <stdlib.h>

struct handler *create_handler(char *path, char *method,
                               int (*handle_route)(struct http_req *, int),
                               int exact_length) {

  struct handler *handler = malloc(sizeof(struct handler));
  handler->path = path;
  handler->method = method;
  handler->exact_length = exact_length;
  handler->handle_route = handle_route;
  handler->next = NULL;
  return handler;
}
