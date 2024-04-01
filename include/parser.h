#ifndef PARSER_SEEN
#define PARSER_SEEN

struct header_list {
  char *type;
  char *value;
  struct header_list *next_header;
}; // A linked list for the headers

struct http_req {
  char *method;
  char *path;
  char *version;
  struct header_list *headers;
}; // make appropriate functions to create and destroy this struct

void free_http_req(struct http_req *req);
struct header_list *parse_header(char *header_data);
struct http_req *parse_req(char *data);

#endif // !PARSER_SEEN
