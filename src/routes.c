#include "../include/routes.h"
#include <argp.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// TODO: put this in other file later on
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

struct arguments arguments = {0};

int handle_dir(struct http_req *req, int fd) {
  struct dirent *dirent;
  DIR *file_dir = opendir(arguments.directory);

  // make it dynamic with the size of the contents of the directory
  char response[800];
  char text[700];

  while ((dirent = readdir(file_dir)) != NULL) {
    printf("DIR: %s\n", dirent->d_name);
    sprintf(text + strlen(response), "%s", dirent->d_name);
  }

  sprintf(response,
          "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
          HTTP_OK, strlen(text), text);
  unsigned int expected_bytes = strlen(HTTP_OK);
  unsigned int bytes_sent = send(fd, response, strlen(response), 0);

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

struct handler **add_route(struct handler **routes, struct handler *route,
                           int *count) {
  struct handler **new_routes = realloc(routes, sizeof(routes) + 1);

  new_routes[*count] = route;
  (*count)++;
  return new_routes;
}

struct handler **create_routes(int *count, int argc, char **argv) {
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  printf("[INFO] directory: %s\n", arguments.directory);

  struct handler *routes[] = {
      create_handler("/echo", "GET", handle_echo, 0),
      create_handler("/user-agent", "GET", handle_useragent, 1),
      create_handler("/", "GET", handle_root, 1)};

  int route_count = sizeof(routes) / sizeof(routes[0]);
  const int route_size = sizeof(struct handler *) * route_count;

  struct handler **heap_routes = malloc(route_size);
  memcpy(heap_routes, routes, route_size);

  if (arguments.directory != NULL) {
    add_route(heap_routes, create_handler("/files", "GET", handle_dir, 1),
              &route_count);
  }

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
