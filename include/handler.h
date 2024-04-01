#ifndef HANDLER_SEEN
#define HANDLER_SEEN
#include "./parser.h"

struct handler {
  char *path;
  char *method;
  short exact_length;
  int (*handle_route)(struct http_req *, int);
  struct handler *next;
};
struct handler *create_handler(char *path, char *method,
                               int (*handle_route)(struct http_req *, int),
                               int exact_length);

#endif
