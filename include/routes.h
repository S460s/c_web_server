#ifndef ROUTES_SEEN
#define ROUTES_SEEN

#include "./handler.h"
#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 Not Found\r\n\n"

struct handler **get_routes(int *count);
void free_routes(struct handler *handlers[], int size);

#endif // !DEBUG
