#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const short BUF_SIZE = 500;

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

// make a copy of a string to the heap
// +1 as strncpy will add a null character if there is more space than needed
char *heap_copy(const char *str) {
  const unsigned long length = strlen(str);
  char *heap_str = malloc(sizeof(char) * length + 1);

  strncpy(heap_str, str, length + 1);
  return heap_str;
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

// Since the tester restarts your program quite often, setting REUSE_PORT
// ensures that we don't run into 'Address already in use' errors
int free_port(int socket_fd) {
  int reuse = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) <
      0) {
    return 1;
  }
  return 0;
}

// getaddrinfo returs a linked list of address structures and we search for a
// valid one to bind to
int get_server_fd(struct addrinfo *info) {
  struct addrinfo *next_addr;
  int server_fd;
  for (next_addr = info; next_addr != NULL; next_addr = next_addr->ai_next) {
    server_fd = socket(next_addr->ai_family, next_addr->ai_socktype,
                       next_addr->ai_protocol);
    if (server_fd == -1)
      continue;

    if (free_port(server_fd) != 0) {
      fprintf(stderr, "SO_REUSEPORT failed: %s \n", strerror(errno));
      // exit(EXIT_FAILURE);
      continue;
    };

    if (bind(server_fd, next_addr->ai_addr, next_addr->ai_addrlen) == 0)
      break;
  }

  if (next_addr == NULL) {
    fprintf(stderr, "Couldn't bind\n");
    exit(EXIT_FAILURE);
  }

  return server_fd;
}

// ROUTING TODO: put into separate file later
const char *HTTP_OK = "HTTP/1.1 200 OK\r\n";
const char *HTTP_NOT_FOUND = "HTTP/1.1 404 Not Found\r\n\n";

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

struct handler {
  char *path;
  char *method;
  short exact_length;
  int (*handle_route)(struct http_req *, int);
  struct handler *next;
};

void free_handalers(struct handler *handlers[], int size) {
  for (int i = 0; i < size; i++) {
    struct handler *current = handlers[i];
    free(current->path);
    free(current->next);
    free(current);
  }
  free(handlers);
}

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

// Multi threaded

void sigchld_handler(int s) {
  int save_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = save_errno;
}

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  printf("[INFO] server %s started\n", argv[0]);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); // set all unspecified options to 0
  hints.ai_family = AF_UNSPEC;      // allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // tcp
  hints.ai_flags = AI_PASSIVE;      // bind to the IP of this host

  struct addrinfo *result;
  int s;
  if ((s = getaddrinfo(NULL, "4221", &hints, &result)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  int server_fd = get_server_fd(result);
  freeaddrinfo(result); // no longer needed

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  // kill dead processes
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    fprintf(stderr, "[Error] Sigacation");
  }

  printf("Waiting for a client to connect...\n");

  struct sockaddr_in client_addr;
  unsigned int client_addr_len = sizeof(client_addr);

  // accept loop
  char client_str[INET6_ADDRSTRLEN];
  struct handler *handlers[] = {
      create_handler("/echo", "GET", handle_echo, 0),
      create_handler("/user-agent", "GET", handle_useragent, 1),
      create_handler("/", "GET", handle_root, 1)};

  const int handler_count = sizeof(handlers) / sizeof(handlers[0]);

  while (1) {
    int new_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (new_fd == -1) {
      fprintf(stderr, "accept failed\n");
      continue;
    }

    inet_ntop(client_addr.sin_family, &client_addr.sin_addr, client_str,
              sizeof(client_str));

    printf("[INFO] connection from: %s\n", client_str);

    if (!fork()) { // child
      close(server_fd);

      const int req_data_sz = 500;
      char req_data[req_data_sz];
      int bytes_recvd = recv(new_fd, req_data, req_data_sz, 0);

      struct http_req *req = parse_req(req_data);
      struct header_list *current = req->headers;

      int not_handled = 1;
      int error = 0;

      for (int i = 0; i < handler_count; i++) {
        struct handler *handler = handlers[i];
        int equal_paths =
            strncmp(req->path, handler->path, strlen(handler->path)) == 0;
        int equal_methods =
            strncmp(req->method, handler->method, strlen(handler->method)) == 0;

        short diff_length = 0;
        if (handler->exact_length)
          diff_length = strlen(handler->path) - strlen(req->path);

        if (equal_paths && equal_methods && !diff_length) {
          int error = handler->handle_route(req, new_fd);
          not_handled = 0;
          break;
        }
      }

      if (not_handled) {
        printf("[INFO] NOT FOUND: %s\n", req->path);
        int expected_bytes = strlen(HTTP_NOT_FOUND);
        int bytes_sent =
            send(new_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
        error = expected_bytes - bytes_sent;
      }

      if (error != 0) {
        fprintf(stderr, "[ERROR] Error while sending data to client\n");
      }

      close(new_fd);
      exit(0);
    }

    close(new_fd);
  }

  free_handalers(handlers, handler_count);
  printf("[INFO] Shutting down...\n");
  return 0;
}
