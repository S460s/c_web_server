#include "../include/routes.h"
#include <argp.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

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

void get_paths(char *path, char *buffer, int TEXT_LENGTH) {
  const int DIRTYPE = 4;
  const int FILETYPE = 8;

  struct dirent *dirent;
  DIR *file_dir = opendir(path);

  while ((dirent = readdir(file_dir)) != NULL) {
    if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
      continue;

    printf("- %s/%s\n", path, dirent->d_name);
    char add_on[100] = "";
    snprintf(add_on, 100, "%s/%s\n", path, dirent->d_name);
    strncat(buffer, add_on, TEXT_LENGTH - 1);

    if (dirent->d_type == DIRTYPE) {
      char new_path[256];
      snprintf(new_path, 256, "%s/%s", path, dirent->d_name);
      get_paths(new_path, buffer, TEXT_LENGTH);
    }
  }
}

int handle_dir(struct http_req *req, int fd) {
#define RES_LENGTH 1400
#define TEXT_LENGTH 1000

  struct dirent *dirent;
  DIR *file_dir = opendir(arguments.directory);

  // make it dynamic with the size of the contents of the directory
  char response[RES_LENGTH] = {0};
  char text[TEXT_LENGTH] = {0};

  get_paths(arguments.directory, text, TEXT_LENGTH);

  snprintf(response, 800,
           "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
           HTTP_OK, strlen(text), text);
  unsigned int expected_bytes = strlen(HTTP_OK);
  unsigned int bytes_sent = send(fd, response, strlen(response), 0);

  return expected_bytes - bytes_sent;
}
/* This routine reads the entire file into memory. */

static char *read_whole_file(FILE *fp) {
  char *contents;
  size_t bytes_read;
  int status;

  fseek(fp, 0, SEEK_END);
  unsigned int s = ftell(fp);
  rewind(fp);

  contents = malloc(s + 1);
  if (!contents) {
    fprintf(stderr, "Not enough memory.\n");
    exit(EXIT_FAILURE);
  }

  bytes_read = fread(contents, sizeof(unsigned char), s, fp);
  if (bytes_read != s) {
    fprintf(stderr,
            "Short read: expected %d bytes "
            "but got %ld: %s.\n",
            s, bytes_read, strerror(errno));
    exit(EXIT_FAILURE);
  }
  status = fclose(fp);
  if (status != 0) {
    fprintf(stderr, "Error closing %s.\n", strerror(errno));
  }
  return contents;
}

int handle_download_file(struct http_req *req, int fd) {
#define RES_LENGTH 1400
  char response[RES_LENGTH];

  char *uri_path = req->path + strlen("/files/");
  char path[256];
  snprintf(path, 256, "%s/%s", arguments.directory, uri_path);

  FILE *file = fopen(path, "r");
  if (file == NULL) {
    char answer[256];
    snprintf(answer, 256,
             "No such file: <%s>, hit \"/files\" to see all files\n", path);

    snprintf(response, RES_LENGTH,
             "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
             HTTP_OK, strlen(answer), answer);

    int expected_bytes = strlen(response);
    int bytes_sent = send(fd, response, strlen(response), 0);
    return expected_bytes - bytes_sent;
  }

  char *content = read_whole_file(file);

  sprintf(response,
          "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\r\n",
          HTTP_OK, strlen(content), content);

  free(content);
  int expected_bytes = strlen(response);
  printf("[INFO] RESPONSE: %s\n", response);

  int bytes_sent = send(fd, response, strlen(response), 0);

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

struct handler **create_routes(int *count, int argc, char **argv) {
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  printf("[INFO] directory: %s\n", arguments.directory);

  int route_count = 0;
  // TODO: Make this dynamic later on if needed
  struct handler *routes[32];
  routes[route_count++] = create_handler("/echo", "GET", handle_echo, 0);
  routes[route_count++] =
      create_handler("/user-agent", "GET", handle_useragent, 1);
  routes[route_count++] = create_handler("/", "GET", handle_root, 1);

  if (arguments.directory != NULL) {
    routes[route_count++] = create_handler("/files", "GET", handle_dir, 1);
    routes[route_count++] =
        create_handler("/files", "GET", handle_download_file, 0);
  }

  const int route_size = sizeof(struct handler *) * route_count;
  printf("route count: %d\n", route_count);
  *count = route_count;
  struct handler **heap_routes = malloc(route_size);
  memcpy(heap_routes, routes, route_size);
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
