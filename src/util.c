#include "../include/util.h"
#include <stdlib.h>
#include <string.h>

char *heap_copy(const char *str) {
  const unsigned long length = strlen(str);
  char *heap_str = malloc(sizeof(char) * length + 1);

  strncpy(heap_str, str, length + 1);
  return heap_str;
}
