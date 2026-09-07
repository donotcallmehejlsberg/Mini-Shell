#include <stdlib.h>

#include "input.h"
#include "shell.h"

int main(void) {
  char *buffer = allocateBuffer();
  if (buffer == NULL) {
    return EXIT_FAILURE;
  }
  runShell(buffer);

  free(buffer);
  buffer = NULL;
  return EXIT_SUCCESS;
}
