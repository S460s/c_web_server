#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/parser.h"
#include "../include/routes.h"
#include "../include/util.h"

const short BUF_SIZE = 500;

#include <argp.h>
const char *argp_program_version = "v1.0.0";
const char *argp_prgram_bug_address = "/dev/null";

static char args_doc[] = "none";
static char doc[] = "a toy http server written in C";

static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"directory", 'd', "FILE", 0,
     "Path to the directory which to use as a starting point of the server"},
    {0}};

struct arguments {
  char *directory;
  int verbose;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {

  case 'v':
    arguments->verbose = 1;
    break;
  case 'd':
    arguments->directory = arg;
    break;
  case ARGP_KEY_ARG:
    if (state->arg_num > 0) {
      printf("[ERROR] too many arguments\n");
      argp_usage(state);
    }
    break;
  case ARGP_KEY_END:
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}
static struct argp argp = {options, parse_opt, args_doc, doc};

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

  struct arguments arguments = {0};
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  printf("[INFO] directory: %s\n", arguments.directory);

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
    fprintf(stderr, "[ERROR] listen failed: %s \n", strerror(errno));
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

  printf("[INFO] waiting for a client to connect...\n");

  struct sockaddr_in client_addr;
  unsigned int client_addr_len = sizeof(client_addr);

  // accept loop
  char client_str[INET6_ADDRSTRLEN];
  int router_count = 0;
  struct handler **routes = get_routes(&router_count);

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

      for (int i = 0; i < router_count; i++) {
        struct handler *handler = routes[i];
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

  free_routes(routes, router_count);
  printf("[INFO] Shutting down...\n");
  return 0;
}
